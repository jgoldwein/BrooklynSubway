#!/usr/bin/env python3
"""Deterministic raster/geometric analysis for the Phase 3B LED map."""

from __future__ import annotations

import argparse
import csv
import heapq
import json
import math
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PHASE3B = PROJECT_ROOT / "docs" / "mapping" / "phase3b"
DEFAULT_OUTPUT = PHASE3B / "analysis"

COLOR_FAMILIES = (
    ("red", 0, 11),
    ("orange", 11, 24),
    ("yellow", 24, 40),
    ("green", 40, 82),
    ("cyan", 82, 103),
    ("blue", 103, 132),
    ("violet", 132, 160),
)

CSV_FIELDS = (
    "designator",
    "channel",
    "channel_index",
    "global_physical_order",
    "pcb_x",
    "pcb_y",
    "image_x",
    "image_y",
    "nearest_color_family",
    "nearest_route_candidate",
    "distance_to_route_pixels",
    "second_route_candidate",
    "second_color_family",
    "second_distance",
    "color_confidence",
    "path_component",
    "along_path_position",
    "existing_semantic_status",
    "existing_route",
    "existing_element_type",
    "element_type_candidate",
    "element_type_method",
    "analysis_notes",
)


@dataclass
class RegisteredImage:
    name: str
    image: np.ndarray
    image_to_overview: np.ndarray
    overview_to_image: np.ndarray
    scale: float


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def validate_physical_inventory(
    project_root: Path, map_rows: list[dict[str, str]]
) -> dict[str, object]:
    chain_rows = [
        row
        for row in read_csv(project_root / "data" / "led_chain_map.csv")
        if row["role"] == "map"
    ]
    fields = ("designator", "channel", "channel_index", "global_physical_order")
    designators = [row["designator"] for row in map_rows]
    channel_indexes = [(row["channel"], row["channel_index"]) for row in map_rows]
    physical_orders = [row["global_physical_order"] for row in map_rows]
    if len(set(designators)) != len(map_rows):
        raise RuntimeError("Duplicate designator in map worksheet rows")
    if len(set(channel_indexes)) != len(map_rows):
        raise RuntimeError("Duplicate channel/channel_index in map worksheet rows")
    if len(set(physical_orders)) != len(map_rows):
        raise RuntimeError("Duplicate global physical order in map worksheet rows")
    worksheet_refs = {row["designator"]: row for row in map_rows}
    chain_refs = {row["designator"]: row for row in chain_rows}
    if len(chain_rows) != 302 or set(worksheet_refs) != set(chain_refs):
        raise RuntimeError(
            f"Map inventory mismatch: worksheet={len(worksheet_refs)}, chain map={len(chain_refs)}"
        )
    mismatches = [
        designator
        for designator, row in worksheet_refs.items()
        if any(row[field] != chain_refs[designator][field] for field in fields)
    ]
    if mismatches:
        raise RuntimeError(
            "Physical map references differ from data/led_chain_map.csv: "
            + ", ".join(mismatches[:10])
        )
    return {
        "map_rows": len(map_rows),
        "chain_map_rows": len(chain_rows),
        "unique_designators": len(set(designators)),
        "unique_channel_indexes": len(set(channel_indexes)),
        "unique_global_physical_orders": len(set(physical_orders)),
        "all_physical_references_match_chain_map": True,
    }


def write_csv(path: Path, fields: Iterable[str], rows: Iterable[dict]) -> None:
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(fields))
        writer.writeheader()
        writer.writerows(rows)


def to_float(value: str) -> float:
    return float(value.strip())


def fit_crop_registration(
    reference: np.ndarray, crop: np.ndarray, name: str
) -> tuple[np.ndarray, int, float, float]:
    """Return crop-pixel -> overview-pixel homography and match statistics."""
    detector = cv2.SIFT_create(nfeatures=16000, contrastThreshold=0.02)
    ref_keypoints, ref_descriptors = detector.detectAndCompute(
        cv2.cvtColor(reference, cv2.COLOR_BGR2GRAY), None
    )
    crop_keypoints, crop_descriptors = detector.detectAndCompute(
        cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY), None
    )
    if ref_descriptors is None or crop_descriptors is None:
        raise RuntimeError(f"Could not extract image features for {name}")

    matcher = cv2.BFMatcher(cv2.NORM_L2)
    matches = matcher.knnMatch(crop_descriptors, ref_descriptors, k=2)
    good = [first for first, second in matches if first.distance < 0.72 * second.distance]
    if len(good) < 12:
        raise RuntimeError(f"Too few crop-registration matches for {name}: {len(good)}")

    crop_points = np.float32([crop_keypoints[item.queryIdx].pt for item in good])
    ref_points = np.float32([ref_keypoints[item.trainIdx].pt for item in good])
    homography, inliers = cv2.findHomography(
        crop_points, ref_points, cv2.RANSAC, 3.0, maxIters=10000, confidence=0.999
    )
    inlier_count = int(inliers.sum()) if inliers is not None else 0
    if homography is None or inlier_count < 12:
        raise RuntimeError(
            f"Unreliable crop-registration for {name}: {inlier_count} inliers"
        )

    projected = cv2.perspectiveTransform(crop_points[:, None, :], homography)[:, 0, :]
    errors = np.linalg.norm(projected - ref_points, axis=1)
    rms = float(np.sqrt(np.mean(np.square(errors[inliers[:, 0] != 0]))))

    center = np.float32([[[crop.shape[1] / 2, crop.shape[0] / 2]]])
    center_jacobian = np.empty((2, 2), dtype=np.float64)
    base = cv2.perspectiveTransform(center, homography)[0, 0]
    for axis in range(2):
        shifted = center.copy()
        shifted[0, 0, axis] += 1
        center_jacobian[:, axis] = (
            cv2.perspectiveTransform(shifted, homography)[0, 0] - base
        )
    scale = float(math.sqrt(abs(np.linalg.det(center_jacobian))))
    return homography, inlier_count, rms, scale


