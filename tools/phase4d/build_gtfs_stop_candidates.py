# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""One-off Phase 4D research tool (not part of the firmware build).

Cross-references the frozen Phase 3B station registry against the official MTA
static GTFS feed (stops.txt, trips.txt, stop_times.txt) and proposes a
supplemental registry-station -> GTFS-parent-stop mapping.

A registry entity is matched to a GTFS parent station only when ALL of these
hold:
  * the normalized station names are identical,
  * the GTFS parent station is served by at least one route that the map
    shows at that registry entity (route/corridor context), and
  * the pairing is unambiguous (otherwise it goes to the manual-review file).

When several GTFS parents share a name, a registry note that explicitly calls the
entity a station complex accepts every name+route match (route-specific parents of
one complex). Otherwise exactly one candidate must be served only by routes the map
shows at that entity. Anything else is left for manual review.

Usage:
  python tools/phase4d/build_gtfs_stop_candidates.py --gtfs DIR --out DIR
"""

import argparse
import csv
import sys
import unicodedata
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import generate_semantic_tables as gen  # noqa: E402

# GTFS route_id -> semantic route label used by the map.
GTFS_TO_MAP_ROUTE = {
    "2": "2", "3": "3", "4": "4", "5": "5", "A": "A", "B": "B", "C": "C",
    "D": "D", "F": "F", "G": "G", "J": "J", "L": "L", "M": "M", "N": "N",
    "Q": "Q", "R": "R", "Z": "Z", "FS": "S", "H": "S",
}


def norm(name):
    text = unicodedata.normalize("NFKC", name).lower()
    for dash in ("\u2013", "\u2014", "\u2212"):
        text = text.replace(dash, "-")
    text = text.replace(" - ", "-").replace("  ", " ")
    return text.strip()


def read(path):
    with open(path, newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def load_gtfs(directory):
    stops = {r["stop_id"]: r for r in read(directory / "stops.txt")}
    parents = {s: r for s, r in stops.items() if r["location_type"] == "1"}
    child_parent = {s: r["parent_station"] for s, r in stops.items()
                    if r["parent_station"]}
    trip_route = {r["trip_id"]: r["route_id"] for r in read(directory / "trips.txt")}
    served = defaultdict(set)
    with open(directory / "stop_times.txt", newline="", encoding="utf-8-sig") as handle:
        for row in csv.DictReader(handle):
            parent = child_parent.get(row["stop_id"], row["stop_id"])
            route = GTFS_TO_MAP_ROUTE.get(trip_route.get(row["trip_id"], ""))
            if route:
                served[parent].add(route)
    return parents, served


def station_routes(records):
    routes = defaultdict(set)
    for rec in records:
        if rec["role"] == "Station":
            routes[rec["station"]].add(rec["route"])
        else:
            routes[rec["a"]].add(rec["route"])
            routes[rec["b"]].add(rec["route"])
    return routes


ALIASES = {  # registry display name -> GTFS parent stop name (abbreviation only)
    "aqueduct-north conduit av": "aqueduct-n conduit av",
}
FRANKLIN_S = (87, "S01")  # name differs ("Franklin Av-Fulton St" vs "Franklin Av")


def decide(registry, routes, parents, served):
    by_name = defaultdict(list)
    for stop_id, row in parents.items():
        by_name[norm(row["stop_name"])].append(stop_id)

    accepted, review = [], []
    for sid in sorted(registry):
        reg = registry[sid]
        have = reg["gtfs_parent_stop_id"].strip()
        wanted = routes[sid]
        name = norm(reg["display_name"])
        if have:
            ok = (have in parents and norm(parents[have]["stop_name"]) == name
                  and bool(served[have] & wanted))
            (accepted if ok else review).append((sid, [have], "registry-verified"
                                                  if ok else "registry-mismatch"))
            continue
        gname = ALIASES.get(name, name)
        cands = [c for c in by_name.get(gname, []) if served[c] & wanted]
        if (sid, "S01") == FRANKLIN_S and "S01" in parents and served["S01"] & wanted:
            accepted.append((sid, ["S01"], "name-token+route (Franklin Av shuttle)"))
        elif not cands:
            review.append((sid, [], "no name+route match"))
        elif len(cands) == 1:
            accepted.append((sid, cands, "name+route"))
        elif "complex" in reg["notes"].lower() or "shared" in reg["notes"].lower():
            accepted.append((sid, cands, "name+route; registry note says complex"))
        else:
            subset = [c for c in cands if served[c] <= wanted]
            if len(subset) == 1:
                accepted.append((sid, subset, "name+route; only candidate whose "
                                 "routes are all shown at this entity"))
            else:
                review.append((sid, cands, "ambiguous candidates"))
    return accepted, review


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gtfs", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    registry, records = gen.build()
    routes = station_routes(records)
    parents, served = load_gtfs(args.gtfs)
    accepted, review = decide(registry, routes, parents, served)

    args.out.mkdir(parents=True, exist_ok=True)
    with open(args.out / "gtfs_stop_map.csv", "w", newline="", encoding="utf-8") as h:
        w = csv.writer(h, lineterminator="\n")
        w.writerow(["gtfs_parent_stop_id", "station_id", "registry_name", "gtfs_stop_name",
                    "gtfs_routes", "registry_routes", "lat", "lon", "evidence"])
        rows = []
        for sid, stops, ev in accepted:
            for stop in stops:
                p = parents[stop]
                rows.append([stop, sid, registry[sid]["display_name"], p["stop_name"],
                             "/".join(sorted(served[stop])),
                             "/".join(sorted(routes[sid])), p["stop_lat"], p["stop_lon"], ev])
        for row in sorted(rows):
            w.writerow(row)
    with open(args.out / "gtfs_stop_review.csv", "w", newline="", encoding="utf-8") as h:
        w = csv.writer(h, lineterminator="\n")
        w.writerow(["station_id", "registry_name", "corridor_or_line", "registry_routes",
                    "reason", "candidate_stop_ids"])
        for sid, stops, reason in review:
            reg = registry[sid]
            w.writerow([sid, reg["display_name"], reg["corridor_or_line"],
                        "/".join(sorted(routes[sid])), reason, "/".join(stops)])
    print(f"accepted entities={len(accepted)} stop rows={sum(len(s) for _, s, _ in accepted)} "
          f"review={len(review)}")


if __name__ == "__main__":
    main()