#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Offline tests for raw capture, validation, firmware replay, and export."""

import hashlib
import importlib.util
import json
import sys
import tempfile
import urllib.error
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from core import (
    FEED_STATUSES,
    FRAME_SCHEMA,
    FRAME_VERSION,
    MANIFEST_SCHEMA,
    MANIFEST_VERSION,
    atomic_write,
    bridge_feeds,
    build_bridge,
    canonical_json,
    export_replay,
    load_manifest,
    run_bridge,
    sha256_bytes,
    validate_replay_schema,
)
from record import fetch, record
from virtual_map.run import load_replay_export

REPLAY_CLI_SPEC = importlib.util.spec_from_file_location(
    "brooklyn_replay_cli", ROOT / "tools/replay/replay.py"
)
REPLAY_CLI = importlib.util.module_from_spec(REPLAY_CLI_SPEC)
REPLAY_CLI_SPEC.loader.exec_module(REPLAY_CLI)

CHECKS = 0
FAILURES = 0


def check(condition, description):
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print("FAIL:", description)


def varint(value):
    result = bytearray()
    while value >= 0x80:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result.append(value)
    return bytes(result)


def scalar(field_number, value):
    return varint(field_number << 3) + varint(value)


def text(field_number, value):
    encoded = value.encode("ascii")
    return varint((field_number << 3) | 2) + varint(len(encoded)) + encoded


def message(field_number, value):
    return varint((field_number << 3) | 2) + varint(len(value)) + value


def make_feed(timestamp, vehicles=(), trip_updates=()):
    header = text(1, "1.0") + scalar(2, 0) + scalar(3, timestamp)
    entities = []
    for trip_id, route, stop, vehicle_id, status in trip_updates:
        descriptor = text(1, trip_id) + text(3, "20261019") + text(5, route)
        stop_update = text(4, stop)
        update = message(1, descriptor) + message(2, stop_update)
        entities.append(message(2, text(1, "tu-" + trip_id) + message(3, update)))
    for trip_id, route, stop, vehicle_id, status, _next_stop in vehicles:
        descriptor = text(1, trip_id) + text(3, "20261019") + text(5, route)
        vehicle = (
            message(1, descriptor)
            + scalar(4, status)
            + scalar(5, timestamp)
            + text(7, stop)
            + message(8, text(1, vehicle_id))
        )
        entities.append(message(2, text(1, "vp-" + trip_id) + message(4, vehicle)))
    return message(1, header) + b"".join(entities)