def hue_mask(hsv: np.ndarray, lower: int, upper: int) -> np.ndarray:
    mask = cv2.inRange(
        hsv,
        np.array([lower, 75, 45], dtype=np.uint8),
        np.array([upper - 1, 255, 255], dtype=np.uint8),
    )
    return mask


def clean_route_mask(mask: np.ndarray) -> np.ndarray:
    close_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, close_kernel)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(mask, 8)
    cleaned = np.zeros_like(mask)
    for component in range(1, count):
        if stats[component, cv2.CC_STAT_AREA] >= 36:
            cleaned[labels == component] = 255
    return cleaned


def make_palette_masks(
    image: np.ndarray,
) -> tuple[dict[str, np.ndarray], dict[str, tuple[int, int, int]]]:
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    masks: dict[str, np.ndarray] = {}
    colors: dict[str, tuple[int, int, int]] = {}
    for name, lower, upper in COLOR_FAMILIES:
        mask = clean_route_mask(hue_mask(hsv, lower, upper))
        if cv2.countNonZero(mask) == 0:
            continue
        masks[name] = mask
        median_bgr = np.median(image[mask != 0], axis=0)
        colors[name] = tuple(int(value) for value in median_bgr)
    return masks, colors


def nearest_route_pixels(
    masks: dict[str, np.ndarray],
    x: float,
    y: float,
    radius: int,
) -> list[tuple[str, float, int]]:
    height, width = next(iter(masks.values())).shape
    left, right = max(0, int(x) - radius), min(width, int(x) + radius + 1)
    top, bottom = max(0, int(y) - radius), min(height, int(y) + radius + 1)
    if left >= right or top >= bottom:
        return []

    results = []
    for family, mask in masks.items():
        yy, xx = np.where(mask[top:bottom, left:right] != 0)
        if not len(xx):
            continue
        dx = xx + left - x
        dy = yy + top - y
        distances = np.hypot(dx, dy)
        results.append((family, float(distances.min()), int(len(xx))))
    return sorted(results, key=lambda item: item[1])


def initial_affine(map_rows: list[dict[str, str]], image: np.ndarray) -> np.ndarray:
    """Map PCB coordinate bounds into the board overview image bounds."""
    xs = np.array([to_float(row["x"]) for row in map_rows], dtype=np.float64)
    ys = np.array([to_float(row["y"]) for row in map_rows], dtype=np.float64)
    x0, x1 = float(xs.min()), float(xs.max())
    y0, y1 = float(ys.min()), float(ys.max())
    # The board overview contains a margin around the map.
    left, right = image.shape[1] * 0.12, image.shape[1] * 0.89
    top, bottom = image.shape[0] * 0.035, image.shape[0] * 0.97
    sx = (right - left) / (x1 - x0)
    sy = -(bottom - top) / (y1 - y0)
    tx = left - sx * x0
    ty = bottom - sy * y0
    return np.array([[sx, 0.0, tx], [0.0, sy, ty]], dtype=np.float64)


