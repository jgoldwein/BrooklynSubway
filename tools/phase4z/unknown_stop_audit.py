#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Audit aggregate UnknownStop observations against static GTFS and map data."""

import argparse
import csv
import hashlib
import io
import json
import math
import sys
import zipfile
from collections import Counter, defaultdict
from contextlib import contextmanager
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NEAR_M = 150.0
COUNT_FIELDS = ("route", "stop_id", "observations_in_10_cycles")
CSV_FIELDS = (
    "route",
    "platform_stop",
    "parent_stop",
    "static_station_name",
    "observations",
    "static_gtfs_routes",
    "observed_route_static_service",
    "classification",
    "project_parent_mapping",
    "route_station_led",
    "nearest_route_led_station",
    "nearest_route_led_distance_m",
    "within_150m_of_route_led",
    "unknown_destination_observations",
    "previous_stop_sources",
    "previous_stop_ids",
    "previous_stop_mapping",
    "confidence",
    "interpretation",
)


class AuditError(ValueError):
    pass


@contextmanager
def open_gtfs_member(source, name):
    if source.is_dir():
        with (source / name).open("rb") as handle:
            yield handle
    else:
        with zipfile.ZipFile(source) as archive:
            with archive.open(name) as handle:
                yield handle


def csv_rows(source, name):
    with open_gtfs_member(source, name) as raw:
        with io.TextIOWrapper(raw, encoding="utf-8-sig", newline="") as text:
            yield from csv.DictReader(text)


def sha256_normalized_gtfs_member(source, name):
    digest = hashlib.sha256()
    with open_gtfs_member(source, name) as handle:
        previous = b""
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            normalized = (previous + chunk).replace(b"\r\n", b"\n")
            if normalized.endswith(b"\r"):
                previous = b"\r"
                normalized = normalized[:-1]
            else:
                previous = b""
            digest.update(normalized)
        if previous:
            digest.update(previous)
    return digest.hexdigest().upper()


def load_static_gtfs(source):
    stops = {row["stop_id"]: row for row in csv_rows(source, "stops.txt")}
    trip_routes = {row["trip_id"]: row["route_id"] for row in csv_rows(source, "trips.txt")}
    served = defaultdict(set)
    for row in csv_rows(source, "stop_times.txt"):
        route = trip_routes.get(row["trip_id"])
        stop = stops.get(row["stop_id"])
        if route and stop:
            parent = stop.get("parent_station", "") or row["stop_id"][:3]
            served[parent].add(route)
    feed_rows = list(csv_rows(source, "feed_info.txt"))
    feed_info = feed_rows[0] if feed_rows else {}
    hashes = {
        name: sha256_normalized_gtfs_member(source, name)
        for name in ("trips.txt", "stop_times.txt", "stops.txt")
    }
    return {
        "stops": stops,
        "served": served,
        "feed_version": feed_info.get("feed_version", ""),
        "hashes": hashes,
        "archive_sha256": (
            hashlib.sha256(source.read_bytes()).hexdigest().upper()
            if source.is_file()
            else ""
        ),
    }


def topology_hashes_match(static, topology_path):
    recorded = {}
    for line in topology_path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("#"):
            continue
        key, separator, value = line[1:].strip().partition("=")
        if separator and key.startswith("sha256_"):
            recorded[key[7:]] = value
    return all(recorded.get(name) == digest for name, digest in static["hashes"].items())


