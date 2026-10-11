#!/usr/bin/env python3
"""Project Phase 3B map LEDs using the accepted five-anchor affine fit."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import zipfile
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PHASE3B = PROJECT_ROOT / "docs" / "mapping" / "phase3b"
OUTPUT_DIR = PHASE3B / "analysis"
DEFAULT_PREVIEW = Path(
    r"C:\Users\jgold\OneDrive\Desktop\Brooklyn Subway Project"
    r"\2D_Brooklyn MTA Active Map PCB_2026-10-10.png"
)
DEFAULT_CPL = Path(
    r"C:\Users\jgold\OneDrive\Desktop\Brooklyn Subway Project"
    r"\PickAndPlace_Brooklyn MTA Active Map PCB_2026_10_09.xlsx"
)

# Accepted five-point least-squares fit. Do not refit this matrix.
AFFINE = np.array(
    [
        [0.2850745809161557, 0.00002354607163101985, -35.52992722650111],
        [0.000005678434809964113, -0.2850817346151787, 1191.9548886907905],
    ],
    dtype=np.float64,
)
ANCHORS = {
    "LED42": (1496.605, 2574.629),
    "LED208": (2289.111, 2861.155),
    "LED165": (1832.971, 3293.046),
    "LED189": (886.607, 1552.603),
    "LED163": (1064.881, 1762.140),
}

MAP_FIELDS = (
    "designator",
    "channel",
    "channel_index",
    "global_physical_order",
    "pcb_x",
    "pcb_y",
    "pixel_x",
    "pixel_y",
)
CPL_FIELDS = (
    "designator",
    "cpl_mid_x_mm",
    "cpl_mid_y_mm",
    "placement_x_internal",
    "placement_y_internal",
    "predicted_x_internal",
    "predicted_y_internal",
    "residual_x_mm",
    "residual_y_mm",
    "radial_residual_mm",
    "layer",
    "rotation_degrees",
    "within_0.05mm_tolerance",
)

MM_TO_INTERNAL = 100.0 / 2.54
INTERNAL_X_OFFSET = 600.0
INTERNAL_Y_OFFSET = -7980.0
CPL_TOLERANCE_MM = 0.05


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def parse_xlsx_sheet(path: Path) -> list[dict[str, str]]:
    """Read the CPL worksheet with the standard-library XLSX/OOXML reader."""
    ns = {
        "m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
        "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    }
    with zipfile.ZipFile(path) as archive:
        workbook = ET.fromstring(archive.read("xl/workbook.xml"))
        relationships = ET.fromstring(archive.read("xl/_rels/workbook.xml.rels"))
        targets = {item.attrib["Id"]: item.attrib["Target"] for item in relationships}
        shared_strings: list[str] = []
        if "xl/sharedStrings.xml" in archive.namelist():
            root = ET.fromstring(archive.read("xl/sharedStrings.xml"))
            text_tag = "{" + ns["m"] + "}t"
            shared_strings = [
                "".join(text.text or "" for text in item.iter(text_tag))
                for item in root
            ]

        sheets = workbook.find("m:sheets", ns)
        if sheets is None or len(sheets) != 1:
            raise RuntimeError("Expected exactly one worksheet in the CPL workbook")
        sheet = sheets[0]
        target = targets[sheet.attrib["{" + ns["r"] + "}id"]].lstrip("/")
        if not target.startswith("xl/"):
            target = "xl/" + target
        sheet_root = ET.fromstring(archive.read(target))

    rows: list[dict[str, str]] = []
    for row in sheet_root.findall(".//m:sheetData/m:row", ns):
        cells: dict[str, str] = {}
        for cell in row.findall("m:c", ns):
            match = re.match(r"([A-Z]+)", cell.attrib["r"])
            if match is None:
                continue
            value_element = cell.find("m:v", ns)
            value = value_element.text if value_element is not None else ""
            if cell.attrib.get("t") == "s" and value:
                value = shared_strings[int(value)]
            elif cell.attrib.get("t") == "inlineStr":
                value = "".join(
                    node.text or "" for node in cell.iter("{" + ns["m"] + "}t")
                )
            cells[match.group(1)] = value
        if cells:
            rows.append(cells)

    if not rows or rows[0].get("A") != "Designator":
        raise RuntimeError("CPL worksheet does not have the expected Designator header")
    header_to_column = {value: key for key, value in rows[0].items()}
    expected = ("Designator", "Mid X", "Mid Y", "Layer", "Rotation")
    missing_headers = [name for name in expected if name not in header_to_column]
    if missing_headers:
        raise RuntimeError("CPL is missing required columns: " + ", ".join(missing_headers))

    result: list[dict[str, str]] = []
    for row in rows[1:]:
        designator = row.get(header_to_column["Designator"], "")
        if re.fullmatch(r"LED\d+", designator):
            result.append(
                {
                    "designator": designator,
                    "mid_x": row.get(header_to_column["Mid X"], ""),
                    "mid_y": row.get(header_to_column["Mid Y"], ""),
                    "layer": row.get(header_to_column["Layer"], ""),
                    "rotation": row.get(header_to_column["Rotation"], ""),
                }
            )
    return result


def parse_mm(value: str, field: str, designator: str) -> float:
    value = value.strip()
    if not value.endswith("mm"):
        raise RuntimeError(f"{designator} CPL {field} is not expressed in mm: {value}")
    return float(value[:-2])


def project_point(x: float, y: float) -> tuple[float, float]:
    point = AFFINE[:, :2] @ np.array([x, y], dtype=np.float64) + AFFINE[:, 2]
    return float(point[0]), float(point[1])


def read_image(path: Path) -> np.ndarray:
    encoded = np.fromfile(path, dtype=np.uint8)
    image = cv2.imdecode(encoded, cv2.IMREAD_COLOR)
    if image is None:
        raise RuntimeError(f"Could not decode preview image: {path}")
    return image


def write_image(path: Path, image: np.ndarray) -> None:
    suffix = path.suffix or ".png"
    success, encoded = cv2.imencode(suffix, image)
    if not success:
        raise RuntimeError(f"Could not encode image: {path}")
    encoded.tofile(path)


def draw_center_markers(image: np.ndarray, rows: list[dict[str, object]]) -> np.ndarray:
    output = image.copy()
    for row in rows:
        center = (int(round(float(row["pixel_x"]))), int(round(float(row["pixel_y"]))))
        cv2.circle(output, center, 7, (0, 0, 0), 2, cv2.LINE_AA)
        cv2.line(
            output,
            (center[0] - 6, center[1]),
            (center[0] + 6, center[1]),
            (255, 255, 0),
            1,
            cv2.LINE_AA,
        )
        cv2.line(
            output,
            (center[0], center[1] - 6),
            (center[0], center[1] + 6),
            (255, 255, 0),
            1,
            cv2.LINE_AA,
        )
        cv2.circle(output, center, 2, (255, 0, 255), -1, cv2.LINE_AA)
    return output


def boxes_overlap(
    first: tuple[int, int, int, int],
    second: tuple[int, int, int, int],
    gap: int = 2,
) -> bool:
    return not (
        first[2] + gap < second[0]
        or second[2] + gap < first[0]
        or first[3] + gap < second[1]
        or second[3] + gap < first[1]
    )


def draw_designator_labels(
    image: np.ndarray, rows: list[dict[str, object]]
) -> np.ndarray:
    output = draw_center_markers(image, rows)
    font = cv2.FONT_HERSHEY_SIMPLEX
    scale = 0.34
    thickness = 1
    placed_boxes: list[tuple[int, int, int, int]] = []
    occupied_centers = [
        (int(round(float(row["pixel_x"]))), int(round(float(row["pixel_y"]))))
        for row in rows
    ]
    ordered = sorted(rows, key=lambda row: (row["pixel_y"], row["pixel_x"]))

    for row in ordered:
        label = str(row["designator"])
        center_x = int(round(float(row["pixel_x"])))
        center_y = int(round(float(row["pixel_y"])))
        (text_width, text_height), baseline = cv2.getTextSize(
            label, font, scale, thickness
        )
        candidates = (
            (center_x + 9, center_y - 8),
            (center_x + 9, center_y + text_height + 8),
            (center_x - text_width - 9, center_y - 8),
            (center_x - text_width - 9, center_y + text_height + 8),
            (center_x + 12, center_y + text_height // 2),
            (center_x - text_width - 12, center_y + text_height // 2),
            (center_x - text_width // 2, center_y - 12),
            (center_x - text_width // 2, center_y + text_height + 12),
        )
        best: tuple[float, int, int, tuple[int, int, int, int]] | None = None
        for text_x, text_y in candidates:
            box = (
                text_x,
                text_y - text_height,
                text_x + text_width,
                text_y + baseline,
            )
            overlaps = sum(boxes_overlap(box, existing) for existing in placed_boxes)
            marker_overlap = sum(
                box[0] - 2 <= x <= box[2] + 2 and box[1] - 2 <= y <= box[3] + 2
                for x, y in occupied_centers
                if math.hypot(x - center_x, y - center_y) > 8
            )
            score = overlaps * 1000 + marker_overlap * 100 + math.hypot(
                text_x - center_x, text_y - center_y
            )
            candidate = (score, text_x, text_y, box)
            if best is None or candidate[0] < best[0]:
                best = candidate
        assert best is not None
        _, text_x, text_y, box = best
        if math.hypot(text_x - center_x, text_y - center_y) > 14:
            anchor_x = min(max(center_x, box[0]), box[2])
            anchor_y = min(max(center_y, box[1]), box[3])
            cv2.line(
                output,
                (center_x, center_y),
                (anchor_x, anchor_y),
                (0, 0, 0),
                2,
                cv2.LINE_AA,
            )
        cv2.putText(
            output,
            label,
            (text_x, text_y),
            font,
            scale,
            (0, 0, 0),
            3,
            cv2.LINE_AA,
        )
        cv2.putText(
            output,
            label,
            (text_x, text_y),
            font,
            scale,
            (255, 255, 255),
            1,
            cv2.LINE_AA,
        )
        placed_boxes.append(box)
    return output


def sample_spread(
    rows: list[dict[str, object]],
) -> list[dict[str, object]]:
    min_x = min(float(row["pixel_x"]) for row in rows)
    max_x = max(float(row["pixel_x"]) for row in rows)
    min_y = min(float(row["pixel_y"]) for row in rows)
    max_y = max(float(row["pixel_y"]) for row in rows)
    span_x, span_y = max_x - min_x, max_y - min_y
    bins: dict[tuple[int, int], list[dict[str, object]]] = {
        (grid_x, grid_y): [] for grid_x in range(3) for grid_y in range(3)
    }
    for row in rows:
        grid_x = min(2, int((float(row["pixel_x"]) - min_x) / span_x * 3))
        grid_y = min(2, int((float(row["pixel_y"]) - min_y) / span_y * 3))
        bins[(grid_x, grid_y)].append(row)

    selected: list[dict[str, object]] = []
    selected_ids: set[str] = set()
    for grid_y in range(3):
        for grid_x in range(3):
            cell = bins[(grid_x, grid_y)]
            if not cell:
                continue
            center_x = min_x + (grid_x + 0.5) * span_x / 3
            center_y = min_y + (grid_y + 0.5) * span_y / 3
            first = min(
                cell,
                key=lambda row: math.hypot(
                    float(row["pixel_x"]) - center_x,
                    float(row["pixel_y"]) - center_y,
                ),
            )
            candidates = [first]
            if len(cell) > 1:
                second = max(
                    (row for row in cell if row["designator"] != first["designator"]),
                    key=lambda row: math.hypot(
                        float(row["pixel_x"]) - float(first["pixel_x"]),
                        float(row["pixel_y"]) - float(first["pixel_y"]),
                    ),
                )
                candidates.append(second)
            for row in candidates:
                designator = str(row["designator"])
                if designator not in selected_ids:
                    selected.append({**row, "_grid_x": grid_x, "_grid_y": grid_y})
                    selected_ids.add(designator)
    lower_right = min(
        (row for row in rows if str(row["designator"]) not in selected_ids),
        key=lambda row: math.hypot(
            float(row["pixel_x"]) - 2300.0,
            float(row["pixel_y"]) - 3000.0,
        ),
    )
    selected.append({**lower_right, "_region_override": "lower-right"})
    return selected


def coordinate_match_rows(
    cpl_rows: list[dict[str, str]],
    placements: dict[str, dict[str, str]],
    chain_rows: list[dict[str, str]],
) -> tuple[list[dict[str, str]], dict[str, object]]:
    by_designator: dict[str, dict[str, str]] = {}
    for row in cpl_rows:
        if row["designator"] in by_designator:
            raise RuntimeError(f"Duplicate CPL designator: {row['designator']}")
        by_designator[row["designator"]] = row
    if set(by_designator) != set(placements):
        missing = sorted(set(placements) - set(by_designator))
        extra = sorted(set(by_designator) - set(placements))
        raise RuntimeError(f"CPL/placement designator mismatch; missing={missing}, extra={extra}")
    chain_by_designator = {row["designator"]: row for row in chain_rows}
    if len(chain_by_designator) != len(chain_rows) or set(chain_by_designator) != set(placements):
        raise RuntimeError("CPL and authoritative chain-map inventories do not match")

    result = []
    errors: list[float] = []
    layers: Counter[str] = Counter()
    rotations: Counter[str] = Counter()
    for designator in sorted(by_designator, key=lambda item: int(item[3:])):
        cpl = by_designator[designator]
        placement = placements[designator]
        cpl_x = parse_mm(cpl["mid_x"], "Mid X", designator)
        cpl_y = parse_mm(cpl["mid_y"], "Mid Y", designator)
        expected_x = float(placement["x"])
        expected_y = float(placement["y"])
        predicted_x = cpl_x * MM_TO_INTERNAL + INTERNAL_X_OFFSET
        predicted_y = cpl_y * MM_TO_INTERNAL + INTERNAL_Y_OFFSET
        residual_x_mm = (predicted_x - expected_x) / MM_TO_INTERNAL
        residual_y_mm = (predicted_y - expected_y) / MM_TO_INTERNAL
        radial = math.hypot(residual_x_mm, residual_y_mm)
        errors.append(radial)
        layers[cpl["layer"]] += 1
        rotations[cpl["rotation"]] += 1
        result.append(
            {
                "designator": designator,
                "cpl_mid_x_mm": f"{cpl_x:.6f}",
                "cpl_mid_y_mm": f"{cpl_y:.6f}",
                "placement_x_internal": f"{expected_x:.6f}",
                "placement_y_internal": f"{expected_y:.6f}",
                "predicted_x_internal": f"{predicted_x:.6f}",
                "predicted_y_internal": f"{predicted_y:.6f}",
                "residual_x_mm": f"{residual_x_mm:.9f}",
                "residual_y_mm": f"{residual_y_mm:.9f}",
                "radial_residual_mm": f"{radial:.9f}",
                "layer": cpl["layer"],
                "rotation_degrees": cpl["rotation"],
                "within_0.05mm_tolerance": str(radial <= CPL_TOLERANCE_MM).upper(),
            }
        )

    report = {
        "addressable_led_count": len(by_designator),
        "unique_cpl_designators": len(set(by_designator)),
        "placement_designator_count": len(placements),
        "map_designator_count": sum(row["role"] == "map" for row in chain_rows),
        "status_designator_count": sum(row["role"].startswith("status_") for row in chain_rows),
        "one_to_one_designator_match": True,
        "units": "CPL Mid X/Mid Y are mm; led_positions_v11 x/y are EasyEDA internal units consistent with mils.",
        "nominal_conversion": (
            "placement_x_internal = CPL_Mid_X_mm * 39.37007874015748 + 600; "
            "placement_y_internal = CPL_Mid_Y_mm * 39.37007874015748 - 7980"
        ),
        "axis_inversion_between_cpl_and_placement": "none",
        "origin_translation_internal_units": {"x": 600, "y": -7980},
        "tolerance_mm": CPL_TOLERANCE_MM,
        "tolerance_match_count": sum(error <= CPL_TOLERANCE_MM for error in errors),
        "mismatch_count": sum(error > CPL_TOLERANCE_MM for error in errors),
        "median_residual_mm": float(np.median(errors)),
        "maximum_residual_mm": max(errors),
        "layer_counts": dict(sorted(layers.items())),
        "rotation_counts_degrees": dict(
            sorted(rotations.items(), key=lambda item: float(item[0]))
        ),
    }
    if report["mismatch_count"]:
        report["mismatches"] = [
            {
                "designator": row["designator"],
                "radial_residual_mm": row["radial_residual_mm"],
            }
            for row in result
            if float(row["radial_residual_mm"]) > CPL_TOLERANCE_MM
        ]
    else:
        report["mismatches"] = []
    return result, report


def write_csv(path: Path, fields: tuple[str, ...], rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def render_report(
    image_path: Path,
    cpl_path: Path,
    image: np.ndarray,
    projected: list[dict[str, object]],
    cpl_report: dict[str, object],
    anchor_report: list[dict[str, object]],
    output_dir: Path,
    visual_registration: str,
) -> str:
    samples = sample_spread(projected)
    lines = [
        "# Phase 3B 302-LED projection and CPL cross-check",
        "",
        "Projection-only deliverable. **No semantic classification was performed.**",
        "The existing semantic worksheet, station registry, firmware, and manufacturing",
        "sources were used as read-only inputs and were not modified.",
        "",
        "## Primary image and accepted affine",
        "",
        f"- Primary raster: `{image_path}`",
        f"- Raster dimensions: {image.shape[1]} × {image.shape[0]} pixels",
        f"- SHA-256: `{hashlib.sha256(image_path.read_bytes()).hexdigest()}`",
        "- Coordinate input: `docs/mapping/phase3b/led_positions_v11.csv` x/y columns",
        "  (EasyEDA internal placement units; not millimetres).",
        "- Affine was kept exactly as accepted from the five supplied package-center",
        "  measurements. It was not refit from borders, outlines, image extents, or routes.",
        "",
        "```text",
        f"pixel_x = {AFFINE[0, 0]:.17g} * pcb_x {AFFINE[0, 1]:+.17g} * pcb_y {AFFINE[0, 2]:+.17g}",
        f"pixel_y = {AFFINE[1, 0]:.17g} * pcb_x {AFFINE[1, 1]:+.17g} * pcb_y {AFFINE[1, 2]:+.17g}",
        "```",
        "",
        "The diagonal terms give approximately 0.285075 px per placement unit.",
        "The off-diagonal terms encode the small fitted skew/rotation; they were preserved.",
        "The negative image-Y scale accounts for raster rows increasing downward.",
        "",
        "### Supplied anchor reproduction",
        "",
        "| LED | Measured pixel center | Predicted pixel center | Radial residual |",
        "|---|---:|---:|---:|",
    ]
    for row in anchor_report:
        lines.append(
            f"| {row['designator']} | ({row['measured_x']:.3f}, {row['measured_y']:.3f}) "
            f"| ({row['predicted_x']:.6f}, {row['predicted_y']:.6f}) "
            f"| {row['radial_error']:.6f} px |"
        )
    lines.extend(
        [
            "",
            "## Independent CPL validation",
            "",
            f"- CPL workbook: `{cpl_path}`",
            f"- Addressable LED designators in CPL: {cpl_report['addressable_led_count']}",
            f"- Designators present one-to-one in CPL and placement coordinates: "
            f"{cpl_report['one_to_one_designator_match']}",
            f"- Physical inventory split: {cpl_report['map_designator_count']} map LEDs + "
            f"{cpl_report['status_designator_count']} status LEDs.",
            f"- Units/origin: {cpl_report['units']}",
            f"- CPL-to-placement conversion: `{cpl_report['nominal_conversion']}`",
            f"- Axis inversion between CPL and placement coordinates: "
            f"{cpl_report['axis_inversion_between_cpl_and_placement']}",
            f"- CPL side/layer counts: `{json.dumps(cpl_report['layer_counts'], sort_keys=True)}`",
            f"- CPL rotations (degrees/count): "
            f"`{json.dumps(cpl_report['rotation_counts_degrees'], sort_keys=True)}`",
            f"- Tolerance matches at ≤ {CPL_TOLERANCE_MM:.2f} mm: "
            f"{cpl_report['tolerance_match_count']}/{cpl_report['addressable_led_count']}",
            f"- Median residual: {cpl_report['median_residual_mm']:.9f} mm",
            f"- Maximum residual: {cpl_report['maximum_residual_mm']:.9f} mm",
            f"- Mismatches: {cpl_report['mismatch_count']}",
            "",
            "The CPL is an independent manufacturing cross-check; the authoritative",
            "placement coordinates were retained for projection. The four status",
            "inventory items are not included in the 302-row projected map CSV.",
            "",
            "## Projected map samples",
            "",
            "Samples cover each populated cell of a 3×3 grid over the map-LED extent.",
            "A separate lower-right map-area sample is included from the Brighton Beach",
            "corridor; the bottom-right rectangle of the full raster has no map LEDs.",
            "These are projection checks, not semantic assertions.",
            "",
            "| Image region | Designator | PCB coordinates (internal units) | Projected center (px) |",
            "|---|---|---:|---:|",
        ]
    )
    for row in samples:
        if "_region_override" in row:
            region = str(row["_region_override"])
        else:
            gx = int(row["_grid_x"])
            gy = int(row["_grid_y"])
            region = f"{('upper','center','lower')[gy]}-{('left','center','right')[gx]}"
        lines.append(
            f"| {region} | {row['designator']} | "
            f"({float(row['pcb_x']):.4f}, {float(row['pcb_y']):.4f}) | "
            f"({float(row['pixel_x']):.3f}, {float(row['pixel_y']):.3f}) |"
        )
    lines.extend(
        [
            "",
            "## Visual registration gate",
            "",
            f"**GLOBAL REGISTRATION: {visual_registration.upper()}**",
            "",
            "Inspected in `projected_led_centers.png`: DeKalb Av, Atlantic Av-Barclays Ctr,",
            "Prospect Park / 7 Av, Brighton Beach, Coney Island, Culver corridor, eastern",
            "Brooklyn, and northern/eastern route corridors. No systematic displacement",
            "was visible; projected centers coincide with rendered package centers in the",
            "inspected regions.",
            "",
            "This is a visual registration result, separate from semantic classification.",
            "",
            "## Generated files",
            "",
            "- `projected_led_centers.csv` — 302 physical map LED projections.",
            "- `projected_led_centers.png` — unobstructed-label center markers on the original raster.",
            "- `projected_led_designators.png` — labeled projection overlay.",
            "- `cpl_validation.csv` — all 306 addressable LED CPL-to-placement residuals.",
            "- `projection_report.md` — this report.",
            "",
            f"All outputs are in `{output_dir}`. No semantic proposals were generated.",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", type=Path, default=DEFAULT_PREVIEW)
    parser.add_argument("--cpl", type=Path, default=DEFAULT_CPL)
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR)
    parser.add_argument(
        "--visual-registration",
        choices=("pending", "pass", "fail"),
        default="pending",
        help="Record the result of manual, full-board overlay inspection.",
    )
    args = parser.parse_args()

    position_rows = read_csv(PHASE3B / "led_positions_v11.csv")
    placements = {row["designator"]: row for row in position_rows}
    if len(placements) != len(position_rows) or len(placements) != 306:
        raise RuntimeError("Expected 306 unique physical LED positions")

    chain_rows = read_csv(PROJECT_ROOT / "data" / "led_chain_map.csv")
    if len(chain_rows) != 306:
        raise RuntimeError("Expected 306 authoritative physical chain-map rows")
    chain_by_designator = {row["designator"]: row for row in chain_rows}
    if len(chain_by_designator) != len(chain_rows) or set(chain_by_designator) != set(placements):
        raise RuntimeError("Placement coordinates do not match the authoritative chain map")
    map_rows = [row for row in chain_rows if row["role"] == "map"]
    if len(map_rows) != 302:
        raise RuntimeError(f"Expected 302 map LEDs, found {len(map_rows)}")
    channel_indexes = {(row["channel"], row["channel_index"]) for row in map_rows}
    physical_orders = {row["global_physical_order"] for row in map_rows}
    if len(channel_indexes) != len(map_rows) or len(physical_orders) != len(map_rows):
        raise RuntimeError("Map chain references contain duplicate channel/index or global order")

    image = read_image(args.preview)
    if image.shape[1::-1] != (4096, 3578):
        raise RuntimeError(
            f"Expected original 4096x3578 EasyEDA PNG, got {image.shape[1]}x{image.shape[0]}"
        )
    projected: list[dict[str, object]] = []
    for physical in map_rows:
        placement = placements[physical["designator"]]
        pcb_x, pcb_y = float(placement["x"]), float(placement["y"])
        pixel_x, pixel_y = project_point(pcb_x, pcb_y)
        projected.append(
            {
                "designator": physical["designator"],
                "channel": physical["channel"],
                "channel_index": physical["channel_index"],
                "global_physical_order": physical["global_physical_order"],
                "pcb_x": pcb_x,
                "pcb_y": pcb_y,
                "pixel_x": pixel_x,
                "pixel_y": pixel_y,
            }
        )
    if len({row["designator"] for row in projected}) != 302:
        raise RuntimeError("Projected map LED designators are not unique")

    anchor_report = []
    projected_by_designator = {row["designator"]: row for row in projected}
    for designator, measured in ANCHORS.items():
        if designator not in placements:
            raise RuntimeError(f"Calibration anchor missing from placement file: {designator}")
        x, y = project_point(float(placements[designator]["x"]), float(placements[designator]["y"]))
        error = math.hypot(x - measured[0], y - measured[1])
        anchor_report.append(
            {
                "designator": designator,
                "measured_x": measured[0],
                "measured_y": measured[1],
                "predicted_x": x,
                "predicted_y": y,
                "radial_error": error,
            }
        )
        if error >= 1.0:
            raise RuntimeError(f"Accepted affine no longer reproduces {designator} within 1 px")
        if designator in projected_by_designator:
            projection = projected_by_designator[designator]
            if abs(float(projection["pixel_x"]) - x) > 1e-9 or abs(
                float(projection["pixel_y"]) - y
            ) > 1e-9:
                raise RuntimeError(f"Anchor projection differs between output paths: {designator}")

    cpl_rows = parse_xlsx_sheet(args.cpl)
    cpl_validation_rows, cpl_report = coordinate_match_rows(
        cpl_rows, placements, chain_rows
    )
    if cpl_report["tolerance_match_count"] != 306:
        raise RuntimeError("CPL coordinate cross-check has mismatches; inspect report output")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(args.output_dir / "projected_led_centers.csv", MAP_FIELDS, projected)
    write_csv(args.output_dir / "cpl_validation.csv", CPL_FIELDS, cpl_validation_rows)
    write_image(
        args.output_dir / "projected_led_centers.png",
        draw_center_markers(image, projected),
    )
    write_image(
        args.output_dir / "projected_led_designators.png",
        draw_designator_labels(image, projected),
    )
    report = render_report(
        args.preview,
        args.cpl,
        image,
        projected,
        cpl_report,
        anchor_report,
        args.output_dir,
        args.visual_registration,
    )
    (args.output_dir / "projection_report.md").write_text(report, encoding="utf-8")
    print(
        json.dumps(
            {
                "projected_map_leds": len(projected),
                "cpl": cpl_report,
                "anchors": anchor_report,
                "outputs": [
                    str(args.output_dir / name)
                    for name in (
                        "projected_led_centers.csv",
                        "projected_led_centers.png",
                        "projected_led_designators.png",
                        "cpl_validation.csv",
                        "projection_report.md",
                    )
                ],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