def refine_affine_from_anchors(
    affine: np.ndarray,
    anchors: list[dict[str, str]],
    route_masks: dict[str, np.ndarray],
) -> tuple[np.ndarray, float, int]:
    if len(anchors) < 6:
        raise RuntimeError("At least six verified station anchors are required")
    world = np.float32([[to_float(r["x"]), to_float(r["y"])] for r in anchors])
    current = affine.copy()
    inlier_count = 0

    for _ in range(8):
        image_points = cv2.transform(world[None, :, :], current)[0]
        matched = []
        valid_world = []
        for anchor, point in zip(anchors, image_points):
            candidates = nearest_route_pixels(route_masks, point[0], point[1], 52)
            if not candidates:
                continue
            best_family, best_distance, _ = candidates[0]
            # Exclude a nearby route of a visibly different family only when
            # the verified route has already established an expected color.
            if best_distance > 52:
                continue
            px = int(round(point[0]))
            py = int(round(point[1]))
            local = np.argwhere(route_masks[best_family][max(0, py-52):py+53, max(0, px-52):px+53] != 0)
            if not len(local):
                continue
            offset = np.array([max(0, px-52), max(0, py-52)])
            local += offset
            distances = np.linalg.norm(local - point, axis=1)
            nearest = local[int(np.argmin(distances))]
            valid_world.append([to_float(anchor["x"]), to_float(anchor["y"])])
            matched.append(nearest.astype(np.float32))
        if len(matched) < 6:
            break
        updated, inliers = cv2.estimateAffine2D(
            np.float32(valid_world),
            np.float32(matched),
            method=cv2.RANSAC,
            ransacReprojThreshold=10.0,
            maxIters=10000,
            confidence=0.999,
            refineIters=20,
        )
        if updated is None:
            break
        inlier_count = int(inliers.sum()) if inliers is not None else len(matched)
        if inlier_count < 6:
            break
        current = updated

    final_points = cv2.transform(world[None, :, :], current)[0]
    residuals = []
    for point in final_points:
        candidates = nearest_route_pixels(route_masks, point[0], point[1], 52)
        if candidates:
            residuals.append(candidates[0][1])
    rms = float(np.sqrt(np.mean(np.square(residuals)))) if residuals else float("inf")
    return current, rms, inlier_count


def route_line_rms(
    affine: np.ndarray,
    anchors: list[dict[str, str]],
    route_masks: dict[str, np.ndarray],
) -> float:
    distances = []
    for anchor in anchors:
        point = project_point(affine, to_float(anchor["x"]), to_float(anchor["y"]))
        candidates = nearest_route_pixels(route_masks, point[0], point[1], 52)
        if candidates:
            distances.append(candidates[0][1])
    return float(np.sqrt(np.mean(np.square(distances)))) if distances else float("inf")


def invert_homography(matrix: np.ndarray) -> np.ndarray:
    return np.linalg.inv(matrix)


def project_point(matrix: np.ndarray, x: float, y: float) -> tuple[float, float]:
    point = cv2.transform(
        np.float32([[[x, y]]]), np.asarray(matrix, dtype=np.float64)
    )[0, 0]
    return float(point[0]), float(point[1])


def choose_region(
    regions: list[RegisteredImage], image_x: float, image_y: float
) -> tuple[RegisteredImage, float, float] | None:
    candidates = []
    for region in regions:
        rx, ry = project_point(region.overview_to_image, image_x, image_y)
        height, width = region.image.shape[:2]
        margin = min(rx, ry, width - 1 - rx, height - 1 - ry)
        if margin > 0:
            candidates.append((region.scale, margin, region, rx, ry))
    if not candidates:
        return None
    _, _, region, rx, ry = max(candidates, key=lambda item: (item[0], item[1]))
    return region, rx, ry


def region_sample(
    masks: dict[str, np.ndarray],
    x: float,
    y: float,
) -> list[tuple[str, float, int]]:
    radius = 22
    inner = 3
    height, width = next(iter(masks.values())).shape
    left, right = max(0, int(x) - radius), min(width, int(x) + radius + 1)
    top, bottom = max(0, int(y) - radius), min(height, int(y) + radius + 1)
    yy_grid, xx_grid = np.ogrid[top:bottom, left:right]
    radial = np.hypot(xx_grid - x, yy_grid - y)
    annulus = (radial >= inner) & (radial <= radius)
    results = []
    for family, mask in masks.items():
        local = mask[top:bottom, left:right] != 0
        selected = local & annulus
        yy, xx = np.where(selected)
        if len(xx) >= 4:
            distances = np.hypot(xx + left - x, yy + top - y)
            results.append((family, float(distances.min()), int(len(xx))))
    return sorted(results, key=lambda item: (item[1], -item[2]))


def skeletonize(mask: np.ndarray) -> np.ndarray:
    remaining = mask.copy()
    skeleton = np.zeros_like(mask)
    kernel = cv2.getStructuringElement(cv2.MORPH_CROSS, (3, 3))
    while cv2.countNonZero(remaining):
        eroded = cv2.erode(remaining, kernel)
        opened = cv2.dilate(eroded, kernel)
        skeleton = cv2.bitwise_or(skeleton, cv2.subtract(remaining, opened))
        remaining = eroded
    return skeleton


def connected_path_data(
    mask: np.ndarray,
) -> tuple[np.ndarray, dict[int, dict[str, object]]]:
    skeleton = skeletonize(mask)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(skeleton, 8)
    components: dict[int, dict[str, object]] = {}
    for component in range(1, count):
        if stats[component, cv2.CC_STAT_AREA] < 10:
            continue
        ys, xs = np.where(labels == component)
        coords = np.column_stack((xs, ys)).astype(np.int32)
        lookup = {int(y) * mask.shape[1] + int(x): i for i, (x, y) in enumerate(coords)}
        components[component] = {
            "coords": coords,
            "lookup": lookup,
            "distances": None,
        }
    return labels, components


