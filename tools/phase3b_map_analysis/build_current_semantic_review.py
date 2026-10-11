#!/usr/bin/env python3
"""Build the Phase 3B current semantic review without promoting semantics."""

from __future__ import annotations

import csv
import re
from collections import Counter, defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PHASE3B = PROJECT_ROOT / "docs" / "mapping" / "phase3b"
ANALYSIS = PHASE3B / "analysis"

OUTPUT_FIELDS = (
    "designator",
    "channel",
    "channel_index",
    "global_physical_order",
    "pcb_x",
    "pcb_y",
    "pixel_x",
    "pixel_y",
    "route",
    "element_type",
    "station_name",
    "station_id",
    "station_key",
    "from_station",
    "to_station",
    "from_station_id",
    "to_station_id",
    "from_station_key",
    "to_station_key",
    "confidence",
    "review_status",
    "current_visual_evidence",
    "legacy_evidence",
    "legacy_relationship",
    "ambiguity_note",
)

NEW_CLEAR_CANDIDATES = {
    "LED9": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Broadway",
        "station_key": "g_line_broadway",
        "confidence": "0.97",
    },
    "LED11": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Flushing Av",
        "station_key": "g_line_flushing_av",
        "confidence": "0.97",
    },
    "LED12": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Myrtle-Willoughby Avs",
        "station_key": "g_line_myrtle_willoughby_avs",
        "confidence": "0.97",
    },
    "LED13": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Bedford-Nostrand Avs",
        "station_key": "g_line_bedford_nostrand_avs",
        "confidence": "0.97",
    },
    "LED14": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Classon Av",
        "station_key": "g_line_classon_av",
        "confidence": "0.97",
    },
    "LED15": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Clinton-Washington Avs",
        "station_key": "g_line_clinton_washington_avs",
        "confidence": "0.97",
    },
    "LED16": {
        "route": "G",
        "element_type": "STATION",
        "station_name": "Fulton St",
        "station_key": "g_line_fulton_st",
        "confidence": "0.97",
    },
    "LED93": {
        "route": "L",
        "element_type": "STATION",
        "station_name": "New Lots Av",
        "station_key": "canarsie_line_new_lots_av",
        "confidence": "0.97",
    },
}

AMBIGUITY_CLUSTERS = (
    (
        "Northern Brooklyn / Culver transition",
        {"LED168", "LED169", "LED172", "LED173"},
    ),
    ("Prospect Park–7 Av corridor", {"LED290"}),
    (
        "Atlantic Av-Barclays Ctr / DeKalb Av convergence",
        {"LED158", "LED159", "LED136", "LED157", "LED161", "LED191", "LED85"},
    ),
    ("Coney Island-Stillwell Av", {"LED165", "LED166", "LED167"}),
    (
        "Rockaway branches / Broad Channel",
        {"LED269", "LED270", "LED277", "LED278", "LED279", "LED280",
         "LED281", "LED282", "LED283", "LED284"},
    ),
)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def normalize(value: str) -> str:
    text = value.casefold().replace("–", "-").replace("—", "-")
    text = re.sub(r"\([^)]*\)", " ", text)
    text = re.sub(r"[^a-z0-9]+", " ", text)
    return " ".join(text.split())


def route_tokens(value: str) -> set[str]:
    return set(re.findall(r"(?<![A-Z0-9])(?:[A-Z]|[1-7])(?![A-Z0-9])", value.upper()))


def candidate_designators(value: str) -> set[str]:
    return set(re.findall(r"\bLED\d+\b", value.upper()))