def load_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def load_replay(path):
    if path is None:
        return None
    replay = json.loads(path.read_text(encoding="utf-8"))
    if replay.get("schema") != "brooklyn-gtfsrt-replay" or replay.get("schemaVersion") != 1:
        raise AuditError("unsupported replay export schema")
    result = defaultdict(lambda: {
        "count": 0,
        "unknown_destination": 0,
        "sources": Counter(),
        "previous_stops": set(),
        "next_stops": set(),
    })
    for frame in replay.get("frames", []):
        for item in frame.get("observations", []):
            if item.get("category") != "UnknownStop":
                continue
            route, stop = item.get("route", ""), item.get("currentStop", "")
            if not route or not stop:
                raise AuditError("replay UnknownStop observation lacks route/currentStop")
            evidence = result[(route, stop)]
            evidence["count"] += 1
            failure = item.get("previousStopFailure", "")
            if failure == "UnknownDestination":
                evidence["unknown_destination"] += 1
            source = item.get("previousStopSource", "")
            evidence["sources"][source] += 1
            for field, target in (("previousStop", "previous_stops"), ("nextStop", "next_stops")):
                value = item.get(field, "")
                if value and value != "Unavailable":
                    evidence[target].add(value)
    return result


def load_map_data(stop_map_rows, led_rows):
    mapped = {row["gtfs_parent_stop_id"]: row for row in stop_map_rows}
    route_led_stations = defaultdict(set)
    station_coordinates = defaultdict(list)
    for row in led_rows:
        if row.get("role") != "map" or row.get("element_type") != "STATION":
            continue
        route_led_stations[row["route"]].add(row["station_id"])
    for row in stop_map_rows:
        station_coordinates[row["station_id"]].append(
            (float(row["lat"]), float(row["lon"]), row["registry_name"])
        )
    return mapped, route_led_stations, station_coordinates


def distance_m(lat1, lon1, lat2, lon2):
    scale = 111320.0
    dy = (lat1 - lat2) * scale
    dx = (lon1 - lon2) * scale * math.cos(math.radians(lat1))
    return math.hypot(dx, dy)


def normalized_parent(stop_id, stop):
    if stop:
        return stop.get("parent_station", "") or (stop_id[:3] if len(stop_id) == 4 else stop_id)
    return stop_id[:3] if len(stop_id) == 4 and stop_id[-1] in "NS" else stop_id


def parse_counts(rows):
    counts = {}
    for row in rows:
        if any(field not in row for field in COUNT_FIELDS):
            raise AuditError("count CSV must contain route, stop_id, observations_in_10_cycles")
        route, stop = row["route"].strip(), row["stop_id"].strip()
        if not route or not stop:
            raise AuditError("count CSV contains an empty route or stop")
        try:
            count = int(row["observations_in_10_cycles"])
        except ValueError as error:
            raise AuditError(f"invalid observation count for {route}/{stop}") from error
        if count <= 0 or (route, stop) in counts:
            raise AuditError(f"invalid or duplicate count row for {route}/{stop}")
        counts[(route, stop)] = count
    return counts