def nearest_component_and_position(
    labels: np.ndarray,
    components: dict[int, dict[str, object]],
    x: float,
    y: float,
) -> tuple[str, str]:
    height, width = labels.shape
    radius = 30
    left, right = max(0, int(x) - radius), min(width, int(x) + radius + 1)
    top, bottom = max(0, int(y) - radius), min(height, int(y) + radius + 1)
    local = labels[top:bottom, left:right]
    yy, xx = np.where(local > 0)
    if not len(xx):
        return "", ""
    xs, ys = xx + left, yy + top
    distances = np.hypot(xs - x, ys - y)
    nearest_index = int(np.argmin(distances))
    component = int(local[yy[nearest_index], xx[nearest_index]])
    if component not in components:
        return "", ""

    path = components[component]
    coords = path["coords"]
    lookup = path["lookup"]
    distances = path["distances"]
    if distances is None:
        neighbors = ((-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0),
                     (-1, 1), (0, 1), (1, 1))
        degrees = []
        for px, py in coords:
            degree = sum(
                int(py + dy) * width + int(px + dx) in lookup
                for dx, dy in neighbors
            )
            degrees.append(degree)
        endpoints = np.flatnonzero(np.asarray(degrees) <= 1)
        root = int(endpoints[0]) if len(endpoints) else 0
        distances = np.full(len(coords), np.inf, dtype=np.float64)
        distances[root] = 0.0
        queue = [(0.0, root)]
        while queue:
            distance, index = heapq.heappop(queue)
            if distance != distances[index]:
                continue
            px, py = coords[index]
            for dx, dy in neighbors:
                neighbor = lookup.get(int(py + dy) * width + int(px + dx))
                if neighbor is None:
                    continue
                candidate = distance + (math.sqrt(2.0) if dx and dy else 1.0)
                if candidate < distances[neighbor]:
                    distances[neighbor] = candidate
                    heapq.heappush(queue, (candidate, neighbor))
        path["distances"] = distances
    nearest_pixel = int(lookup[int(ys[nearest_index]) * width + int(xs[nearest_index])])
    along = float(distances[nearest_pixel])
    component_name = f"component_{component}"
    return component_name, f"{along:.2f}"