def legacy_matches(
    crosswalk_rows: list[dict[str, str]],
    designator: str,
    station_name: str,
    route: str,
) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    target_station = normalize(station_name)
    target_route = route_tokens(route)
    linked = []
    station_service_only = []
    for item in crosswalk_rows:
        legacy_station = normalize(item.get("legacy_station", ""))
        legacy_route = route_tokens(item.get("legacy_route", ""))
        station_matches = bool(
            target_station
            and legacy_station
            and (
                target_station == legacy_station
                or target_station in legacy_station
                or legacy_station in target_station
            )
        )
        route_matches = bool(target_route & legacy_route)
        if station_matches and route_matches:
            station_service_only.append(item)
            if designator in candidate_designators(
                item.get("current_candidate_designator", "")
            ):
                linked.append(item)
    return linked, station_service_only


def legacy_conflicts(
    crosswalk_rows: list[dict[str, str]],
    designator: str,
    station_name: str,
    route: str,
) -> list[dict[str, str]]:
    target_station = normalize(station_name)
    target_route = route_tokens(route)
    conflicts = []
    for item in crosswalk_rows:
        if item.get("relationship") != "MOVED_OR_REPURPOSED":
            continue
        if designator not in candidate_designators(
            item.get("current_candidate_designator", "")
        ):
            continue
        old_station = normalize(item.get("legacy_station", ""))
        old_route = route_tokens(item.get("legacy_route", ""))
        station_differs = bool(
            target_station
            and old_station
            and target_station not in old_station
            and old_station not in target_station
        )
        route_differs = bool(target_route and old_route and not target_route & old_route)
        if station_differs or route_differs:
            conflicts.append(item)
    return conflicts


def unique_nonempty(values: list[str]) -> list[str]:
    return list(dict.fromkeys(value.strip() for value in values if value.strip()))


def evidence_summary(items: list[dict[str, str]], limit: int = 12) -> str:
    parts = []
    for item in items:
        source = Path(item["legacy_source"]).name
        old = item.get("legacy_designator", "").strip() or "no LED ref"
        station = item.get("legacy_station", "").strip()
        route = item.get("legacy_route", "").strip()
        parts.append(f"{source}: {old}, {station}, route {route}")
    parts = unique_nonempty(parts)
    if len(parts) > limit:
        return "; ".join(parts[:limit]) + f"; plus {len(parts) - limit} more record(s)"
    return "; ".join(parts)


def get_station_id(
    station_key: str,
    station_name: str,
    route: str,
    registry_by_key: dict[str, dict[str, str]],
) -> str:
    if station_key and station_key in registry_by_key:
        return registry_by_key[station_key]["station_id"]
    if not station_key:
        return ""
    same_name = [
        row
        for row in registry_by_key.values()
        if normalize(row["display_name"]) == normalize(station_name)
        and route in row["corridor_or_line"]
    ]
    return same_name[0]["station_id"] if len(same_name) == 1 else ""