def audit_pairs(count_rows, static, stop_map_rows, led_rows, replay_evidence=None):
    counts = parse_counts(count_rows)
    mapped, route_led_stations, station_coordinates = load_map_data(stop_map_rows, led_rows)
    if replay_evidence is not None:
        observed = {key: item["count"] for key, item in replay_evidence.items()}
        if observed != counts:
            raise AuditError("replay UnknownStop counts do not match the aggregate counts CSV")

    results = []
    for (route, stop_id), count in sorted(counts.items()):
        stop_record = static["stops"].get(stop_id)
        if stop_record is None:
            raise AuditError(f"observed stop {stop_id} is absent from static GTFS")
        parent_id = normalized_parent(stop_id, stop_record)
        parent_record = static["stops"].get(parent_id, stop_record or {})
        static_routes = sorted(static["served"].get(parent_id, set()))
        map_record = mapped.get(parent_id)
        route_led = bool(
            map_record
            and map_record["station_id"] in route_led_stations.get(route, set())
        )
        parent_lat = float(parent_record.get("stop_lat") or (map_record or {}).get("lat") or 0)
        parent_lon = float(parent_record.get("stop_lon") or (map_record or {}).get("lon") or 0)
        route_candidates = []
        for station_id, locations in station_coordinates.items():
            for lat, lon, name in locations:
                candidate = (distance_m(parent_lat, parent_lon, lat, lon), name, station_id)
                if station_id in route_led_stations.get(route, set()):
                    route_candidates.append(candidate)
        nearest_route = min(route_candidates) if route_candidates else None
        near_route = bool(nearest_route and nearest_route[0] <= NEAR_M)

        evidence = (replay_evidence or {}).get((route, stop_id), {})
        previous_ids = sorted(evidence.get("previous_stops", set()))
        previous_unmapped = [
            previous for previous in previous_ids
            if normalized_parent(previous, static["stops"].get(previous)) not in mapped
        ]
        unknown_destination = evidence.get("unknown_destination", 0)
        if route not in static_routes:
            classification = "STATIC_ROUTE_SERVICE_MISMATCH"
            confidence = "High for discrepancy; cause unresolved"
            interpretation = (
                "The observed route is absent from static GTFS service at this stop. "
                "A live diversion or feed inconsistency cannot be distinguished here."
            )
        elif map_record and route_led and previous_unmapped:
            classification = "MAP_BOUNDARY_SEGMENT_PREVIOUS_STOP_OFF_MAP"
            confidence = "High"
            interpretation = (
                "The current station has a route-specific map LED, but the recorded "
                "previous stop has no project stop-map entry; the segment crosses "
                "the map boundary."
            )
        elif map_record and route_led and unknown_destination:
            classification = "MAPPED_STOP_TOPOLOGY_LOOKUP_GAP"
            confidence = "Medium"
            interpretation = (
                "The current stop is mapped, while previous-stop topology reports "
                "UnknownDestination; inspect topology evidence before any change."
            )
        elif map_record and route_led:
            classification = "MAPPED_STOP_UNKNOWN_ADAPTER_CAUSE"
            confidence = "Medium"
            interpretation = (
                "The current stop and route-specific LED exist in project data; "
                "the replay export does not identify the adapter operand causing "
                "UnknownStop."
            )
        elif near_route:
            classification = "UNMAPPED_STOP_NEAR_ROUTE_LED"
            confidence = "Low"
            interpretation = (
                "The stop is within the audit's 150 m route-LED proximity threshold; "
                "proximity alone is not evidence for a semantic mapping."
            )
        else:
            classification = "OUTSIDE_ROUTE_MAP_SCOPE"
            confidence = "High for scope; no mapping inference"
            interpretation = (
                "Static GTFS serves this route at a stop with no exact project "
                "parent mapping and no route-specific map LED within 150 m."
            )
            if unknown_destination:
                interpretation += (
                    " Some observations also have UnknownDestination because the "
                    "compiled topology is limited to on-map destinations."
                )

        sources = evidence.get("sources", Counter())
        source_text = ";".join(
            f"{name or 'Unavailable'}:{amount}" for name, amount in sorted(sources.items())
        )
        results.append({
            "route": route,
            "platform_stop": stop_id,
            "parent_stop": parent_id,
            "static_station_name": parent_record.get("stop_name", ""),
            "observations": count,
            "static_gtfs_routes": "/".join(static_routes),
            "observed_route_static_service": "yes" if route in static_routes else "no",
            "classification": classification,
            "project_parent_mapping": (
                f'{map_record["station_id"]}:{map_record["registry_name"]}'
                if map_record else "not mapped"
            ),
            "route_station_led": "yes" if route_led else "no",
            "nearest_route_led_station": nearest_route[1] if nearest_route else "",
            "nearest_route_led_distance_m": (
                str(round(nearest_route[0])) if nearest_route else ""
            ),
            "within_150m_of_route_led": "yes" if near_route else "no",
            "unknown_destination_observations": unknown_destination,
            "previous_stop_sources": source_text,
            "previous_stop_ids": "/".join(previous_ids),
            "previous_stop_mapping": (
                "off-map:" + "/".join(previous_unmapped)
                if previous_unmapped
                else ("mapped" if previous_ids else "not reported")
            ),
            "confidence": confidence,
            "interpretation": interpretation,
        })
    return results


def write_audit_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def route_totals(rows):
    totals = defaultdict(lambda: [0, 0, Counter()])
    for row in rows:
        value = totals[row["route"]]
        value[0] += 1
        value[1] += int(row["observations"])
        value[2][row["classification"]] += int(row["observations"])
    return totals


