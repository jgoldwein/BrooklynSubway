#!/usr/bin/env python3
"""Build a station-centric legacy catalog, review atlas, and semantic review."""

from __future__ import annotations

import csv
import re
import unicodedata
from collections import Counter, defaultdict
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PHASE3B = PROJECT_ROOT / "docs" / "mapping" / "phase3b"
ANALYSIS = PHASE3B / "analysis"
HISTORY = PROJECT_ROOT / "local_reference" / "historical_mapping"
ATLAS = ANALYSIS / "station_recovery_atlas"
PREVIEW = Path(
    r"C:\Users\jgold\OneDrive\Desktop\Brooklyn Subway Project"
    r"\2D_Brooklyn MTA Active Map PCB_2026-10-10.png"
)

CATALOG_FIELDS = (
    "station_name",
    "normalized_station_key",
    "historical_routes",
    "gtfs_parent_station",
    "legacy_designators",
    "legacy_coordinates",
    "legacy_mapping_mode",
    "legacy_sources",
)
CP19_FIELDS = (
    "legacy_designator",
    "station_name",
    "route",
    "legacy_coordinates",
    "current_designator",
    "current_station_name",
    "current_route",
    "current_review_status",
    "relationship",
    "notes",
)
REVIEW_FIELDS = (
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
SOURCES = (
    "Brooklyn_MTA_GTFS_LED_mapping_v1.csv",
    "Brooklyn_CP19_NEW_LED_ROUTE_MAP.csv",
    "Brooklyn_transfer_LED_expansion_plan_v1.csv",
    "Brooklyn_CP12_JZM_L_station_batch.csv",
    "Brooklyn_CP04_station_batch.csv",
    "brooklyn_station_placement_v03.csv",
)
ROUTE_RE = re.compile(r"(?<![A-Z0-9])(?:[A-Z]|[1-7])(?![A-Z0-9])")
LED_RE = re.compile(r"\bLED\d+\b", re.IGNORECASE)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def write_csv(path: Path, fields: tuple[str, ...], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def clean_text(value: str) -> str:
    return " ".join(value.replace("�", "–").strip().split())


def normalize(value: str) -> str:
    text = value.replace("�", " ").replace("–", " ").replace("—", " ").replace("-", " ")
    text = unicodedata.normalize("NFKD", text)
    text = text.encode("ascii", "ignore").decode("ascii").casefold()
    text = text.replace("&", " and ")
    return " ".join(re.sub(r"[^a-z0-9]+", " ", text).split())


def routes(value: str) -> set[str]:
    return set(ROUTE_RE.findall(value.upper()))


def station_from_marker(value: str, service: str = "") -> str:
    name = clean_text(value)
    suffix = clean_text(service)
    if suffix:
        name = re.sub(
            rf"\s+{re.escape(suffix)}\s*$", "", name, flags=re.IGNORECASE
        )
    name = re.sub(r"\s+[A-Z](?:/[A-Z])*\s*$", "", name)
    return name.strip(" -–—")


def csv_rows_for(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        raise FileNotFoundError(f"Required historical source is missing: {path}")
    return read_csv(path)


def history_records() -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    for filename in SOURCES:
        path = HISTORY / filename
        for row in csv_rows_for(path):
            source = filename
            if filename == "Brooklyn_MTA_GTFS_LED_mapping_v1.csv":
                station = station_from_marker(row["station_marker"], row["route_id"])
                route = row["route_id"]
                designator = row["physical_led"]
                parent = row["gtfs_parent_station"]
                x, y = row["easyeda_x_mm"], row["easyeda_y_mm"]
                mode = row["mapping_mode"]
            elif filename == "Brooklyn_CP19_NEW_LED_ROUTE_MAP.csv":
                station = clean_text(row["station"])
                route = row["route"]
                designator = row["physical_led"]
                parent = ""
                x, y = row["pcb_x_mm"], row["pcb_y_mm"]
                mode = "CP19_DESIGN_INTENT"
            elif filename == "Brooklyn_transfer_LED_expansion_plan_v1.csv":
                station = row["transfer_station"]
                route = row["route_id"]
                designator = row["proposed_physical_led"]
                if designator == "NEW_TBD":
                    designator = row["current_shared_led"]
                parent = row["gtfs_parent_station"]
                x, y = "", ""
                mode = "SHARED_LED_TRANSFER_EXPANSION"
            elif filename == "Brooklyn_CP12_JZM_L_station_batch.csv":
                route = row["Group"].replace("/", " ")
                station = station_from_marker(row["Station marker"], route)
                designator = row["LED"]
                parent = ""
                x, y = row["EasyEDA_X_mm"], row["EasyEDA_Y_mm"]
                mode = "CP12_ROUTE_BATCH"
            elif filename == "Brooklyn_CP04_station_batch.csv":
                marker = row["Station marker"]
                service_match = re.search(r"\s+([A-Z](?:/[A-Z])*)$", marker)
                route = service_match.group(1) if service_match else ""
                station = station_from_marker(marker, route)
                designator = row["LED"]
                parent = ""
                x, y = row["EasyEDA_X_mm"], row["EasyEDA_Y_mm"]
                mode = "CP04_ROUTE_BATCH"
            else:
                station = row["name"]
                route = row["services"]
                designator = row["ref"]
                parent = ""
                x, y = row["pcb_x_mm"], row["pcb_y_mm"]
                mode = "STATION_PLACEMENT"
            if not station:
                continue
            records.append(
                {
                    "source": source,
                    "station": clean_text(station),
                    "station_key": normalize(station),
                    "routes": " ".join(sorted(routes(route))),
                    "parent": clean_text(parent),
                    "designator": clean_text(designator),
                    "coordinates": (
                        f"{source}:{clean_text(x)},{clean_text(y)}"
                        if x or y
                        else ""
                    ),
                    "mode": clean_text(mode),
                    "raw_route": clean_text(route),
                }
            )
    return records


def catalog_entities(records: list[dict[str, str]]) -> list[dict[str, str]]:
    entities: list[dict[str, object]] = []
    for record in records:
        possible = [
            entity
            for entity in entities
            if entity["station_key"] == record["station_key"]
        ]
        parent = record["parent"]
        route_set = set(record["routes"].split())
        matching = [
            entity
            for entity in possible
            if (
                parent
                and parent in entity["parents"]
                or not parent
                and (
                    not entity["parents"]
                    or not route_set
                    or not entity["routes"]
                    or route_set & entity["routes"]
                )
            )
        ]
        if len(matching) == 1:
            entity = matching[0]
        elif not matching and len(possible) == 1 and not parent:
            entity = possible[0]
        else:
            entity = {
                "station": record["station"],
                "station_key": record["station_key"],
                "parents": set(),
                "routes": set(),
                "designators": [],
                "coordinates": [],
                "modes": [],
                "sources": [],
            }
            entities.append(entity)
        if parent:
            entity["parents"].add(parent)
        entity["routes"].update(route_set)
        for key, value in (
            ("designators", record["designator"]),
            ("coordinates", record["coordinates"]),
            ("modes", record["mode"]),
            ("sources", record["source"]),
        ):
            if value and value not in entity[key]:
                entity[key].append(value)

    result = []
    for entity in entities:
        key = entity["station_key"]
        parents = sorted(entity["parents"])
        if len([item for item in entities if item["station_key"] == key]) > 1:
            identity = f"{key}__{normalize(' '.join(parents) or '_'.join(sorted(entity['routes']))).replace(' ', '_')}"
        else:
            identity = key
        result.append(
            {
                "station_name": entity["station"],
                "normalized_station_key": identity,
                "historical_routes": "/".join(sorted(entity["routes"])),
                "gtfs_parent_station": ";".join(parents),
                "legacy_designators": ";".join(entity["designators"]),
                "legacy_coordinates": ";".join(entity["coordinates"]),
                "legacy_mapping_mode": ";".join(entity["modes"]),
                "legacy_sources": ";".join(entity["sources"]),
            }
        )
    return sorted(
        result,
        key=lambda row: (row["station_name"].casefold(), row["normalized_station_key"]),
    )


def names_match(first: str, second: str) -> bool:
    a, b = normalize(first), normalize(second)
    return bool(a and b and (a == b or a in b or b in a))


def station_route_matches(
    station_name: str, route: str, catalog: list[dict[str, str]]
) -> list[dict[str, str]]:
    route_set = routes(route)
    return [
        item
        for item in catalog
        if names_match(station_name, item["station_name"])
        and route_set & routes(item["historical_routes"])
    ]


def cp19_review(
    cp19_rows: list[dict[str, str]], review_by_led: dict[str, dict[str, str]]
) -> list[dict[str, str]]:
    result = []
    for old in cp19_rows:
        designator = old["physical_led"]
        station, route = old["station"], old["route"]
        station = clean_text(station)
        current = review_by_led.get(designator)
        same_station_service = bool(
            current
            and current["route"] == route
            and names_match(current["station_name"], station)
        )
        current_station_route = [
            row
            for row in review_by_led.values()
            if row["route"] == route and names_match(row["station_name"], station)
        ]
        current_station_any_route = [
            row
            for row in review_by_led.values()
            if names_match(row["station_name"], station)
        ]
        if same_station_service:
            relationship = "CONFIRMED_CONTINUITY"
        elif current and current["station_name"]:
            relationship = "CONFLICT"
        elif current_station_route:
            relationship = "SUPERSEDED"
        elif current_station_any_route:
            relationship = "SAME_STATION_DIFFERENT_ROUTE"
        else:
            relationship = "UNCERTAIN"
        result.append(
            {
                "legacy_designator": designator,
                "station_name": station,
                "route": route,
                "legacy_coordinates": f"{old['pcb_x_mm']},{old['pcb_y_mm']} mm",
                "current_designator": designator if current else "",
                "current_station_name": current["station_name"] if current else "",
                "current_route": current["route"] if current else "",
                "current_review_status": current["review_status"] if current else "",
                "relationship": relationship,
                "notes": (
                    "Same designator and current station/service."
                    if relationship == "CONFIRMED_CONTINUITY"
                    else "Relationship assessed from the current review ledger; "
                    "CP19 is design intent, not V1.1 truth."
                ),
            }
        )
    return result


def imread(path: Path) -> np.ndarray:
    data = np.fromfile(path, dtype=np.uint8)
    image = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if image is None:
        raise RuntimeError(f"Could not read raster image: {path}")
    return image


def imwrite(path: Path, image: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(".png", image)
    if not ok:
        raise RuntimeError(f"Could not encode atlas image: {path}")
    encoded.tofile(path)


def slug(value: str) -> str:
    text = normalize(value).replace(" ", "_")
    return text[:72] or "station"


def build_atlas(
    catalog: list[dict[str, str]],
    review: list[dict[str, str]],
    cp19: list[dict[str, str]],
    transfer_rows: list[dict[str, str]],
    raster: np.ndarray,
) -> tuple[int, set[str], dict[str, list[dict[str, str]]]]:
    review_by_led = {row["designator"]: row for row in review}
    by_station: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in review:
        if row["element_type"] == "STATION" or row["review_status"] in (
            "PROPOSED_HIGH",
            "PROPOSED_MEDIUM",
        ):
            by_station[normalize(row["station_name"])].append(row)

    history_by_station: dict[str, list[dict[str, str]]] = defaultdict(list)
    for item in catalog:
        history_by_station[normalize(item["station_name"])].append(item)

    ATLAS.mkdir(parents=True, exist_ok=True)
    for row in cp19:
        current = review_by_led.get(row["legacy_designator"])
        if current is None:
            continue
        atlas_row = dict(current)
        atlas_row["station_name"] = row["station_name"]
        atlas_row["atlas_label"] = (
            f"CP19 {row['route']} intent / {current['designator']} current "
            f"{current['route'] or '?'} {current['review_status']}"
        )
        by_station[f"cp19:{normalize(row['station_name'])}"].append(atlas_row)
    for row in transfer_rows:
        designator = row["current_shared_led"]
        current = review_by_led.get(designator)
        if current is None:
            continue
        atlas_row = dict(current)
        atlas_row["station_name"] = row["transfer_station"]
        atlas_row["atlas_label"] = (
            f"Legacy shared-pointer check: {designator}; no V1.1 identity assumed"
        )
        by_station[
            f"shared:{normalize(row['transfer_station'])}"
        ].append(atlas_row)

    image_height, image_width = raster.shape[:2]
    generated = 0
    matched = set()
    for station_key, rows in by_station.items():
        is_cp19 = station_key.startswith("cp19:")
        is_shared_locator = station_key.startswith("shared:")
        normalized_name = station_key.partition(":")[2] if ":" in station_key else station_key
        if not normalized_name or normalized_name not in history_by_station:
            continue
        name = rows[0]["station_name"]
        matched.add(normalized_name)
        valid_rows = [
            row
            for row in rows
            if row["pixel_x"] and row["pixel_y"]
        ]
        if not valid_rows:
            continue
        points = [
            (int(round(float(row["pixel_x"]))), int(round(float(row["pixel_y"]))))
            for row in valid_rows
        ]
        left = max(0, min(x for x, _ in points) - 155)
        top = max(0, min(y for _, y in points) - 115)
        right = min(image_width, max(x for x, _ in points) + 155)
        bottom = min(image_height, max(y for _, y in points) + 115)
        crop = raster[top:bottom, left:right].copy()
        header_height = 34
        canvas = np.full(
            (crop.shape[0] + header_height, crop.shape[1], 3),
            (250, 250, 250),
            dtype=np.uint8,
        )
        canvas[header_height:, :] = crop
        cv2.putText(
            canvas,
            f"{name.replace('–', '-').replace('—', '-') } - station review",
            (8, 23),
            cv2.FONT_HERSHEY_SIMPLEX,
            min(
                0.48,
                max(
                    0.20,
                    0.48
                    * canvas.shape[1]
                    / max(1, cv2.getTextSize(
                        f"{name.replace('–', '-').replace('—', '-') } - station review",
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.48,
                        1,
                    )[0][0]),
                ),
            ),
            (15, 15, 15),
            1,
            cv2.LINE_AA,
        )
        nearby_rows = [
            row
            for row in review
            if row["pixel_x"]
            and row["pixel_y"]
            and left <= float(row["pixel_x"]) <= right
            and top <= float(row["pixel_y"]) <= bottom
        ]
        target_by_designator = {
            row["designator"]: row for row in valid_rows
        }
        for row in nearby_rows:
            x = int(round(float(row["pixel_x"])))
            y = int(round(float(row["pixel_y"])))
            local = (x - left, y - top + header_height)
            status = row["review_status"]
            if status == "ALREADY_VERIFIED":
                color = (0, 160, 0)
            elif status in ("PROPOSED_HIGH", "PROPOSED_MEDIUM"):
                color = (0, 80, 240)
            else:
                color = (170, 80, 0)
            matched_station = any(
                item["designator"] == row["designator"] for item in valid_rows
            )
            cv2.circle(canvas, local, 13 if matched_station else 8, color, 2, cv2.LINE_AA)
            target = target_by_designator.get(row["designator"])
            label = (
                target.get("atlas_label", "")
                if target
                else f"{row['designator']} {row['route'] or '?'} {status}"
            )
            label_x = min(max(local[0] + 12, 3), max(3, canvas.shape[1] - 180))
            label_y = min(max(local[1] - 8, header_height + 12), canvas.shape[0] - 3)
            cv2.putText(
                canvas,
                label,
                (label_x, label_y),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.32,
                color,
                1,
                cv2.LINE_AA,
            )
        canvas = cv2.resize(
            canvas, None, fx=2.0, fy=2.0, interpolation=cv2.INTER_CUBIC
        )
        suffix = "_cp19_intent" if is_cp19 else ""
        if is_shared_locator:
            suffix = "_legacy_shared_locator"
        output_name = f"{slug(name)}{suffix}.png"
        output = ATLAS / output_name
        imwrite(output, canvas)
        generated += 1
    return generated, matched, by_station


def main() -> None:
    records = history_records()
    catalog = catalog_entities(records)
    write_csv(ANALYSIS / "legacy_station_service_catalog.csv", CATALOG_FIELDS, catalog)

    review_path = ANALYSIS / "current_semantic_review.csv"
    review = read_csv(review_path)
    review_by_led = {row["designator"]: row for row in review}
    master_by_led = {
        row["designator"]: row
        for row in read_csv(PHASE3B / "phase3b_semantic_mapping_worksheet.csv")
        if row["role"] == "map"
    }
    cp19_rows = csv_rows_for(HISTORY / "Brooklyn_CP19_NEW_LED_ROUTE_MAP.csv")
    transfer_rows = csv_rows_for(
        HISTORY / "Brooklyn_transfer_LED_expansion_plan_v1.csv"
    )
    cp19 = cp19_review(cp19_rows, review_by_led)
    write_csv(ANALYSIS / "cp19_continuity_review.csv", CP19_FIELDS, cp19)

    single_records = [
        row
        for row in records
        if row["source"] == "Brooklyn_MTA_GTFS_LED_mapping_v1.csv"
        and row["mode"] == "single_route_LED"
    ]
    shared_records = [
        row
        for row in records
        if row["source"] == "Brooklyn_MTA_GTFS_LED_mapping_v1.csv"
        and row["mode"] == "shared_LED"
    ]
    single_entity_keys = {
        (row["station_key"], row["routes"])
        for row in single_records
    }
    shared_entities = {
        row["station_key"]
        for row in shared_records
    }

    single_route_statuses: Counter[str] = Counter()
    recovered_high = set()
    recovered_medium = set()
    station_matches: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in review:
        if row["review_status"] not in (
            "ALREADY_VERIFIED",
            "PROPOSED_HIGH",
            "PROPOSED_MEDIUM",
            "AMBIGUOUS",
        ):
            continue
        matched = station_route_matches(row["station_name"], row["route"], catalog)
        if matched:
            station_matches[normalize(row["station_name"])].extend(matched)
        single_support = [
            record
            for record in single_records
            if names_match(row["station_name"], record["station"])
            and routes(row["route"]) & routes(record["routes"])
        ]
        if single_support and row["review_status"] != "ALREADY_VERIFIED":
            if row["review_status"] == "PROPOSED_HIGH":
                single_route_statuses["PROPOSED_HIGH"] += 1
                recovered_high.add(row["designator"])
            elif row["review_status"] == "PROPOSED_MEDIUM":
                single_route_statuses["PROPOSED_MEDIUM"] += 1
                recovered_medium.add(row["designator"])
            elif row["review_status"] == "AMBIGUOUS":
                single_route_statuses["AMBIGUOUS"] += 1
            else:
                single_route_statuses["UNRESOLVED"] += 1

    for row in review:
        if (
            row["review_status"]
            not in ("ALREADY_VERIFIED", "PROPOSED_HIGH", "PROPOSED_MEDIUM")
            or not row["station_name"]
            or not row["route"]
        ):
            continue
        matches = station_route_matches(row["station_name"], row["route"], catalog)
        if not matches:
            row["legacy_evidence"] = (
                "No matching historical station/service entity was found by "
                "normalized station identity and route."
            )
            row["legacy_relationship"] = "NO_STATION_SERVICE_MATCH"
            continue
        display = []
        for item in matches:
            display.append(
                f"{item['station_name']} [{item['historical_routes']}; "
                f"{item['legacy_mapping_mode']}; {item['legacy_sources']}]"
            )
        evidence = "Historical station/service catalog: " + " || ".join(
            dict.fromkeys(display)
        )
        row["legacy_evidence"] = evidence
        row["legacy_relationship"] = "SAME_STATION_SERVICE"
        if row["review_status"] != "ALREADY_VERIFIED":
            row["current_visual_evidence"] = (
                "Current V1.1 artwork and projected package position identify the "
                "station/service lane; historical catalog independently corroborates "
                "the station identity and service. Legacy LED designator is not used "
                "as the current physical identity."
            )
        single_match = any(
            record["mode"] == "single_route_LED"
            and names_match(row["station_name"], record["station"])
            and routes(row["route"]) & routes(record["routes"])
            for record in single_records
        )
        if single_match and row["review_status"] == "PROPOSED_MEDIUM":
            row["review_status"] = "PROPOSED_HIGH"
            row["confidence"] = "0.97"
            recovered_medium.discard(row["designator"])
            recovered_high.add(row["designator"])
        elif single_match and row["review_status"] == "PROPOSED_HIGH":
            recovered_high.add(row["designator"])
        if not row["station_id"] and row["station_key"]:
            for master in master_by_led.values():
                if (
                    master["station_key"] == row["station_key"]
                    and master["station_id"]
                ):
                    row["station_id"] = master["station_id"]
                    break

    write_csv(review_path, REVIEW_FIELDS, review)

    preview = imread(PREVIEW)
    if preview.shape[1] != 4096 or preview.shape[0] != 3578:
        raise RuntimeError(
            f"Expected original 4096x3578 preview, got "
            f"{preview.shape[1]}x{preview.shape[0]}"
        )
    atlas_count, atlas_matched, _ = build_atlas(
        catalog, review, cp19, transfer_rows, preview
    )

    status_counts = Counter(row["review_status"] for row in review)
    shared_status_counts: Counter[str] = Counter()
    resolved_shared_leds = set()
    shared_clusters = set()
    for row in review:
        if not row["station_name"]:
            continue
        matched = [
            item
            for item in shared_records
            if names_match(row["station_name"], item["station"])
            and routes(row["route"]) & routes(item["routes"])
        ]
        if not matched:
            continue
        shared_clusters.add(normalize(row["station_name"]))
        shared_status_counts[row["review_status"]] += 1
        if row["review_status"] in ("ALREADY_VERIFIED", "PROPOSED_HIGH", "PROPOSED_MEDIUM"):
            resolved_shared_leds.add(row["designator"])

    cp19_counts = Counter(row["relationship"] for row in cp19)
    proposal_count = status_counts["PROPOSED_HIGH"] + status_counts["PROPOSED_MEDIUM"]
    lines = [
        "# Phase 3B station-centric legacy recovery",
        "",
        "This is a review-only pass. The accepted physical/raster registration and",
        "projected LED coordinates were used unchanged. No semantic was promoted",
        "to VERIFIED, and the master worksheet and station registry were not edited.",
        "",
        "## Current totals",
        "",
        f"- ALREADY_VERIFIED: **{status_counts['ALREADY_VERIFIED']}**",
        f"- PROPOSED_HIGH: **{status_counts['PROPOSED_HIGH']}**",
        f"- PROPOSED_MEDIUM: **{status_counts['PROPOSED_MEDIUM']}**",
        f"- AMBIGUOUS: **{status_counts['AMBIGUOUS']}**",
        f"- UNRESOLVED: **{status_counts['UNRESOLVED']}**",
        f"- POTENTIAL_CONTRADICTION: **{status_counts['POTENTIAL_CONTRADICTION']}**",
        f"- Usable proposals among the original 251 unverified LEDs: **{proposal_count}**.",
        f"- Current unresolved review rows: **{status_counts['UNRESOLVED']}**. The current",
        "  unresolved rows do not yet have a supported element type, so an exact count",
        "  of unresolved *station* LEDs cannot be established without guessing station",
        "  versus segment geometry.",
        "",
        "## A. Old single-route stations",
        "",
        f"- Historical single-route station/service records: **{len(single_records)}**.",
        f"- Distinct station/service entities represented: **{len(single_entity_keys)}**.",
        f"- Current LEDs supported at HIGH level by current artwork plus single-route",
        f"  history: **{len(recovered_high)}**.",
        f"- Current LEDs supported at MEDIUM level by single-route history: **{len(recovered_medium)}**.",
        f"- Matched current ambiguous LEDs: **{single_route_statuses['AMBIGUOUS']}**.",
        f"- Matched current unresolved LEDs with a station identity available: **{single_route_statuses['UNRESOLVED']}**.",
        "",
        "Legacy designators are retained as evidence only. The recovered identity is",
        "the station/service entity; the current physical LED is selected by the V1.1",
        "artwork and projection. Existing single-route-supported MEDIUM proposals are",
        "raised to HIGH only when the station name and service agree and no verified",
        "assignment is displaced.",
        "",
        "## B. Old shared stations",
        "",
        f"- Shared historical station entities represented: **{len(shared_entities)}**.",
        f"- Station clusters with a current review-ledger station match: **{len(shared_clusters)}**.",
        f"- Route-specific current LEDs already VERIFIED or proposed at those clusters: **{len(resolved_shared_leds)}**.",
        f"- Current route-specific rows at matched shared clusters by status: "
        f"{dict(shared_status_counts)}.",
        "",
        "The old shared physical LED is not restored as a V1.1 identity. Where the",
        "current review already has a station/service assignment, the catalog supplies",
        "station-level corroboration; clusters without a defensible V1.1 route-lane",
        "assignment remain ambiguous or unresolved.",
        "",
        "## C. CP19 design-intent continuity",
        "",
        f"- CP19 rows reviewed: **{len(cp19)}**.",
        f"- Relationship counts: **{dict(cp19_counts)}**.",
        f"- Same designator + same station/service: **{cp19_counts['CONFIRMED_CONTINUITY']}**.",
        f"- Same designator with current semantic conflict: **{cp19_counts['CONFLICT']}**.",
        f"- CP19 designators superseded by another current LED at the station/service: **{cp19_counts['SUPERSEDED']}**.",
        f"- Conflicts: **{cp19_counts['CONFLICT']}**.",
        f"- Same station but different current service/LED: **{cp19_counts['SAME_STATION_DIFFERENT_ROUTE']}**.",
        f"- Still uncertain: **{cp19_counts['UNCERTAIN']}**.",
        "- `CONFIRMED_CONTINUITY` means current row has same designator, station, and",
        "  route. `SAME_STATION_DIFFERENT_ROUTE` and `SUPERSEDED` do not assert that",
        "  CP19's old physical placement survived. `CONFLICT` and `UNCERTAIN` are not",
        "  promoted.",
        "",
        "## D. Historical station catalog and visual atlas",
        "",
        f"- Historical catalog records normalized into station entities: **{len(catalog)}**.",
        f"- Station-name entities with current review-ledger matches: **{len(atlas_matched)}**.",
        f"- Atlas crops generated from the original 4096×3578 EasyEDA image: **{atlas_count}**.",
        f"- Shared-transfer legacy-pointer locator crops: **{len({normalize(row['transfer_station']) for row in transfer_rows})}**.",
        f"- Atlas directory: `station_recovery_atlas/`.",
        "- Crops show current nearby projected LED centers. Green denotes existing",
        "  ALREADY_VERIFIED items; orange denotes unverified reviewed candidates. Labels",
        "  include current designator, service (when assigned), and review status. CP19",
        "  and legacy-shared locator crops are explicitly investigative; the old LED",
        "  designator is not presumed to retain its historical station meaning.",
        "",
        "## Outputs and safeguards",
        "",
        "- `legacy_station_service_catalog.csv` records station entities, service",
        "  families, available GTFS parents, historical designators, source coordinates,",
        "  mapping modes, and source files.",
        "- `cp19_continuity_review.csv` classifies all CP19 rows against the current",
        "  review ledger without treating CP19 placements as current truth.",
        "- `current_semantic_review.csv` was updated only with station/service legacy",
        "  evidence and the supported single-route confidence/status adjustment.",
        "- No segment pass was attempted. No new VERIFIED assignment was made.",
        "- The original raster, accepted affine, projected centers, master worksheet,",
        "  station registry, firmware, and manufacturing files were not modified.",
    ]
    report_text = "\n".join(lines) + "\n"
    (ANALYSIS / "STATION_CENTRIC_LEGACY_RECOVERY.md").write_text(
        report_text, encoding="utf-8"
    )
    (ANALYSIS / "CURRENT_SEMANTIC_REVIEW.md").write_text(
        report_text, encoding="utf-8"
    )
    print(
        "Station recovery outputs:",
        {
            "catalog_entities": len(catalog),
            "atlas_crops": atlas_count,
            "CP19_relationships": dict(cp19_counts),
            "review_status": dict(status_counts),
            "single_route_recovered_high": len(recovered_high),
        },
    )


if __name__ == "__main__":
    main()