def main() -> None:
    master_rows = read_csv(PHASE3B / "phase3b_semantic_mapping_worksheet.csv")
    physical_rows = [row for row in master_rows if row["role"] == "map"]
    master_by_designator = {row["designator"]: row for row in physical_rows}
    if len(physical_rows) != 302 or len(master_by_designator) != 302:
        raise RuntimeError("Expected 302 unique map rows in the protected worksheet")

    global_rows = read_csv(PHASE3B / "global_semantic_review.csv")
    global_by_designator = {row["designator"]: row for row in global_rows}
    projected_rows = read_csv(ANALYSIS / "projected_led_centers.csv")
    projected_by_designator = {row["designator"]: row for row in projected_rows}
    if len(projected_by_designator) != 302:
        raise RuntimeError("Expected 302 projected map LED centers")

    crosswalk_rows = read_csv(ANALYSIS / "legacy_mapping_crosswalk.csv")
    registry_rows = read_csv(PHASE3B / "station_registry.csv")
    registry_by_key = {row["station_key"]: row for row in registry_rows}
    review_rows: list[dict[str, str]] = []

    for physical in physical_rows:
        designator = physical["designator"]
        master = master_by_designator[designator]
        seed = global_by_designator.get(designator, {})
        projection = projected_by_designator[designator]
        if master["semantic_status"] == "VERIFIED":
            status = "ALREADY_VERIFIED"
            route = master["route"]
            element_type = master["element_type"]
            station_name = master["station_name"]
            station_id = master["station_id"]
            station_key = master["station_key"]
            from_station = master["from_station"]
            to_station = master["to_station"]
            from_station_id = master["from_station_id"]
            to_station_id = master["to_station_id"]
            from_station_key = master["from_station_key"]
            to_station_key = master["to_station_key"]
            confidence = master["confidence"]
            current_visual = (
                "Existing VERIFIED assignment retained unchanged from the master; "
                + master["source_note"]
            )
            ambiguity_note = ""
        else:
            candidate = dict(seed)
            if designator in NEW_CLEAR_CANDIDATES:
                candidate.update(NEW_CLEAR_CANDIDATES[designator])
                candidate["review_status"] = "PROPOSED_HIGH"
            status = candidate.get("review_status") or "UNRESOLVED"
            route = candidate.get("route", "")
            element_type = candidate.get("element_type", "")
            station_name = candidate.get("station_name", "")
            station_id = candidate.get("station_id", "")
            station_key = candidate.get("station_key", "")
            from_station = candidate.get("from_station", "")
            to_station = candidate.get("to_station", "")
            from_station_id = candidate.get("from_station_id", "")
            to_station_id = candidate.get("to_station_id", "")
            from_station_key = candidate.get("from_station_key", "")
            to_station_key = candidate.get("to_station_key", "")
            confidence = candidate.get("confidence", "0.00")
            ambiguity_note = candidate.get("ambiguity_note", "")
            conflicts = legacy_conflicts(
                crosswalk_rows, designator, station_name, route
            )
            if conflicts:
                status = "POTENTIAL_CONTRADICTION"
                conflict_summary = evidence_summary(conflicts, limit=4)
                ambiguity_note = (
                    (ambiguity_note + " " if ambiguity_note else "")
                    + "Direct legacy/current candidate conflict: "
                    + conflict_summary
                )
            if status in ("PROPOSED_HIGH", "PROPOSED_MEDIUM"):
                station_id = get_station_id(
                    station_key, station_name, route, registry_by_key
                )
                current_visual = (
                    "Original 4096x3578 EasyEDA V1.1 artwork: projected package center "
                    "coincides with the printed station marker on the identified "
                    f"{route} lane. The corridor is separated from neighboring route lanes."
                )
                if seed.get("evidence"):
                    current_visual += " Prior review evidence: " + seed["evidence"]
            elif status == "AMBIGUOUS":
                current_visual = (
                    "Full-resolution EasyEDA V1.1 artwork confirms the physical marker "
                    "location, but the nearby lanes/transfer geometry do not yet establish "
                    "a unique service or station-versus-segment role."
                )
            elif status == "POTENTIAL_CONTRADICTION":
                current_visual = (
                    "The V1.1 artwork supports the current candidate fields, but the "
                    "legacy crosswalk contains a conflicting record linked to this exact "
                    "current designator. Keep the assignment out of usable proposals "
                    "pending human reconciliation."
                )
            else:
                element_type = element_type or "UNRESOLVED"
                current_visual = (
                    "No unique route, station marker, or segment endpoints are established "
                    "for this physical LED in the current V1.1 review."
                )
                ambiguity_note = ambiguity_note or (
                    "Needs a focused current-artwork correspondence before a semantic "
                    "proposal is supportable."
                )

        linked_legacy, station_service_records = legacy_matches(
            crosswalk_rows, designator, station_name, route
        )
        conflicting_legacy = legacy_conflicts(
            crosswalk_rows, designator, station_name, route
        )
        legacy_evidence = evidence_summary(
            linked_legacy + conflicting_legacy
        )
        if not legacy_evidence and station_service_records:
            legacy_evidence = (
                "Historical station/service record(s) exist, but the crosswalk does not "
                "reconcile them to this V1.1 physical designator; not counted as "
                "independent physical support. "
                + evidence_summary(station_service_records, limit=4)
            )
        legacy_relationship = ";".join(
            unique_nonempty(
                [row.get("relationship", "") for row in linked_legacy]
                + [row.get("relationship", "") for row in conflicting_legacy]
            )
        )
        if status == "ALREADY_VERIFIED" and not legacy_evidence:
            legacy_evidence = (
                "No linked historical evidence used; current VERIFIED master semantics "
                "remain authoritative."
            )

        review_rows.append(
            {
                "designator": designator,
                "channel": physical["channel"],
                "channel_index": physical["channel_index"],
                "global_physical_order": physical["global_physical_order"],
                "pcb_x": physical["x"],
                "pcb_y": physical["y"],
                "pixel_x": projection["pixel_x"],
                "pixel_y": projection["pixel_y"],
                "route": route,
                "element_type": element_type,
                "station_name": station_name,
                "station_id": station_id,
                "station_key": station_key,
                "from_station": from_station,
                "to_station": to_station,
                "from_station_id": from_station_id,
                "to_station_id": to_station_id,
                "from_station_key": from_station_key,
                "to_station_key": to_station_key,
                "confidence": confidence,
                "review_status": status,
                "current_visual_evidence": current_visual,
                "legacy_evidence": legacy_evidence,
                "legacy_relationship": legacy_relationship,
                "ambiguity_note": ambiguity_note,
            }
        )

    status_counts = Counter(row["review_status"] for row in review_rows)
    if len(review_rows) != 302 or status_counts["ALREADY_VERIFIED"] != 51:
        raise RuntimeError(
            f"Review inventory mismatch: rows={len(review_rows)}, "
            f"already_verified={status_counts['ALREADY_VERIFIED']}"
        )
    designators = {row["designator"] for row in review_rows}
    if len(designators) != 302:
        raise RuntimeError("Duplicate designator in semantic review output")
    if any(row["review_status"] == "VERIFIED" for row in review_rows):
        raise RuntimeError("New rows must not be promoted to VERIFIED")

    for row in review_rows:
        if row["review_status"] == "PROPOSED_HIGH" and float(row["confidence"]) < 0.95:
            raise RuntimeError(f"PROPOSED_HIGH confidence below 0.95: {row['designator']}")
        if row["review_status"] == "PROPOSED_MEDIUM" and not (
            0.80 <= float(row["confidence"]) < 0.95
        ):
            raise RuntimeError(
                f"PROPOSED_MEDIUM confidence outside [0.80, 0.95): {row['designator']}"
            )

    with (ANALYSIS / "current_semantic_review.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as stream:
        writer = csv.DictWriter(stream, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(review_rows)

    proposals = [
        row
        for row in review_rows
        if row["review_status"] in ("PROPOSED_HIGH", "PROPOSED_MEDIUM")
    ]
    proposal_status_counts = Counter(row["review_status"] for row in proposals)
    evidence_counts = Counter()
    for row in proposals:
        if row["legacy_relationship"]:
            evidence_counts["both"] += 1
        else:
            evidence_counts["artwork_only"] += 1
    route_type_counts: dict[str, Counter[str]] = defaultdict(Counter)
    for row in proposals:
        route_type_counts[row["route"]][row["element_type"]] += 1
    crosswalk_relation_counts = Counter(
        row.get("relationship", "") for row in crosswalk_rows
    )
    repurposed = [
        row
        for row in crosswalk_rows
        if row.get("relationship") in ("MOVED_OR_REPURPOSED", "SUPERSEDED_SHARED_LED")
    ]
    legacy_disagreements = [
        row
        for row in crosswalk_rows
        if row.get("relationship") == "MOVED_OR_REPURPOSED"
    ]

    lines = [
        "# Phase 3B current semantic review",
        "",
        "Review-only deliverable. No semantics were promoted to VERIFIED. The 51",
        "existing VERIFIED master rows are carried as ALREADY_VERIFIED without",
        "changing their semantic fields. Registration was accepted previously and",
        "was not revisited or refit here.",
        "",
        "## Outcome",
        "",
        f"- Total map LEDs: **{len(review_rows)}**",
        f"- ALREADY_VERIFIED: **{status_counts['ALREADY_VERIFIED']}**",
        f"- PROPOSED_HIGH: **{status_counts['PROPOSED_HIGH']}**",
        f"- PROPOSED_MEDIUM: **{status_counts['PROPOSED_MEDIUM']}**",
        f"- AMBIGUOUS: **{status_counts['AMBIGUOUS']}**",
        f"- UNRESOLVED: **{status_counts['UNRESOLVED']}**",
        f"- POTENTIAL_CONTRADICTION: **{status_counts['POTENTIAL_CONTRADICTION']}**",
        f"- Newly proposed stations: **{sum(row['element_type'] == 'STATION' for row in proposals)}**",
        f"- Newly proposed segments: **{sum(row['element_type'] == 'SEGMENT' for row in proposals)}**",
        f"- Of the 251 currently unverified LEDs, **{len(proposals)}** now have a usable "
        f"proposal (**{len(proposals) / 251 * 100:.2f}%**).",
        f"- Route-family assignments among proposals: **{len({row['route'] for row in proposals if row['route']})}** "
        f"families ({', '.join(sorted({row['route'] for row in proposals if row['route']}))}); "
        f"individual service assignments: **{sum(bool(row['route']) for row in proposals)} of {len(proposals)}**.",
        f"- If all new proposals were accepted, **{51 + len(proposals)} of 302** LEDs "
        f"would be semantically classified (**{(51 + len(proposals)) / 302 * 100:.2f}%**).",
        "",
        "The additional whole-corridor proposals in this pass are limited to",
        "visually isolated G stations (Broadway through Fulton St) and L New Lots Av.",
        "Transfer/branch stations remain separate route-specific physical candidates",
        "unless the V1.1 lane is individually established.",
        "",
        "## New proposals by route and element type",
        "",
        "| Route | Stations | Segments | Other | High | Medium |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for route in sorted(route_type_counts):
        route_rows = [row for row in proposals if row["route"] == route]
        lines.append(
            f"| {route} | {route_type_counts[route]['STATION']} | "
            f"{route_type_counts[route]['SEGMENT']} | {route_type_counts[route]['OTHER']} | "
            f"{sum(row['review_status'] == 'PROPOSED_HIGH' for row in route_rows)} | "
            f"{sum(row['review_status'] == 'PROPOSED_MEDIUM' for row in route_rows)} |"
        )
    lines.extend(
        [
            "",
            "All newly proposed rows in this pass are station candidates; no new",
            "segment is asserted without clear route-specific endpoints.",
            "",
            "## Evidence independence",
            "",
            f"- Supported by both current V1.1 artwork and a reconciled legacy station/service record: "
            f"**{evidence_counts['both']}** proposed LEDs.",
            f"- Supported by current artwork only (legacy mapping not reconciled to that current LED): "
            f"**{evidence_counts['artwork_only']}** proposed LEDs.",
            f"- Crosswalk rows classed as MOVED_OR_REPURPOSED (legacy/current discrepancy): "
            f"**{len(legacy_disagreements)}**.",
            f"- Crosswalk rows classed as SUPERSEDED_SHARED_LED: "
            f"**{crosswalk_relation_counts['SUPERSEDED_SHARED_LED']}**.",
            f"- Total legacy rows with a physical-identity discrepancy or superseded shared-LED "
            f"meaning: **{len(legacy_disagreements) + crosswalk_relation_counts['SUPERSEDED_SHARED_LED']}** "
            f"({len(legacy_disagreements)} moved/repurposed; "
            f"{crosswalk_relation_counts['SUPERSEDED_SHARED_LED']} superseded shared-LED rows).",
            "",
            "A station/service string in legacy data is not counted as independent",
            "physical support unless its crosswalk row names the current V1.1 designator.",
            "Legacy designator equality alone never establishes a match.",
            "",
            "## Legacy designators changed or superseded",
            "",
        ]
    )
    if repurposed:
        lines.extend(
            [
                "| Source | Legacy LED | Legacy station / route | Current candidate(s) | Relationship | Notes |",
                "|---|---|---|---|---|---|",
            ]
        )
        for row in repurposed:
            lines.append(
                f"| {row.get('legacy_source', '')} | {row.get('legacy_designator', '')} | "
                f"{row.get('legacy_station', '')} / {row.get('legacy_route', '')} | "
                f"{row.get('current_candidate_designator', '')} | "
                f"{row.get('relationship', '')} | {row.get('notes', '')} |"
            )
    else:
        lines.append("No records were classified as MOVED_OR_REPURPOSED or SUPERSEDED_SHARED_LED.")
    lines.extend(
        [
            "",
            "## Ambiguity clusters",
            "",
        ]
    )
    review_by_designator = {row["designator"]: row for row in review_rows}
    for cluster, members in AMBIGUITY_CLUSTERS:
        present = sorted(
            (designator for designator in members if designator in review_by_designator),
            key=lambda value: int(value[3:]),
        )
        ambiguous_present = [
            designator
            for designator in present
            if review_by_designator[designator]["review_status"] in ("AMBIGUOUS", "UNRESOLVED")
        ]
        if ambiguous_present:
            lines.append(f"- **{cluster}:** {', '.join(ambiguous_present)}.")
    lines.extend(
        [
            "",
            "Other unresolved corridors are the northern and central Brooklyn",
            "multi-service/J/M/Z corridors, Downtown Brooklyn route merges, Broadway",
            "Junction/Brownsville/Canarsie approaches, Eastern Parkway/Nostrand branches,",
            "and southern 4th Avenue/West End/Sea Beach approaches. These remain",
            "UNRESOLVED where current route-lane identity or exact station-marker placement",
            "is not unambiguous.",
            "",
            "## Crosswalk and method cautions",
            "",
            f"- Legacy crosswalk rows: **{len(crosswalk_rows)}**.",
            "- The crosswalk includes old route/service mappings, station placement records,",
            "  route batches, and transfer-expansion proposals; records from different",
            "  revisions are not treated as one authoritative map.",
            "- The Newkirk Av–Little Haiti LED147 proposal conflicts with the GTFS mapping's",
            "  MISSING_LED records and remains a revision conflict, not a V1.1 assignment.",
            "- The current package center/artwork determines the V1.1 physical candidate;",
            "  old station/service data corroborates identity only when crosswalk matching",
            "  is explicit.",
            "- The 4096×3578 EasyEDA 2D Preview and accepted transform were used as the",
            "  current geometry authority. No MTA PDF, electrical ordering, or board-outline",
            "  transform was used for semantic assignment.",
            "",
            "## Validation and safeguards",
            "",
            "- All 302 map designators have one row in the CSV.",
            "- Exactly 51 current VERIFIED semantics are represented as ALREADY_VERIFIED.",
            "- No new row is marked VERIFIED.",
            "- Existing segment endpoint IDs/keys are copied directly from the protected",
            "  master worksheet.",
            "- The master semantic worksheet, station registry, firmware, and manufacturing",
            "  files were read-only inputs and were not modified.",
            "- U23 remains outside the 306 addressable LEDs; the four CH8 status LEDs are",
            "  excluded; LED291 remains an ordinary map LED.",
            "",
            "The CSV is the complete 302-row review ledger. Its `current_visual_evidence`,",
            "`legacy_evidence`, `legacy_relationship`, and `ambiguity_note` columns preserve",
            "row-level rationale.",
        ]
    )
    (ANALYSIS / "CURRENT_SEMANTIC_REVIEW.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )
    print(
        "Generated current_semantic_review.csv and CURRENT_SEMANTIC_REVIEW.md:",
        dict(status_counts),
    )


if __name__ == "__main__":
    main()