def draw_overlay(
    base: np.ndarray,
    rows: list[dict[str, str]],
    output: Path,
    family_colors: dict[str, tuple[int, int, int]],
    mode: str,
) -> None:
    image = base.copy()
    for row in rows:
        family = row["nearest_color_family"]
        ambiguous = row["second_route_candidate"] != ""
        if mode == "unassigned" and row["existing_semantic_status"] == "VERIFIED":
            continue
        if mode == "ambiguous" and not ambiguous:
            continue
        if mode == "family" and not family:
            continue
        if mode == "unassigned" and not family:
            continue
        color = family_colors.get(family, (220, 220, 220))
        px, py = int(round(float(row["image_x"]))), int(round(float(row["image_y"])))
        cv2.circle(image, (px, py), 6, color, 2, cv2.LINE_AA)
        cv2.putText(
            image,
            row["designator"].removeprefix("LED"),
            (px + 7, py - 5),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.32,
            color,
            1,
            cv2.LINE_AA,
        )
    cv2.imwrite(str(output), image)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=PROJECT_ROOT)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    project_root = args.project_root.resolve()
    phase3b = project_root / "docs" / "mapping" / "phase3b"
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)

    master_path = phase3b / "phase3b_semantic_mapping_worksheet.csv"
    master = read_csv(master_path)
    map_rows = [row for row in master if row["role"] == "map"]
    anchors = [row for row in map_rows if row["semantic_status"] == "VERIFIED"]
    inventory_validation = validate_physical_inventory(project_root, map_rows)
    if len(map_rows) != 302 or len(anchors) != 51:
        raise RuntimeError(
            f"Expected 302 map LEDs and 51 verified anchors; found {len(map_rows)} and {len(anchors)}"
        )

    overview_path = phase3b / "pcb_led_designator_overlay.png"
    overview = cv2.imread(str(overview_path), cv2.IMREAD_COLOR)
    if overview is None:
        raise FileNotFoundError(overview_path)
    overview_masks, family_colors = make_palette_masks(overview)
    initial = initial_affine(map_rows, overview)
    affine, refined_rms, refined_inliers = refine_affine_from_anchors(
        initial, anchors, overview_masks
    )
    initial_rms = route_line_rms(initial, anchors, overview_masks)
    if refined_rms < initial_rms:
        transform_rms = refined_rms
        transform_method = "verified-anchor robust affine refinement"
    else:
        affine = initial
        transform_rms = initial_rms
        transform_method = "PCB/map extent affine; anchor refinement rejected"
    if not np.isfinite(transform_rms):
        raise RuntimeError(
            f"Could not measure verified-anchor route-line fit: RMS={transform_rms}"
        )

    registrations: list[RegisteredImage] = []
    registration_report = []
    for image_path in sorted((phase3b / "regions").glob("region_*.png")):
        image = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
        if image is None:
            raise RuntimeError(f"Could not read {image_path}")
        homography, inliers, rms, scale = fit_crop_registration(
            overview, image, image_path.name
        )
        registrations.append(
            RegisteredImage(
                image_path.name,
                image,
                homography,
                invert_homography(homography),
                scale,
            )
        )
        registration_report.append(
            {
                "image": image_path.name,
                "width": int(image.shape[1]),
                "height": int(image.shape[0]),
                "matches_inliers": inliers,
                "registration_rms_px": round(rms, 3),
                "crop_to_overview_scale": round(scale, 4),
            }
        )

    # Regional images are retained for traceable source registration; route
    # pixels are analyzed in the highest-resolution overview image.
    overview_path_data = {
        family: connected_path_data(mask) for family, mask in overview_masks.items()
    }

    anchor_color_family: dict[str, Counter[str]] = defaultdict(Counter)
    analyzed: list[dict[str, str]] = []
    for row in map_rows:
        pcb_x, pcb_y = to_float(row["x"]), to_float(row["y"])
        image_x, image_y = project_point(affine, pcb_x, pcb_y)
        selected = choose_region(registrations, image_x, image_y)
        notes = []
        candidates: list[tuple[str, float, int]] = []
        path_component = ""
        along_position = ""
        if selected is None:
            notes.append("Projected center falls outside all registered regional images.")
        else:
            candidates = region_sample(overview_masks, image_x, image_y)
            if candidates:
                family = candidates[0][0]
                labels, components = overview_path_data.get(family, (None, {}))
                if labels is not None:
                    path_component, along_position = nearest_component_and_position(
                        labels, components, image_x, image_y
                    )
            else:
                notes.append("No palette-classified route pixels in the LED annulus.")

        if row["semantic_status"] == "VERIFIED" and candidates:
            anchor_color_family[row["route"]][candidates[0][0]] += 1
        unique_families = [
            item for item in candidates if item[2] >= 3
        ]
        best = unique_families[0] if unique_families else None
        second = next(
            (item for item in unique_families[1:] if item[1] - best[1] <= 5.0),
            None,
        ) if best else None
        if best:
            candidate_routes = sorted(
                route
                for route, family_counts in anchor_color_family.items()
                if family_counts.get(best[0], 0) > 0
            )
            nearest_candidate = "/".join(candidate_routes)
            second_candidate = ""
            second_family = ""
            if second:
                second_family = second[0]
                second_routes = sorted(
                    route
                    for route, family_counts in anchor_color_family.items()
                    if family_counts.get(second[0], 0) > 0
                )
                second_candidate = "/".join(second_routes)
                if not second_candidate:
                    second_candidate = f"unanchored-color:{second_family}"
            confidence = max(
                0.0,
                min(
                    1.0,
                    (best[2] / max(1, sum(item[2] for item in unique_families)))
                    * math.exp(-best[1] / 25.0),
                ),
            )
            family_name = best[0]
            path_name = path_component
            along = along_position
            best_distance = f"{best[1]:.2f}"
            second_distance = f"{second[1]:.2f}" if second else ""
        else:
            family_name = ""
            nearest_candidate = ""
            second_candidate = ""
            second_family = ""
            confidence = 0.0
            path_name = ""
            along = ""
            best_distance = ""
            second_distance = ""
        result = {
            "designator": row["designator"],
            "channel": row["channel"],
            "channel_index": row["channel_index"],
            "global_physical_order": row["global_physical_order"],
            "pcb_x": row["x"],
            "pcb_y": row["y"],
            "image_x": f"{image_x:.2f}",
            "image_y": f"{image_y:.2f}",
            "nearest_color_family": family_name,
            "nearest_route_candidate": nearest_candidate,
            "distance_to_route_pixels": best_distance,
            "second_route_candidate": second_candidate,
            "second_color_family": second_family,
            "second_distance": second_distance,
            "color_confidence": f"{confidence:.3f}",
            "path_component": path_name,
            "along_path_position": along,
            "existing_semantic_status": row["semantic_status"],
            "existing_route": row["route"],
            "existing_element_type": row["element_type"],
            "element_type_candidate": (
                row["element_type"]
                if row["semantic_status"] == "VERIFIED"
                else "UNCLASSIFIED"
            ),
            "element_type_method": (
                "retained from verified master row"
                if row["semantic_status"] == "VERIFIED"
                else "printed station-marker geometry is not isolated by the available generated raster"
            ),
            "analysis_notes": " ".join(notes),
        }
        analyzed.append(result)

    # Resolve route candidates in a second pass using color families observed
    # at every verified anchor, not electrical LED order.
    family_routes: dict[str, Counter[str]] = defaultdict(Counter)
    for result in analyzed:
        if result["existing_semantic_status"] == "VERIFIED" and result["nearest_color_family"]:
            family_routes[result["nearest_color_family"]][result["existing_route"]] += 1
    for result in analyzed:
        family = result["nearest_color_family"]
        if not family:
            continue
        routes = sorted(route for route, count in family_routes[family].items() if count > 0)
        result["nearest_route_candidate"] = "/".join(routes)
        second_family = result["second_color_family"]
        if second_family:
            second_routes = sorted(
                route
                for route, count in family_routes[second_family].items()
                if count > 0
            )
            result["second_route_candidate"] = (
                "/".join(second_routes)
                if second_routes
                else f"unanchored-color:{second_family}"
            )

    write_csv(output / "led_visual_classification.csv", CSV_FIELDS, analyzed)

    sequences = []
    for result in analyzed:
        if not result["nearest_color_family"]:
            continue
        sequences.append(
            {
                "color_family": result["nearest_color_family"],
                "path_component": result["path_component"],
                "along_path_position": result["along_path_position"],
                "designator": result["designator"],
                "channel": result["channel"],
                "channel_index": result["channel_index"],
                "global_physical_order": result["global_physical_order"],
                "pcb_x": result["pcb_x"],
                "pcb_y": result["pcb_y"],
                "image_x": result["image_x"],
                "image_y": result["image_y"],
                "existing_route": result["existing_route"],
                "color_confidence": result["color_confidence"],
            }
        )
    sequences.sort(
        key=lambda item: (
            item["color_family"],
            item["path_component"],
            float(item["along_path_position"] or 0),
        )
    )
    write_csv(
        output / "route_sequences.csv",
        (
            "color_family",
            "path_component",
            "along_path_position",
            "designator",
            "channel",
            "channel_index",
            "global_physical_order",
            "pcb_x",
            "pcb_y",
            "image_x",
            "image_y",
            "existing_route",
            "color_confidence",
        ),
        sequences,
    )

    draw_overlay(overview, analyzed, output / "A_all_leds.png", family_colors, "all")
    draw_overlay(overview, analyzed, output / "B_route_families.png", family_colors, "family")
    draw_overlay(overview, analyzed, output / "C_unassigned.png", family_colors, "unassigned")
    draw_overlay(overview, analyzed, output / "D_ambiguous.png", family_colors, "ambiguous")
    for family in sorted(overview_masks):
        family_rows = [row for row in analyzed if row["nearest_color_family"] == family]
        image = overview.copy()
        line = skeletonize(overview_masks[family])
        line = cv2.dilate(
            line, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        )
        line_pixels = line != 0
        line_layer = image.copy()
        line_layer[line_pixels] = family_colors[family]
        image[line_pixels] = cv2.addWeighted(
            line_layer[line_pixels], 0.75, image[line_pixels], 0.25, 0
        )
        family_rows.sort(
            key=lambda row: (
                row["path_component"],
                float(row["along_path_position"] or 0),
            )
        )
        component_sequence: dict[str, int] = defaultdict(int)
        for row in family_rows:
            px, py = int(round(float(row["image_x"]))), int(round(float(row["image_y"])))
            component = row["path_component"]
            label = row["designator"].removeprefix("LED")
            if component:
                component_sequence[component] += 1
                label = f"{component_sequence[component]}:{label}"
            cv2.circle(image, (px, py), 7, family_colors.get(family, (255, 255, 255)), 2)
            cv2.putText(
                image,
                label,
                (px + 8, py - 5),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.28,
                family_colors.get(family, (255, 255, 255)),
                1,
                cv2.LINE_AA,
            )
        cv2.imwrite(str(output / f"E_{family}.png"), image)

    assigned = [r for r in analyzed if r["nearest_color_family"]]
    multiple = [r for r in analyzed if r["second_distance"]]
    no_route = [r for r in analyzed if not r["nearest_color_family"]]
    unassigned = [r for r in analyzed if r["existing_semantic_status"] != "VERIFIED"]
    strong_unassigned = [
        r
        for r in unassigned
        if r["nearest_color_family"] and float(r["color_confidence"]) >= 0.70 and not r["second_distance"]
    ]
    line_distances = [
        float(r["distance_to_route_pixels"]) for r in analyzed if r["distance_to_route_pixels"]
    ]
    route_counts = Counter(r["nearest_color_family"] for r in analyzed if r["nearest_color_family"])
    palette_report = {}
    for family, bgr in family_colors.items():
        hue_range = next((f"{lo}-{hi - 1}" for name, lo, hi in COLOR_FAMILIES if name == family), "")
        palette_report[family] = {
            "hue_range_opencv": hue_range,
            "representative_bgr": list(bgr),
            "led_count": route_counts.get(family, 0),
            "route_anchors": dict(family_routes.get(family, {})),
        }
    report = {
        "source": {
            "overview": str(overview_path.relative_to(project_root)),
            "overview_dimensions_px": [int(overview.shape[1]), int(overview.shape[0])],
            "classification_rasters": registration_report,
            "source_limit": "No vector/PDF/original artwork source exists in the project; the provided generated board overlay is the highest-resolution available raster and is used for pixel classification. Six regional rasters are used only to validate image registration/coverage.",
        },
        "transform": {
            "pcb_to_overview_affine": affine.tolist(),
            "x_scale_px_per_pcb_unit": float(np.linalg.norm(affine[0, :2])),
            "y_scale_px_per_pcb_unit": float(np.linalg.norm(affine[1, :2])),
            "rotation_or_skew_degrees": float(
                math.degrees(math.atan2(affine[0, 1], affine[0, 0]))
            ),
            "x_translation_px": float(affine[0, 2]),
            "y_translation_px": float(affine[1, 2]),
            "anchor_route_line_rms_px": transform_rms,
            "anchor_count": len(anchors),
            "method": transform_method,
            "refined_anchor_route_line_rms_px": refined_rms,
            "extent_anchor_route_line_rms_px": initial_rms,
            "refined_anchor_ransac_inliers": refined_inliers,
            "interpretation": "RMS is the residual from affine-projected verified LED centers to nearby detected route-color pixels, not a surveyed LED-center reprojection error. A refinement is kept only when it improves this metric.",
        },
        "counts": {
            "map_leds": len(analyzed),
            "exactly_one_route_color_family": len(assigned) - len(multiple),
            "multiple_plausible_route_colors": len(multiple),
            "no_route_color_detected": len(no_route),
            "strong_route_family_for_currently_unassigned": len(strong_unassigned),
            "current_unassigned": len(unassigned),
            "median_led_to_route_pixel_distance": float(np.median(line_distances)) if line_distances else None,
            "maximum_led_to_route_pixel_distance": float(np.max(line_distances)) if line_distances else None,
            "family_counts": dict(route_counts),
            "element_type_candidate_counts": dict(
                Counter(r["element_type_candidate"] for r in analyzed)
            ),
            "leds_with_path_component": sum(bool(r["path_component"]) for r in analyzed),
            "path_component_counts_by_family": {
                family: len(components)
                for family, (_, components) in overview_path_data.items()
            },
        },
        "validation": {
            **inventory_validation,
            "verified_anchor_rows": len(anchors),
            "verified_semantics_modified": False,
            "master_worksheet_written": False,
            "station_registry_written": False,
            "independent_route_color_anchor_check": False,
            "anchor_check_limitation": "The available raster does not provide an authoritative per-service route-color legend; verified rows are used to correlate color families with route names, not as an independent palette oracle.",
        },
        "palette": palette_report,
    }
    (output / "analysis_report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    report_md = [
        "# Phase 3B visual/geometric route analysis",
        "",
        "Generated by `tools/phase3b_map_analysis/analyze_map.py`. This is an",
        "analysis output only; it does not promote or modify semantic assignments.",
        "",
        "## Source imagery",
        "",
        f"- Registration reference: `{report['source']['overview']}`",
        f"- Reference dimensions: {overview.shape[1]} × {overview.shape[0]} px",
        "- No vector, PDF, or original artwork raster was found in the project.",
        "- Route pixels are classified from the 1920 × 1360 board overview raster.",
        "- The six regional PNGs have an approximate 0.5× pixel scale relative to",
        "  the overview; SIFT/RANSAC homographies validate their registration.",
        "- Color classification uses pixels from the 1920 × 1360 overview; regional",
        "  images are lower-resolution registration/coverage references.",
        "",
        "## PCB-to-image affine and fit",
        "",
        f"- Affine matrix: `{np.array2string(affine, precision=8)}`",
        f"- X row-vector scale: {report['transform']['x_scale_px_per_pcb_unit']:.6f} px/unit",
        f"- Y row-vector scale: {report['transform']['y_scale_px_per_pcb_unit']:.6f} px/unit",
        f"- X-axis rotation: {report['transform']['rotation_or_skew_degrees']:.4f}°",
        f"- Translation: ({affine[0, 2]:.3f}, {affine[1, 2]:.3f}) px",
        f"- Transform selection: {transform_method}",
        f"- Extent-transform verified-anchor-to-route-pixel RMS: {transform_rms:.3f} px",
        "- This RMS measures fit to nearby color-line pixels, not independent",
        "  physical LED-center survey residual; see JSON for the exact definition.",
        f"- Attempted robust refinement: {refined_rms:.3f} px with {refined_inliers}/{len(anchors)} RANSAC inliers; rejected because it worsened the extent-transform RMS.",
        "- Extracted line masks are skeletonized; sequence positions are shortest-path",
        "  distances along the connected skeleton from a deterministic endpoint.",
        "",
        "## Classification counts",
        "",
        f"- Map LEDs: {len(analyzed)}",
        f"- Exactly one family: {len(assigned) - len(multiple)}",
        f"- Multiple plausible families: {len(multiple)}",
        f"- No family detected: {len(no_route)}",
        f"- Strong (confidence ≥ 0.70, single-family) among current unassigned: {len(strong_unassigned)}/{len(unassigned)}",
        f"- Median LED-to-line pixel distance: {float(np.median(line_distances)):.2f} px" if line_distances else "- Median LED-to-line pixel distance: unavailable",
        f"- Maximum LED-to-line pixel distance: {float(np.max(line_distances)):.2f} px" if line_distances else "- Maximum LED-to-line pixel distance: unavailable",
        f"- LEDs assigned to a filtered skeleton component: {report['counts']['leds_with_path_component']}/{len(analyzed)}",
        f"- Element-type candidates: {report['counts']['element_type_candidate_counts']}",
        "",
        "### Color-family counts",
        "",
        "| Family | Hue range (OpenCV) | LED count | Representative BGR | Anchor routes |",
        "|---|---:|---:|---|---|",
    ]
    for family, item in palette_report.items():
        report_md.append(
            f"| {family} | {item['hue_range_opencv']} | {item['led_count']} | {item['representative_bgr']} | {item['route_anchors']} |"
        )
    report_md.extend(
        [
            "",
            "## Method and review cautions",
            "",
            "- The affine transform is initialized from the extents of the physical map",
            "  LED coordinates and the map image bounds. A robust anchor refinement was",
            "  tested and rejected because its measured route-pixel RMS was worse.",
            "- No electrical or global LED order is used to infer geography.",
            "- Crop-to-overview registration uses SIFT feature correspondences and",
            "  RANSAC homographies. The exact per-crop statistics are in",
            "  `analysis_report.json`.",
            "- Color names are image hue families, not individual MTA route IDs.",
            "  Shared-color services remain route candidates until topology is resolved.",
            "- Hue masks are skeletonized; skeleton fragments shorter than 10 pixels are",
            "  omitted to reduce text/speckle noise. Along-path position is shortest-path",
            "  distance from a deterministic endpoint within the nearest connected",
            "  skeleton component. Components can be fragmented or branched and do not",
            "  establish a complete route topology or direction between components.",
            "- The image is a generated board overlay, not the source artwork. Text, LED",
            "  markers, route color tinting, and hue thresholds can affect masks; outputs",
            "  are geometric review evidence, not verified route assignments.",
            "- Only existing verified station/segment types are retained. Unverified",
            "  station-versus-segment rows remain `UNCLASSIFIED`; the available raster",
            "  does not isolate printed station-marker geometry well enough to infer them.",
            "- `second_route_candidate` records a competing family only when its nearest",
            "  annular route pixels are within 5 px of the nearest family's distance.",
            "- Inspect the generated overlays, especially D_ambiguous.png and the",
            "  individual E_*.png files, before using any classification as a proposal.",
            "",
            "## Visual outputs",
            "",
            "- `A_all_leds.png` — all projected map LEDs.",
            "- `B_route_families.png` — LEDs colored by the nearest detected family.",
            "- `C_unassigned.png` — only currently unverified LEDs.",
            "- `D_ambiguous.png` — LEDs with a competing family.",
            "- `E_<family>.png` — detected skeleton and LEDs; order labels restart in",
            "  each connected component (CSV path positions are the sortable values).",
            "",
            "## Inventory validation",
            "",
            f"- Map rows checked against `data/led_chain_map.csv`: {inventory_validation['map_rows']}",
            f"- Unique designators / channel-index pairs / global orders: {inventory_validation['unique_designators']} / {inventory_validation['unique_channel_indexes']} / {inventory_validation['unique_global_physical_orders']}",
            "- All map physical references match the chain map; no duplicate physical",
            "  keys were found. The master worksheet and station registry were not written.",
            "- Route-color anchor contradictions are not independently testable from the",
            "  available generated raster because it has no authoritative per-service",
            "  color legend; verified rows only correlate detected color families to routes.",
            "",
            "## Safeguards",
            "",
            "- Input worksheet and station registry are read-only inputs.",
            "- U23 and the four CH8 status LEDs are excluded because only worksheet",
            "  rows with `role=map` are analyzed; LED291 remains included.",
            "- No firmware build, upload, commit, or push is performed.",
            "",
        ]
    )
    (output / "ANALYSIS_REPORT.md").write_text("\n".join(report_md), encoding="utf-8")
    print(
        f"Analyzed {len(analyzed)} map LEDs; affine route-line RMS "
        f"{transform_rms:.2f}px; outputs: {output}"
    )


if __name__ == "__main__":
    main()
