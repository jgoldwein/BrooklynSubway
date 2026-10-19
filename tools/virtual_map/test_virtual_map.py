#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Host-only Phase 4X metadata, renderer, and interchange regression tests."""

import json
import tempfile
from pathlib import Path

from core import GENERATED, build, check_generated, generated_metadata, run_renderer
from run import validate_render_request

CHECKS = 0
FAILURES = 0


def check(condition, message):
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print("FAIL:", message)


def rgb_black(pixel):
    return pixel == {"r": 0, "g": 0, "b": 0}


def request(occupancy, state="Fresh", brightness=64, mask=0x3FFFF, **kwargs):
    return {
        "schemaVersion": 1,
        "frameTimestamp": "2026-10-18T00:00:00Z",
        "trainFrame": {"occupancy": occupancy, "trains": kwargs.get("trains", [])},
        "displayState": state,
        "brightness": brightness,
        "routeVisibilityMask": mask,
        "status": kwargs.get(
            "status",
            {"systemHealthy": True, "wifiState": "Associated", "provisioning": False},
        ),
    }


def main():
    with tempfile.TemporaryDirectory(prefix="virtual-map-test-") as temporary:
        executable = Path(temporary) / "virtual_map_host.exe"
        build(executable)
        metadata = generated_metadata(executable)
        canonical = json.dumps(metadata, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
        check(GENERATED.read_text(encoding="utf-8") == canonical, "generated metadata matches current firmware tables and geometry")
        check(check_generated(executable) is not None, "metadata check mode accepts committed export")
        second = json.dumps(generated_metadata(executable), ensure_ascii=False, sort_keys=True, indent=2) + "\n"
        check(canonical == second, "generated metadata is deterministic")

        pixels = metadata["pixels"]
        check(len(pixels) == 306, "exact addressable count")
        map_pixels = [p for p in pixels if p["semanticType"] != "STATUS"]
        status_pixels = [p for p in pixels if p["semanticType"] == "STATUS"]
        check(len(map_pixels) == 302, "map pixel count")
        check(len(status_pixels) == 4, "status pixel count")
        check(sum(p["semanticType"] == "STATION" for p in map_pixels) == 276, "station semantic count")
        check(sum(p["semanticType"] == "SEGMENT" for p in map_pixels) == 26, "segment semantic count")
        check(sum(not p.get("route") for p in map_pixels) == 0, "no unresolved map semantics")
        check(len({p["globalIndex"] for p in pixels}) == 306, "unique global indices")
        check(len({p["designator"] for p in pixels}) == 306, "unique PCB designators")
        check(all(p["channel"] in range(1, 9) and p["channelIndex"] >= 0 for p in pixels), "valid channel addresses")
        check(all(isinstance(p["x"], (int, float)) and isinstance(p["y"], (int, float)) for p in pixels), "finite geometry values present")
        check(all(p["x"] == p["x"] and p["y"] == p["y"] for p in pixels), "no NaN geometry")
        check(all(abs(p["x"]) < float("inf") and abs(p["y"]) < float("inf") for p in pixels), "no infinite geometry")
        check(all("angle" in p and abs(p["angle"]) < float("inf") for p in pixels), "all PCB placement angles are finite")
        counts = [sum(p["channel"] == ch for p in pixels) for ch in range(1, 9)]
        expected_counts = [37, 38, 37, 37, 42, 37, 38, 40]
        check(counts == expected_counts, "exact channel pixel counts")
        check(all(
            {p["channelIndex"] for p in pixels if p["channel"] == ch} == set(range(n))
            for ch, n in enumerate(expected_counts, 1)
        ), "channel indices are complete and contiguous")
        check([c["gpio"] for c in metadata["channels"]] == [4, 5, 6, 7, 10, 11, 12, 13], "exact channel GPIO mapping")
        check([r["name"] for r in metadata["routes"]] == ["2", "3", "4", "5", "A", "B", "C", "D", "F", "G", "J", "L", "M", "N", "Q", "R", "S", "Z"], "exact canonical route bit order")
        check([r["maskBit"] for r in metadata["routes"]] == list(range(18)), "canonical route mask bit positions")
        statuses = sorted(status_pixels, key=lambda p: p["channelIndex"])
        check([(p["globalIndex"], p["channel"], p["channelIndex"], p["designator"], p["statusRole"]) for p in statuses] == [
            (266, 8, 0, "LED252", "SYSTEM"),
            (267, 8, 1, "LED251", "NETWORK"),
            (268, 8, 2, "LED306", "MTA"),
            (269, 8, 3, "LED307", "MODE"),
        ], "status indices/roles")
        check(all(p["designator"] != "U23" for p in pixels), "U23 excluded from addressable pixels")

        by_designator = {p["designator"]: p for p in map_pixels}
        spots = {
            "LED89": ("L", "STATION", "Bushwick Av-Aberdeen St"),
            "LED299": ("L", "SEGMENT", 83, 50),
            "LED96": ("G", "STATION", "Lorimer St"),
            "LED308": ("D", "SEGMENT", "Grand St", "Atlantic Av-Barclays Ctr"),
            "LED309": ("B", "SEGMENT", "Grand St", "DeKalb Av"),
            "LED310": ("Q", "SEGMENT", "Canal St", "DeKalb Av"),
            "LED311": ("N", "SEGMENT", "Canal St", "Atlantic Av-Barclays Ctr"),
            "LED291": ("F", "SEGMENT", "York St", "East Broadway"),
            "LED304": ("S", "SEGMENT", "Botanic Garden", "Park Pl"),
            "LED287": ("A", "SEGMENT", "Hoyt-Schermerhorn Sts", "Nostrand Av"),
            "LED285": ("B", "SEGMENT", "Grand St", "DeKalb Av"),
            "LED286": ("D", "SEGMENT", "Grand St", "Atlantic Av-Barclays Ctr"),
            "LED187": ("G", "SEGMENT", "Greenpoint Av", "Nassau Av"),
            "LED288": ("C", "SEGMENT", "Lafayette Av", "Clinton-Washington Avs"),
            "LED300": ("2", "SEGMENT", "Franklin Av-Medgar Evers College", "President St-Medgar Evers College"),
            "LED301": ("3", "SEGMENT", "Franklin Av-Medgar Evers College", "Nostrand Av"),
            "LED303": ("5", "SEGMENT", "Atlantic Av-Barclays Ctr", "Franklin Av-Medgar Evers College"),
            "LED305": ("S", "SEGMENT", "Broad Channel", "Beach 90 St"),
            "LED292": ("N", "SEGMENT", "Atlantic Av-Barclays Ctr", "36 St"),
            "LED295": ("G", "SEGMENT", "Lorimer St", "Broadway"),
            "LED297": ("M", "SEGMENT", "Hewes St", "Lorimer St"),
        }
        for designator, expected in spots.items():
            p = by_designator.get(designator, {})
            if expected[1] == "STATION":
                actual = (p.get("route"), p.get("semanticType"), p.get("stationName"))
            elif designator == "LED299":
                actual = (p.get("route"), p.get("semanticType"), p.get("endpointAId"), p.get("endpointBId"))
            else:
                actual = (p.get("route"), p.get("semanticType"), p.get("endpointAName"), p.get("endpointBName"))
            check(actual == expected, f"semantic spot check {designator}: {actual!r}")
        delancey = [by_designator.get("LED" + str(n), {}) for n in range(255, 260)]
        check([p.get("route") for p in delancey] == ["F", "M", "J", "Z", "F"], "Delancey cluster route semantics preserved")
        check(all(p.get("semanticType") == "STATION" and p.get("stationId") == 185 for p in delancey[:4]), "Delancey F/M/J/Z station cluster semantics preserved")

        station = next(p for p in map_pixels if p["semanticType"] == "STATION")
        segment = next(p for p in map_pixels if p["semanticType"] == "SEGMENT")
        one = [0] * 306
        one[station["globalIndex"]] = 1
        fresh = run_renderer(executable, request(one, "Fresh", 255))
        check(fresh["displayState"] == "Fresh" and fresh["occupancy"][station["globalIndex"]] == 1, "actual Fresh pipeline output")
        check(fresh["pixels"][station["globalIndex"]] != {"r": 0, "g": 0, "b": 0}, "station route color rendered by firmware")
        check(len(fresh["pixels"]) == 306 and len(fresh["occupancy"]) == 306, "frame JSON has exactly 306 RGB and occupancy values")
        check(len(fresh["routeVisible"]) == 306 and len(fresh["filteredByRoute"]) == 306, "route diagnostics cover all 306 indices")
        scaled = run_renderer(executable, request(one, "Fresh", 64))
        check(scaled["pixels"][station["globalIndex"]] != fresh["pixels"][station["globalIndex"]], "brightness changes firmware RGB output")
        fresh_default_brightness = scaled
        all_off = run_renderer(executable, request([0] * 306, "Fresh", 255))
        check(all(rgb_black(p) for p in all_off["pixels"][:266] + all_off["pixels"][270:]), "all-map-off scenario has no map RGB")

        seg_occupancy = [0] * 306
        seg_occupancy[segment["globalIndex"]] = 1
        segment_result = run_renderer(executable, request(seg_occupancy))
        check(segment_result["renderedSegmentLeds"] == 1 and segment_result["renderedStationLeds"] == 0, "single segment rendered by real renderer")
        routes = []
        seen = set()
        for pixel in map_pixels:
            if pixel["route"] not in seen:
                routes.append(pixel)
                seen.add(pixel["route"])
            if len(routes) == 5:
                break
        multi = [0] * 306
        for p in routes:
            multi[p["globalIndex"]] = 1
        multi_result = run_renderer(executable, request(multi))
        check(multi_result["renderedLeds"] == len(routes), "multiple active routes rendered")

        collision = [0] * 306
        collision[station["globalIndex"]] = 2
        trains = [{"trainId": "A", "route": station["route"], "globalIndex": station["globalIndex"]},
                  {"trainId": "B", "route": station["route"], "globalIndex": station["globalIndex"]}]
        collision_result = run_renderer(executable, request(collision, trains=trains))
        check(collision_result["collisionLeds"] == 1 and collision_result["occupancy"][station["globalIndex"]] == 2, "collision count comes from TrainFrame")
        check(collision_result["pixels"][station["globalIndex"]] == fresh_default_brightness["pixels"][station["globalIndex"]], "collision color remains firmware-selected route RGB")
        check(collision_result["trains"] == trains, "train ID/route metadata preserved for inspector")

        bit = next(r["maskBit"] for r in metadata["routes"] if r["name"] == station["route"])
        hidden = run_renderer(executable, request(one, mask=0x3FFFF & ~(1 << bit)))
        check(rgb_black(hidden["pixels"][station["globalIndex"]]), "route visibility suppresses occupied map LED")
        check(hidden["filteredByRoute"][station["globalIndex"]], "route filter state comes from firmware renderer bridge")
        check(hidden["routeVisible"][station["globalIndex"]] is False, "firmware reports hidden route")
        no_routes = run_renderer(executable, request([1 if p["semanticType"] != "STATUS" else 0 for p in pixels], mask=0))
        check(all(rgb_black(no_routes["pixels"][p["globalIndex"]]) for p in map_pixels), "all-hidden route mask blacks map LEDs")

        states = ["WaitingForData", "Fresh", "DegradedHold", "DegradedPartial", "StaleHold", "NoData"]
        state_results = {state: run_renderer(executable, request(one, state)) for state in states}
        check([state_results[s]["displayState"] for s in states] == states, "all Phase 4M operational states execute")
        check(all(rgb_black(state_results["NoData"]["pixels"][p["globalIndex"]]) for p in map_pixels), "NoData controller clears map")
        for state in ("Fresh", "DegradedHold", "DegradedPartial", "StaleHold"):
            check(state_results[state]["pixels"][station["globalIndex"]] == fresh_default_brightness["pixels"][station["globalIndex"]], f"status state {state} does not alter map RGB")

        alternate_status = run_renderer(executable, request(one, "Fresh", status={
            "systemHealthy": False, "wifiState": "TimeSyncing", "provisioning": True
        }))
        check([alternate_status["pixels"][i] for i in range(266, 270)] != [fresh["pixels"][i] for i in range(266, 270)], "status inputs change status overlay")
        check([alternate_status["pixels"][i] for i in range(266)] + [alternate_status["pixels"][i] for i in range(270, 306)] ==
              [fresh_default_brightness["pixels"][i] for i in range(266)] + [fresh_default_brightness["pixels"][i] for i in range(270, 306)], "status overlay never changes map pixels")
        hidden_status = run_renderer(executable, request(one, mask=0))
        check([hidden_status["pixels"][i] for i in range(266, 270)] == [fresh_default_brightness["pixels"][i] for i in range(266, 270)], "route filtering never changes status LEDs")
        check(any(rgb_black(hidden_status["pixels"][i]) for i in range(266)), "route filtering affects map pixels")

        snapshot = {
            "schemaVersion": 1,
            "frameTimestamp": fresh["frameTimestamp"],
            "pixels": fresh["pixels"],
            "trainFrame": fresh["trainFrame"],
            "operational": {"displayState": fresh["displayState"]},
            "brightness": fresh["brightness"],
            "routeVisibilityMask": fresh["routeVisibilityMask"],
        }
        encoded = json.dumps(snapshot, sort_keys=True)
        parsed = json.loads(encoded)
        check(parsed["schemaVersion"] == 1 and len(parsed["pixels"]) == 306, "frame interchange JSON schema round-trip")
        again = run_renderer(executable, request(one, "Fresh", 255))
        check(json.dumps(again, sort_keys=True) == json.dumps(fresh, sort_keys=True), "firmware frame export is deterministic")

        valid_request = request(one)
        check(validate_render_request(valid_request, metadata) is valid_request, "render API accepts valid frame input")
        invalid_cases = []
        bad_version = dict(valid_request)
        bad_version["schemaVersion"] = 2
        invalid_cases.append((bad_version, "unsupported schema version"))
        bad_count = request(one)
        bad_count["trainFrame"]["occupancy"] = one[:-1]
        invalid_cases.append((bad_count, "incomplete occupancy array"))
        bad_brightness = request(one)
        bad_brightness["brightness"] = 256
        invalid_cases.append((bad_brightness, "out-of-range brightness"))
        invalid_mask = request(one, mask=0x40000)
        invalid_cases.append((invalid_mask, "noncanonical route bits"))
        status_occupancy = request(one.copy())
        status_occupancy["trainFrame"]["occupancy"][266] = 1
        invalid_cases.append((status_occupancy, "status LED occupancy"))
        for invalid_request, description in invalid_cases:
            try:
                validate_render_request(invalid_request, metadata)
                check(False, "render API rejects " + description)
            except ValueError:
                check(True, "render API rejects " + description)

        representative_status = run_renderer(
            executable,
            request(one, "DegradedHold", status={
                "systemHealthy": False,
                "wifiState": "TimeSyncing",
                "provisioning": True,
            }),
        )
        check(
            all(not rgb_black(representative_status["pixels"][p["globalIndex"]])
                for p in status_pixels),
            "representative state renders all four status LEDs",
        )

    print(f"Phase 4X virtual-map: {CHECKS} checks, {FAILURES} failures")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