def render_report(rows, static, count_path, replay_path, csv_path, replay_evidence=None):
    observations = sum(int(row["observations"]) for row in rows)
    by_class = Counter()
    by_direction = Counter()
    by_parent = defaultdict(int)
    by_parent_direction = defaultdict(Counter)
    unknown_destination = 0
    for row in rows:
        count = int(row["observations"])
        by_class[row["classification"]] += count
        by_direction[row["platform_stop"][-1]] += count
        by_parent[row["parent_stop"]] += count
        by_parent_direction[row["parent_stop"]][row["platform_stop"][-1]] += count
        unknown_destination += int(row["unknown_destination_observations"])
    mismatch_observations = by_class["STATIC_ROUTE_SERVICE_MISMATCH"]
    mismatch_pairs = [
        f"{row['route']}/{row['platform_stop']} (static {row['static_gtfs_routes']})"
        for row in rows
        if row["classification"] == "STATIC_ROUTE_SERVICE_MISMATCH"
    ]
    d21_next = (
        replay_evidence.get(("D", "A21S"), {}).get("next_stops", set())
        if replay_evidence is not None else set()
    )
    d22_next = (
        replay_evidence.get(("D", "A22S"), {}).get("next_stops", set())
        if replay_evidence is not None else set()
    )
    if "A22S" in d21_next and "A24S" in d22_next:
        d_context = (
            "The replay records A21S with next stop A22S and later records "
            "A24S as a next stop while at A22S; this is consistent with a live "
            "path different from static route service."
        )
    elif replay_evidence is None:
        d_context = "No replay next-stop context was supplied to this audit run."
    else:
        d_context = "The supplied replay does not contain the expected D-route next-stop sequence."

    out = [
        "# Phase 4Z — UnknownStop evidence audit",
        "",
        "## Scope and decision boundary",
        "",
        "This is a host-only, first-pass explanation of the Phase 4Y live "
        "UnknownStop observations. It makes no firmware, semantic, route-alias, "
        "GTFS mapping, hardware, or release changes. Classifications describe "
        "evidence and are not mapping-change recommendations. The H/H19 "
        "realtime-only operational-stop contract remains separate and unchanged. "
        "The Phase 4Y capture separately recorded 100 `OperationalStop` "
        "observations at H19N; this stop remains deliberately unmapped and has no LED.",
        "",
        f"Input aggregates: `{count_path.name}` ({len(rows)} route/platform pairs, "
        f"{observations} observations).",
        f"Static source: MTA NYCT GTFS `{static['feed_version']}`.",
        f"Capture replay input: `{replay_path.name if replay_path else 'not supplied'}`.",
        f"Derived detail: [`{csv_path.name}`](./{csv_path.name}); it contains "
        "only aggregate stop/service/map evidence, not raw feed payloads or trip IDs.",
        "",
        "The geographic scope check uses the project's authoritative "
        "`data/gtfs_stop_map.csv` and Phase 3B semantic worksheet. A route-specific "
        "map stop is considered nearby only within 150 m, matching the existing "
        "Phase 4E audit threshold. Nearest candidates are evidence only; no "
        "station or route assignment is inferred from proximity.",
        "",
        "## Findings",
        "",
        f"- **{by_class['OUTSIDE_ROUTE_MAP_SCOPE']} observations** "
        f"({sum(r['classification'] == 'OUTSIDE_ROUTE_MAP_SCOPE' for r in rows)} "
        "pairs) are static-served route positions outside route-specific map "
        "coverage.",
        f"- **{by_class['MAP_BOUNDARY_SEGMENT_PREVIOUS_STOP_OFF_MAP']} observations** "
        "are at the mapped route-2 Clark St parent 231, with previous stop 230 "
        "outside the project stop map; this is a map-boundary segment, not a "
        "missing Clark St station LED.",
        f"- **{mismatch_observations} observations** occur at "
        f"{sum(r['classification'] == 'STATIC_ROUTE_SERVICE_MISMATCH' for r in rows)} "
        "pairs where the observed route is absent from static GTFS service: "
        f"{'; '.join(mismatch_pairs)}. "
        f"{d_context} The cause (for example, a diversion or feed inconsistency) "
        "remains unresolved.",
        (
            f"- **{unknown_destination} observations** across "
            f"{sum(int(r['unknown_destination_observations']) > 0 for r in rows)} "
            "pairs report `UnknownDestination` from the embedded topology lookup. "
            "The topology table intentionally contains on-map destinations only; "
            "these observations are consistent with off-map destinations."
            if replay_evidence is not None
            else "- Replay event context was not supplied, so topology-failure "
            "counts are unavailable."
        ),
        f"- All {len(rows)} observed platform IDs have static GTFS stop records. "
        f"{observations - mismatch_observations} observations match static route "
        "service. No route/stop mapping correction is justified by this sample.",
        "",
        (
            f"The replay export groups {observations - unknown_destination} "
            f"UnknownStop observations without an `UnknownDestination` "
            f"previous-stop failure and {unknown_destination} with that failure. "
            "The no-failure group is not sufficient to infer whether a train "
            "was stopped or which adapter endpoint failed; the replay schema "
            "does not retain `current_status`. The Clark St pair is identifiable "
            "from its mapped current parent and reported previous stop 230."
            if replay_evidence is not None
            else "The replay schema does not retain `current_status`; an "
            "aggregate-only run cannot distinguish stopped observations from "
            "which adapter endpoint failed."
        ),
        "",
        "The raw capture and replay JSON remain local under Git-ignored "
        "`captures/`; they were not copied into the audit outputs.",
        "",
        "## Static GTFS provenance",
        "",
        f"- Feed version: `{static['feed_version']}`.",
        f"- Archive SHA-256: `{static['archive_sha256'] or 'directory source; no archive hash'}`.",
        f"- `stops.txt` normalized SHA-256: `{static['hashes']['stops.txt']}`.",
        f"- `trips.txt` normalized SHA-256: `{static['hashes']['trips.txt']}`.",
        f"- `stop_times.txt` normalized SHA-256: `{static['hashes']['stop_times.txt']}`.",
        (
            "- All three normalized member hashes match the source hashes "
            "recorded in `data/gtfs_topology.csv`, confirming the static feed "
            "matches the compiled topology's source snapshot."
            if static["topology_hash_match"]
            else "- The normalized GTFS member hashes do not all match "
            "`data/gtfs_topology.csv`; treat topology comparisons as unverified."
        ),
        "",
        "## Aggregate distribution",
        "",
        "| Route | Pairs | Observations | Topology UnknownDestination |",
        "|---|---:|---:|---:|",
    ]
    for route, (pairs, count, classes) in sorted(route_totals(rows).items()):
        dest_count = sum(
            int(row["unknown_destination_observations"])
            for row in rows if row["route"] == route
        )
        out.append(f"| {route} | {pairs} | {count} | {dest_count} |")
    out += [
        "",
        f"By platform suffix: N = {by_direction['N']} observations; "
        f"S = {by_direction['S']} observations.",
        "",
        "Most frequent parent stops (all directions combined):",
        "",
        "| Parent | Station | N | S | Total |",
        "|---|---|---:|---:|---:|",
    ]
    parent_names = {}
    for row in rows:
        parent_names[row["parent_stop"]] = row["static_station_name"]
    for parent, count in sorted(by_parent.items(), key=lambda item: (-item[1], item[0]))[:25]:
        directions = by_parent_direction[parent]
        out.append(
            f"| {parent} | {parent_names[parent]} | {directions['N']} | "
            f"{directions['S']} | {count} |"
        )
    out += [
        "",
        "## Highest-frequency route/platform pairs",
        "",
        "| Route | Platform | Static station | Observations | Classification | Nearest route LED | Distance (m) |",
        "|---|---|---|---:|---|---|---:|",
    ]
    for row in sorted(rows, key=lambda item: (-int(item["observations"]), item["route"], item["platform_stop"]))[:25]:
        out.append(
            f"| {row['route']} | {row['platform_stop']} | {row['static_station_name']} | "
            f"{row['observations']} | {row['classification']} | "
            f"{row['nearest_route_led_station']} | {row['nearest_route_led_distance_m']} |"
        )
    out += [
        "",
        "## Interpretation and follow-up",
        "",
        "The 149 route-served, unmapped pairs have no route-specific map LED "
        "within 150 m. They are outside the Brooklyn map's displayed service "
        "scope, rather than evidence that 149 station assignments are missing. "
        "The single mapped Clark St pair reflects a segment crossing the map "
        "edge because Wall St parent 230 is not mapped. The two D-route pairs "
        "should remain an operational-feed investigation item; the capture "
        "does not authorize route aliases or GTFS changes.",
        "",
        "Next recommended work is a dedicated review of the D/A21S and D/A22S "
        "live/static route discrepancy and, separately, a later capture that "
        "includes real on-map segment observations. Preserve the raw capture "
        "under ignored `captures/`; do not add it or live replay payloads to Git.",
        "",
        "Reproduce the focused tests with `python tools\\run_phase4z_tests.py`. "
        "Regenerate the local audit from the same ignored inputs with:",
        "",
        "```powershell",
        "python tools\\phase4z\\unknown_stop_audit.py "
        "--gtfs captures\\phase4z_static\\gtfs_subway.zip "
        "--replay captures\\20261019T062411Z-464a7da9\\replay.json "
        "--csv docs\\PHASE4Z_UNKNOWN_STOP_AUDIT.csv "
        "--report docs\\PHASE4Z_UNKNOWN_STOP_AUDIT.md",
        "```",
        "",
        "No stop mappings, route aliases, topology, station semantics, segment "
        "semantics, route colors, LED assignments, firmware, or hardware were changed.",
        "",
    ]
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gtfs", type=Path, required=True, help="official static GTFS directory or ZIP")
    parser.add_argument("--counts", type=Path, default=ROOT / "docs/PHASE4Y_UNKNOWN_STOP_COUNTS.csv")
    parser.add_argument("--replay", type=Path, help="optional local Phase 4Y replay JSON (not copied to output)")
    parser.add_argument("--csv", type=Path, required=True, help="write aggregate audit CSV")
    parser.add_argument("--report", type=Path, help="write Markdown report")
    args = parser.parse_args(argv)
    try:
        static = load_static_gtfs(args.gtfs)
        static["topology_hash_match"] = topology_hashes_match(
            static, ROOT / "data/gtfs_topology.csv"
        )
        replay = load_replay(args.replay)
        rows = audit_pairs(
            load_csv(args.counts),
            static,
            load_csv(ROOT / "data/gtfs_stop_map.csv"),
            load_csv(ROOT / "docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv"),
            replay,
        )
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        write_audit_csv(args.csv, rows)
        if args.report:
            report = render_report(
                rows, static, args.counts, args.replay, args.csv, replay
            )
            args.report.parent.mkdir(parents=True, exist_ok=True)
            with args.report.open("w", encoding="utf-8", newline="\n") as handle:
                handle.write(report)
        totals = Counter(row["classification"] for row in rows)
        observation_totals = Counter()
        for row in rows:
            observation_totals[row["classification"]] += int(row["observations"])
        print(
            f"Audited {len(rows)} pairs / "
            f"{sum(int(row['observations']) for row in rows)} observations."
        )
        for classification in sorted(totals):
            print(
                f"{classification}: {totals[classification]} pairs, "
                f"{observation_totals[classification]} observations"
            )
        print(f"GTFS feed version: {static['feed_version']}")
        print(f"CSV: {args.csv}")
        if args.report:
            print(f"Report: {args.report}")
        return 0
    except (AuditError, OSError, KeyError, ValueError, zipfile.BadZipFile, json.JSONDecodeError) as error:
        parser.exit(1, f"audit failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
