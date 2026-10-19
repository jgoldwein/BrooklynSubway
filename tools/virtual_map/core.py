#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Build and run the host-only virtual-map bridge."""

import csv
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
HOST_CPP = HERE / "host.cpp"
COORDINATES = ROOT / "docs/mapping/phase3b/led_positions_v11.csv"
WORKSHEET = ROOT / "docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv"
GENERATED = HERE / "generated/metadata.json"
SOURCES = [
    ROOT / "src/generated/semantic_tables.cpp",
    ROOT / "src/led_map.cpp",
    ROOT / "src/semantic_api.cpp",
    ROOT / "src/train_frame.cpp",
    ROOT / "src/rgb_renderer.cpp",
    ROOT / "src/operational_display.cpp",
]


def compiler():
    sys.path.insert(0, str(ROOT / "tools"))
    import phase4e_host

    return phase4e_host.compiler()


def build(executable):
    command = (
        compiler()
        + [
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-O1",
            "-fno-exceptions",
            "-DBROOKLYN_TARGET_PCB=1",
            f"-I{ROOT / 'include'}",
            f"-I{ROOT / 'tools/host_tests'}",
            str(HOST_CPP),
            *map(str, SOURCES),
            "-o",
            str(executable),
        ]
    )
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise RuntimeError("host simulator bridge compilation failed")