def create_session(root, feeds, cycles, started_unix=1800000000):
    root.mkdir(parents=True, exist_ok=False)
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "schemaVersion": MANIFEST_VERSION,
        "sessionId": root.name,
        "startedUtc": "2027-01-15T08:00:00Z",
        "intervalSeconds": 30,
        "requestedCycles": len(cycles),
        "feeds": [{"id": feed["id"]} for feed in feeds],
        "cycles": [],
    }
    for index, cycle in enumerate(cycles):
        elapsed, feed_statuses, vehicles = cycle
        unix = (started_unix + elapsed // 1000) & 0xFFFFFFFF
        cycle_feed_slots = []
        for feed in feeds:
            feed_id = feed["id"]
            status = feed_statuses.get(feed_id, "SUCCESS")
            slot = {
                "feedId": feed_id,
                "status": status,
                "result": "HTTP_OK" if status == "SUCCESS" else status,
                "durationMs": 0,
                "fetchOffsetMs": elapsed,
            }
            if status == "SUCCESS":
                if feed_id == "G":
                    feed_vehicles = vehicles
                    updates = [
                        (trip_id, route, next_stop, vehicle_id, state)
                        for trip_id, route, stop, vehicle_id, state, next_stop in vehicles
                        if next_stop
                    ]
                    payload = make_feed(unix, feed_vehicles, updates)
                else:
                    payload = make_feed(unix)
                filename = f"cycle-{index:04d}-{feed_id.replace(',', '-')}.pb"
                atomic_write(root / filename, payload)
                slot.update(
                    {
                        "file": filename,
                        "bytes": len(payload),
                        "sha256": sha256_bytes(payload),
                    }
                )
            cycle_feed_slots.append(slot)
        manifest["cycles"].append(
            {
                "index": index,
                "elapsedMs": elapsed,
                "capturedUtc": f"2027-01-15T08:{elapsed // 60000:02d}:{(elapsed // 1000) % 60:02d}Z",
                "feeds": cycle_feed_slots,
            }
        )
    atomic_write(root / "manifest.json", json.dumps(
        manifest, ensure_ascii=False, sort_keys=True, indent=2
    ).encode("utf-8") + b"\n")
    return manifest


def main():
    with tempfile.TemporaryDirectory(prefix="phase4y-tests-") as temporary:
        root = Path(temporary)
        executable = root / "brooklyn_replay_host.exe"
        build_bridge(executable)
        feeds = bridge_feeds(executable)
        feed_ids = [feed["id"] for feed in feeds]
        check(len(feeds) == 8 and len(set(feed_ids)) == 8, "feed table comes from compiled firmware and has eight unique ids")
        check(all(feed["url"].startswith("https://") for feed in feeds), "authoritative feed endpoints require HTTPS")
        check([feed["id"] for feed in feeds] == ["1-7,S", "ACE", "BDFM", "G", "JZ", "NQRW", "L", "SI"], "authoritative feed order matches firmware")
        check(set(FEED_STATUSES) == {"SUCCESS", "HTTP_FAILURE", "DNS_CONNECT_FAILURE", "TIMEOUT", "OMITTED", "UNAVAILABLE"}, "all manifest fetch outcomes have defined status codes")

        timing_cycles = [
            (0, {}, [("T-G-1", "G", "G29N", "VEH-G1", 0, "G29N")]),
            (89999, {"L": "OMITTED"}, [("T-G-1", "G", "G29N", "VEH-G1", 0, "G29N")]),
            (90000, {"L": "OMITTED"}, [("T-G-1", "G", "G29N", "VEH-G1", 0, "G29N")]),
            (90001, {"L": "OMITTED"}, [("T-G-1", "G", "G29N", "VEH-G1", 0, "G29N")]),
            (100000, {}, [("T-G-1", "G", "G29N", "VEH-G1", 0, "G29N")]),
            (399999, {feed_id: "OMITTED" for feed_id in feed_ids}, []),
            (400000, {feed_id: "OMITTED" for feed_id in feed_ids}, []),
            (400001, {feed_id: "OMITTED" for feed_id in feed_ids}, []),
            (400002, {}, [("T-G-2", "G", "G29N", "VEH-G2", 0, "G29N")]),
        ]
        session = root / "session"
        create_session(session, feeds, timing_cycles)
        manifest = load_manifest(session, feeds)
        check(manifest["_startUnix"] == 1800000000, "recording UTC deterministically anchors replay Unix time")
        check(manifest["_manifestSha256"] == sha256_bytes((session / "manifest.json").read_bytes()), "manifest digest is captured")
        first_payload_path = session / manifest["cycles"][0]["feeds"][3]["file"]
        original_payload = first_payload_path.read_bytes()
        check(len(original_payload) == manifest["cycles"][0]["feeds"][3]["bytes"], "raw protobuf byte length is preserved")
        check(hashlib.sha256(original_payload).hexdigest() == manifest["cycles"][0]["feeds"][3]["sha256"], "raw protobuf SHA-256 matches manifest")
        check(original_payload == (session / manifest["cycles"][0]["feeds"][3]["file"]).read_bytes(), "raw payload is unchanged by replay preparation")

        encoded_manifest = json.loads((session / "manifest.json").read_text(encoding="utf-8"))
        unsupported = dict(encoded_manifest)
        unsupported["schemaVersion"] = 99
        atomic_write(session / "manifest.json", json.dumps(unsupported).encode("utf-8"))
        try:
            load_manifest(session, feeds)
            check(False, "unsupported capture manifest version is rejected")
        except ValueError:
            check(True, "unsupported capture manifest version is rejected")
        atomic_write(session / "manifest.json", json.dumps(encoded_manifest).encode("utf-8"))
        invalid_time = dict(encoded_manifest)
        invalid_time["startedUtc"] = "2027-01-15T09:00:00+01:00"
        atomic_write(session / "manifest.json", json.dumps(invalid_time).encode("utf-8"))
        try:
            load_manifest(session, feeds)
            check(False, "non-UTC capture timestamp is rejected")
        except ValueError:
            check(True, "non-UTC capture timestamp is rejected")
        invalid_elapsed = json.loads(json.dumps(encoded_manifest))
        invalid_elapsed["cycles"][0]["elapsedMs"] = True
        atomic_write(session / "manifest.json", json.dumps(invalid_elapsed).encode("utf-8"))
        try:
            load_manifest(session, feeds)
            check(False, "boolean elapsed value is rejected as a non-integer timestamp")
        except ValueError:
            check(True, "boolean elapsed value is rejected as a non-integer timestamp")
        escaped_payload = json.loads(json.dumps(encoded_manifest))
        escaped_payload["cycles"][0]["feeds"][3]["file"] = "../outside.pb"
        atomic_write(session / "manifest.json", json.dumps(escaped_payload).encode("utf-8"))
        try:
            load_manifest(session, feeds)
            check(False, "payload path traversal is rejected")
        except ValueError:
            check(True, "payload path traversal is rejected")
        atomic_write(session / "manifest.json", json.dumps(encoded_manifest).encode("utf-8"))
        payload_path = session / encoded_manifest["cycles"][0]["feeds"][3]["file"]
        payload_path.write_bytes(original_payload + b"\x00")
        try:
            load_manifest(session, feeds)
            check(False, "modified raw payload is rejected")
        except ValueError as error:
            check("mismatch" in str(error), "modified raw payload is rejected with integrity error")
        payload_path.write_bytes(original_payload)
        manifest = load_manifest(session, feeds)
        selected, selected_count = REPLAY_CLI.select_cycles(manifest, 5, False)
        check(selected_count == 5 and len(selected["cycles"]) == 5, "one-based cycle selection replays through the requested cycle")
        selected, selected_count = REPLAY_CLI.select_cycles(manifest, None, True)
        check(selected_count == len(manifest["cycles"]), "--all selection preserves every capture cycle")
        selected, selected_count = REPLAY_CLI.select_cycles(manifest, None, False)
        check(selected_count == 1 and selected["cycles"][0]["index"] == 0, "default replay selects the first recorded cycle")
        try:
            run_bridge(executable, manifest, speed=float("nan"))
            check(False, "non-finite replay speed is rejected")
        except ValueError:
            check(True, "non-finite replay speed is rejected")

        replay1 = run_bridge(executable, manifest)
        replay2 = run_bridge(executable, manifest)
        check(canonical_json(replay1) == canonical_json(replay2), "same multi-cycle capture replays byte-identically")
        frames = replay1["frames"]
        check([frame["displayState"] for frame in frames[:4]] == ["Fresh", "DegradedHold", "DegradedHold", "DegradedPartial"], "partial-grace just-before/exact/after policy boundaries")
        check(frames[5]["displayState"] == "StaleHold", "hard-stale just-before boundary retains stale frame")
        check(frames[5]["occupancy"] == frames[4]["occupancy"] and frames[5]["trains"] == frames[4]["trains"], "stale-hold preserves displayed occupancy and metadata")
        check(frames[6]["displayState"] == "NoData" and frames[7]["displayState"] == "NoData", "hard-stale exact/after boundary clears frame")
        check(not any(frames[6]["occupancy"]) and not frames[6]["trains"], "NoData clears stale occupancy and metadata")
        check(frames[8]["displayState"] == "Fresh", "complete feed recovers after NoData")
        first = frames[0]
        check(first["feeds"]["available"] == 8 and first["feeds"]["completeness"] == "COMPLETE", "complete 8-feed cycle")
        check(first["pipeline"]["vehiclesSeen"] == 1 and first["pipeline"]["resolvedTrains"] == 1, "actual firmware pipeline parses and resolves vehicle")
        check(first["pipeline"]["resolvedSegment"] == 1, "vehicle-position status is routed through the real segment resolver")
        check(first["pipeline"]["occupiedLeds"] == 1 and sum(first["occupancy"]) == 1, "TrainFrame occupancy derives from firmware pipeline")
        check(first["trains"][0]["tripId"] == "T-G-1" and first["trains"][0]["vehicleId"] == "VEH-G1", "train trip and vehicle metadata exported")
        check(first["trains"][0]["currentStop"] == "G29N" and first["trains"][0]["nextStop"] == "G29N", "current and TripUpdate next stop metadata exposed without changing pipeline")
        check(first["trains"][0]["previousStop"] == "G30" and first["trains"][0]["previousStopSource"] == "StaticTopology", "previous-stop metadata comes from compiled GTFS topology")
        unknown_session = root / "unknown-stop-session"
        create_session(
            unknown_session,
            feeds,
            [(0, {}, [("T-G-UNKNOWN", "G", "G99N", "VEH-G-UNKNOWN", 0, "G29N")])],
        )
        unknown_frame = run_bridge(
            executable, load_manifest(unknown_session, feeds)
        )["frames"][0]
        check(
            any(
                item["route"] == "G"
                and item["currentStop"] == "G99N"
                and item["category"] == "UnknownStop"
                and item["previousStopFailure"] == "UnknownDestination"
                for item in unknown_frame["observations"]
            ),
            "unknown-stop export identifies route, stop, and topology failure evidence",
        )
        check(frames[1]["feeds"]["available"] == 7 and frames[1]["feeds"]["missing"] == ["L"], "partial cycle lists omitted feed")
        check(frames[5]["feeds"]["available"] == 0 and frames[5]["feeds"]["completeness"] == "NO_FEEDS", "no-feed cycle health")

        no_feed_replay = run_bridge(executable, manifest, no_feeds=True)
        check(no_feed_replay["frames"][0]["feeds"]["available"] == 0, "fault injection omits every feed")
        omit_replay = run_bridge(executable, manifest, omitted=("G", "L"))
        check(omit_replay["frames"][0]["feeds"]["missing"] == ["G", "L"], "fault injection omits multiple selected feeds")
        corrupt_replay = run_bridge(executable, manifest, corrupt_feed="G")
        check(corrupt_replay["frames"][0]["feeds"]["available"] == 8, "corruption is delivery-time only, not capture omission")
        check(corrupt_replay["frames"][0]["pipeline"]["feedDecodeFailures"] >= 1, "corrupt copied payload reaches real parser failure path")
        check(first_payload_path.read_bytes() == original_payload, "fault injection leaves original payload immutable")

        g_bit = next(i for i, name in enumerate(
            ["2", "3", "4", "5", "A", "B", "C", "D", "F", "G", "J", "L", "M", "N", "Q", "R", "S", "Z"]
        ) if name == "G")
        hidden = run_bridge(executable, manifest, route_mask=0x3FFFF & ~(1 << g_bit))["frames"][0]
        mapped_index = first["trains"][0]["globalIndex"]
        check(hidden["occupancy"] == first["occupancy"], "route visibility does not modify pipeline occupancy")
        check(not hidden["routeVisible"][mapped_index] and hidden["filteredByRoute"][mapped_index], "route visibility uses canonical firmware mask")
        check(hidden["pixels"][mapped_index] == {"r": 0, "g": 0, "b": 0}, "hidden route contributes no map RGB")
        check(hidden["pixels"][266:270] == first["pixels"][266:270], "route mask leaves status RGB unchanged")
        bright = run_bridge(executable, manifest, brightness=128)["frames"][0]
        check(bright["pixels"][mapped_index] != first["pixels"][mapped_index], "brightness uses firmware renderer")
        check(first["brightness"] == 64, "diagnostic default brightness is 64")
        check(len(first["pixels"]) == 306 and len(first["occupancy"]) == 306, "Phase 4X-compatible replay frame shape")

        create_session(
            root / "collision",
            feeds,
            [(0, {}, [
                ("T-G-A", "G", "G29N", "VEH-A", 1, ""),
                ("T-G-B", "G", "G29N", "VEH-B", 1, ""),
            ])],
        )
        collision_frame = run_bridge(executable, load_manifest(root / "collision", feeds))["frames"][0]
        check(collision_frame["pipeline"]["collisionLeds"] == 1 and len(collision_frame["collisions"]) == 1, "collision metadata follows actual TrainFrame")
        collision = collision_frame["collisions"][0]
        check(collision["trainCount"] == 2 and len(collision["trains"]) == 2, "collision has two actual contributing train descriptors")
        check(collision["rgb"] == collision_frame["pixels"][collision["globalIndex"]], "collision RGB is actual firmware output")

        duplicate_session = root / "duplicate-trip"
        create_session(
            duplicate_session,
            feeds,
            [(0, {}, [
                ("T-G-DUP", "G", "G29N", "VEH-A", 1, ""),
                ("T-G-DUP", "G", "G29N", "VEH-B", 1, ""),
            ])],
        )
        duplicate_frame = run_bridge(
            executable, load_manifest(duplicate_session, feeds)
        )["frames"][0]
        check(duplicate_frame["pipeline"]["duplicateIdentical"] == 1, "firmware pipeline deduplicates identical trip outcomes")
        check(len(duplicate_frame["trains"]) == 1, "metadata contains only the actual deduplicated train")

        conflict_session = root / "conflicting-trip"
        create_session(
            conflict_session,
            feeds,
            [(0, {}, [
                ("T-G-CONFLICT", "G", "G29N", "VEH-A", 1, ""),
                ("T-G-CONFLICT", "G", "G30N", "VEH-A", 1, ""),
            ])],
        )
        conflict_frame = run_bridge(
            executable, load_manifest(conflict_session, feeds)
        )["frames"][0]
        check(conflict_frame["pipeline"]["conflictedTrips"] == 1, "firmware pipeline rejects conflicting trip observations")
        check(not conflict_frame["trains"] and not any(conflict_frame["occupancy"]), "conflicted trip is absent from displayed train metadata")

        exported = root / "replay.json"
        export_replay(exported, replay1)
        check(exported.read_bytes() == canonical_json(replay1), "replay-frame JSON is canonical and deterministic")
        replay_parsed = json.loads(exported.read_text(encoding="utf-8"))
        check(replay_parsed["schema"] == FRAME_SCHEMA and replay_parsed["schemaVersion"] == FRAME_VERSION, "replay-frame schema/version declared")
        generated_metadata = json.loads(
            (ROOT / "tools/virtual_map/generated/metadata.json").read_text(encoding="utf-8")
        )
        verified_export, imported_export = load_replay_export(exported, generated_metadata)
        check(verified_export == exported.read_bytes() and imported_export == replay_parsed, "simulator accepts the canonical replay export")
        unsupported_frame = dict(replay_parsed)
        unsupported_frame["schemaVersion"] = 99
        try:
            validate_replay_schema(unsupported_frame)
            check(False, "unsupported replay export version is rejected")
        except ValueError:
            check(True, "unsupported replay export version rejected")
        unsupported_export = root / "unsupported-replay.json"
        atomic_write(unsupported_export, canonical_json(unsupported_frame))
        try:
            load_replay_export(unsupported_export, generated_metadata)
            check(False, "simulator rejects unsupported replay export version")
        except ValueError:
            check(True, "simulator rejects unsupported replay export version")

        recorded = root / "recorded"

        def mocked_fetch(url, timeout, context):
            check(url.startswith("https://"), "recorder fetch URL is authoritative HTTPS")
            return {"status": "SUCCESS", "result": "HTTP_OK", "httpStatus": 200, "durationMs": 1}, b"\x0a\x00"

        capture_path = record(1, 0, 3, recorded, mocked_fetch)
        captured = load_manifest(capture_path, feeds)
        check(len(captured["cycles"]) == 1 and len(captured["cycles"][0]["feeds"]) == 8, "recorder atomically writes complete eight-slot cycle")
        check(all(slot["status"] == "SUCCESS" for slot in captured["cycles"][0]["feeds"]), "mocked recorder captures all feed slots")
        check(all((capture_path / slot["file"]).read_bytes() == b"\x0a\x00" for slot in captured["cycles"][0]["feeds"]), "recorder preserves exact response bytes")

        with mock.patch("urllib.request.urlopen", side_effect=urllib.error.HTTPError(
            "https://example.invalid", 503, "Unavailable", {}, None
        )):
            failed, payload = fetch("https://example.invalid", 1)
        check(failed["status"] == "HTTP_FAILURE" and failed["httpStatus"] == 503 and payload is None, "recorder classifies HTTP failures without saving a body")

        with mock.patch("urllib.request.urlopen", side_effect=TimeoutError):
            timed_out, payload = fetch("https://example.invalid", 1)
        check(timed_out["status"] == "TIMEOUT" and payload is None, "recorder classifies timeout")

        with mock.patch("urllib.request.urlopen", side_effect=OSError("offline")):
            unavailable, payload = fetch("https://example.invalid", 1)
        check(unavailable["status"] == "DNS_CONNECT_FAILURE" and payload is None, "recorder classifies DNS/connect failure")

        with mock.patch("urllib.request.urlopen", side_effect=AssertionError("offline replay attempted HTTPS")):
            offline = run_bridge(executable, manifest)
        check(canonical_json(offline) == canonical_json(replay1), "replay completes deterministically with all network access disabled")

    print(f"Phase 4Y replay: {CHECKS} checks, {FAILURES} failures")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