def load_coordinates():
    with COORDINATES.open("r", newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    positions = {}
    for row in rows:
        name = row["designator"]
        if name in positions:
            raise ValueError(f"duplicate geometry entry: {name}")
        x, y, angle = (float(row[field]) for field in ("x", "y", "angle"))
        if not all(math.isfinite(value) for value in (x, y, angle)):
            raise ValueError(f"non-finite geometry for {name}")
        positions[name] = {"x": x, "y": y, "angle": angle}
    if len(positions) != 306:
        raise ValueError(f"expected 306 coordinate rows, found {len(positions)}")
    return positions


def generated_metadata(executable):
    result = subprocess.run(
        [str(executable), "metadata"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=True,
    )
    metadata = json.loads(result.stdout)
    positions = load_coordinates()
    pixels = metadata["pixels"]
    if len(pixels) != 306 or len({p["designator"] for p in pixels}) != 306:
        raise ValueError("compiled firmware metadata does not identify 306 unique LEDs")
    if {p["designator"] for p in pixels} != set(positions):
        raise ValueError("PCB coordinate designators do not match the compiled chain")
    for pixel in pixels:
        pixel.update(positions[pixel["designator"]])

    with WORKSHEET.open("r", newline="", encoding="utf-8-sig") as source:
        worksheet_rows = list(csv.DictReader(source))
    if len(worksheet_rows) != 306:
        raise ValueError(f"expected 306 semantic worksheet rows, found {len(worksheet_rows)}")
    firmware = {pixel["designator"]: pixel for pixel in pixels}
    for row in worksheet_rows:
        designator = row["designator"]
        pixel = firmware.get(designator)
        if pixel is None:
            raise ValueError(f"worksheet LED is absent from compiled firmware: {designator}")
        if (int(row["channel"]), int(row["channel_index"]),
                int(row["global_physical_order"])) != (
                pixel["channel"], pixel["channelIndex"], pixel["globalIndex"]):
            raise ValueError(f"physical identity mismatch for {designator}")
        geometry = positions[designator]
        if tuple(float(row[field]) for field in ("x", "y", "angle")) != (
                geometry["x"], geometry["y"], geometry["angle"]):
            raise ValueError(f"worksheet geometry mismatch for {designator}")
        if pixel["semanticType"] == "STATUS":
            suffix = {"SYSTEM": "system", "NETWORK": "network",
                      "MTA": "mta_feed", "MODE": "mode"}[pixel["statusRole"]]
            expected_role = "status_" + suffix
            if row["role"].lower() != expected_role or row["element_type"] != "STATUS":
                raise ValueError(f"status role mismatch for {designator}")
            continue
        if row["role"] != "map" or row["semantic_status"] != "VERIFIED":
            raise ValueError(f"map semantic row is not verified: {designator}")
        if row["route"] != pixel["route"] or row["element_type"] != pixel["semanticType"]:
            raise ValueError(f"route/type mismatch for {designator}")
        if pixel["semanticType"] == "STATION":
            matches = (int(row["station_id"]) == pixel["stationId"] and
                       row["station_name"] == pixel["stationName"])
        else:
            matches = (
                int(row["from_station_id"]) == pixel["endpointAId"] and
                int(row["to_station_id"]) == pixel["endpointBId"] and
                row["from_station"] == pixel["endpointAName"] and
                row["to_station"] == pixel["endpointBName"]
            )
        if not matches:
            raise ValueError(f"semantic identity mismatch for {designator}")

    inputs = [
        ROOT / "include/generated/semantic_tables.h",
        ROOT / "src/generated/semantic_tables.cpp",
        ROOT / "include/led_map.h",
        ROOT / "src/led_map.cpp",
        ROOT / "include/semantic_api.h",
        ROOT / "src/semantic_api.cpp",
        ROOT / "include/board_pins.h",
        COORDINATES,
        WORKSHEET,
        HOST_CPP,
        Path(__file__).resolve(),
    ]
    metadata["source"] = {
        "generatorVersion": 1,
        "semanticAuthority": "compiled firmware semantic tables and led_map",
        "semanticCrossCheck": "docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv",
        "geometryAuthority": "docs/mapping/phase3b/led_positions_v11.csv",
        "geometryCoordinates": "exact manufactured V1.1 PCB X/Y/angle",
        "inputSha256": {
            path.relative_to(ROOT).as_posix(): hashlib.sha256(
                path.read_bytes()
            ).hexdigest()
            for path in inputs
        },
    }
    return metadata


def write_generated(executable):
    metadata = generated_metadata(executable)
    content = json.dumps(
        metadata, ensure_ascii=False, sort_keys=True, indent=2
    ) + "\n"
    GENERATED.parent.mkdir(parents=True, exist_ok=True)
    GENERATED.write_text(content, encoding="utf-8", newline="\n")
    return content


def check_generated(executable):
    expected = (
        json.dumps(
            generated_metadata(executable), ensure_ascii=False, sort_keys=True, indent=2
        )
        + "\n"
    )
    actual = GENERATED.read_text(encoding="utf-8")
    if actual != expected:
        raise ValueError(
            "generated metadata is stale; run `python tools/virtual_map/run.py --export`"
        )
    return actual


def run_renderer(executable, request):
    wifi = {
        "Disabled": 0,
        "Unavailable": 1,
        "Connecting": 2,
        "Associated": 3,
        "TimeSyncing": 4,
    }
    states = {
        "WaitingForData": 0,
        "Fresh": 1,
        "DegradedHold": 2,
        "DegradedPartial": 3,
        "StaleHold": 4,
        "NoData": 5,
    }
    occupancy = request["trainFrame"]["occupancy"]
    frame_timestamp = request.get("frameTimestamp", "0")
    if isinstance(frame_timestamp, str):
        timestamp = 0
    else:
        timestamp = frame_timestamp
    status = request.get("status", {})
    header = "{} {} {} {} {} {}\n".format(
        request["brightness"],
        request["routeVisibilityMask"],
        states[request["displayState"]],
        int(status.get("systemHealthy", True)),
        wifi[status.get("wifiState", "Associated")],
        int(status.get("provisioning", False)),
    )
    body = header + " ".join(str(value) for value in occupancy) + "\n"
    result = subprocess.run(
        [str(executable), "render", str(timestamp)],
        input=body,
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=True,
    )
    rendered = json.loads(result.stdout)
    rendered["frameTimestamp"] = frame_timestamp
    rendered["trains"] = request["trainFrame"].get("trains", [])
    rendered["trainFrame"] = {
        "occupancy": rendered["occupancy"],
        "resolvedTrains": rendered["resolvedTrains"],
        "occupiedLeds": rendered["occupiedLeds"],
        "collisionLeds": rendered["collisionLeds"],
        "trains": rendered["trains"],
    }
    return rendered
