#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Shared manifest, hash, and host-bridge helpers for Phase 4Y."""

import hashlib
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
MANIFEST_NAME = "manifest.json"
MANIFEST_SCHEMA = "brooklyn-gtfsrt-capture"
MANIFEST_VERSION = 1
FRAME_SCHEMA = "brooklyn-gtfsrt-replay"
FRAME_VERSION = 1
MAX_PAYLOAD_BYTES = 512 * 1024
MAX_CYCLES = 10000
MAX_SESSION_BYTES = 512 * 1024 * 1024

CORE_SOURCES = [
    ROOT / "src/gtfs_rt_decoder.cpp",
    ROOT / "src/gtfs_rt_state.cpp",
    ROOT / "src/previous_stop.cpp",
    ROOT / "src/generated/gtfs_topology_table.cpp",
    ROOT / "src/gtfs_adapter.cpp",
    ROOT / "src/generated/gtfs_stop_table.cpp",
    ROOT / "src/train_position_resolver.cpp",
    ROOT / "src/semantic_api.cpp",
    ROOT / "src/generated/semantic_tables.cpp",
    ROOT / "src/led_map.cpp",
    ROOT / "src/train_frame.cpp",
    ROOT / "src/rgb_renderer.cpp",
    ROOT / "src/train_pipeline.cpp",
    ROOT / "src/operational_display.cpp",
]

FEED_STATUSES = {
    "SUCCESS": 0,
    "HTTP_FAILURE": 1,
    "DNS_CONNECT_FAILURE": 2,
    "TIMEOUT": 3,
    "OMITTED": 4,
    "UNAVAILABLE": 5,
}


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def canonical_json(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )


def atomic_write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("wb") as output:
        output.write(data)
        output.flush()
        os.fsync(output.fileno())
    os.replace(temporary, path)


def compiler():
    sys.path.insert(0, str(ROOT / "tools"))
    import phase4e_host

    return phase4e_host.compiler()


def build_bridge(executable):
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
            str(HERE / "host.cpp"),
            *map(str, CORE_SOURCES),
            "-o",
            str(executable),
        ]
    )
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise RuntimeError("Phase 4Y host replay bridge compilation failed")


def bridge_feeds(executable):
    result = subprocess.run(
        [str(executable), "feeds"], capture_output=True, text=True, encoding="utf-8", check=True
    )
    parsed = json.loads(result.stdout)
    feeds = parsed.get("feeds")
    if parsed.get("formatVersion") != 1 or not isinstance(feeds, list) or len(feeds) != 8:
        raise ValueError("compiled firmware feed table must contain exactly eight feeds")
    names = [feed.get("id") for feed in feeds]
    if len(set(names)) != 8 or any(not isinstance(name, str) or not name for name in names):
        raise ValueError("compiled firmware feed identifiers are invalid")
    if any(not feed["url"].startswith("https://") for feed in feeds):
        raise ValueError("compiled firmware feed table contains a non-HTTPS endpoint")
    return feeds


def parse_utc(value, label):
    if not isinstance(value, str) or re.fullmatch(
        r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?Z", value
    ) is None:
        raise ValueError(f"{label} must be an RFC3339 UTC timestamp")
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise ValueError(f"{label} must be an RFC3339 UTC timestamp") from error
    return parsed


def load_manifest(session_dir, expected_feeds=None, verify_payloads=True):
    session_dir = Path(session_dir).resolve()
    manifest_path = session_dir / MANIFEST_NAME
    try:
        manifest_bytes = manifest_path.read_bytes()
        manifest = json.loads(manifest_bytes)
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read capture manifest: {error}") from error
    if not isinstance(manifest, dict) or manifest.get("schema") != MANIFEST_SCHEMA:
        raise ValueError("unsupported capture manifest schema")
    if type(manifest.get("schemaVersion")) is not int or manifest["schemaVersion"] != MANIFEST_VERSION:
        raise ValueError(f"unsupported capture manifest version: {manifest.get('schemaVersion')}")
    parse_utc(manifest.get("startedUtc"), "manifest startedUtc")
    requested_cycles = manifest.get("requestedCycles")
    if type(requested_cycles) is not int or not 1 <= requested_cycles <= MAX_CYCLES:
        raise ValueError("manifest requestedCycles must be from 1 to 10000")
    interval = manifest.get("intervalSeconds")
    if (
        type(interval) not in (int, float)
        or not math.isfinite(interval)
        or not 0 <= interval <= 86400
    ):
        raise ValueError("manifest intervalSeconds must be from 0 to 86400")
    feeds = manifest.get("feeds")
    cycles = manifest.get("cycles")
    if not isinstance(feeds, list) or len(feeds) != 8:
        raise ValueError("manifest must declare exactly eight authoritative feed slots")
    feed_ids = [feed.get("id") for feed in feeds if isinstance(feed, dict)]
    if (
        len(feed_ids) != 8
        or len(set(feed_ids)) != 8
        or any(not isinstance(item, str) or not item for item in feed_ids)
    ):
        raise ValueError("manifest feed identifiers must be unique strings")
    if expected_feeds is not None and feed_ids != [feed["id"] for feed in expected_feeds]:
        raise ValueError("manifest feed identifiers/order differ from the compiled firmware table")
    if (
        not isinstance(cycles, list)
        or not cycles
        or len(cycles) > MAX_CYCLES
        or len(cycles) > requested_cycles
    ):
        raise ValueError("manifest must contain 1..10000 cycles")

    total_bytes = 0
    previous_elapsed = -1
    for index, cycle in enumerate(cycles):
        if (
            not isinstance(cycle, dict)
            or type(cycle.get("index")) is not int
            or cycle["index"] != index
        ):
            raise ValueError(f"cycle {index} index/order is invalid")
        elapsed = cycle.get("elapsedMs")
        if type(elapsed) is not int or elapsed < previous_elapsed or elapsed > 0xFFFFFFFF:
            raise ValueError(f"cycle {index} elapsedMs is invalid or non-monotonic")
        parse_utc(cycle.get("capturedUtc"), f"cycle {index} capturedUtc")
        previous_elapsed = elapsed
        slots = cycle.get("feeds")
        if not isinstance(slots, list) or len(slots) != 8:
            raise ValueError(f"cycle {index} must contain all eight feed slots")
        for feed_index, slot in enumerate(slots):
            if not isinstance(slot, dict) or slot.get("feedId") != feed_ids[feed_index]:
                raise ValueError(f"cycle {index} feed slot {feed_index} is invalid")
            status = slot.get("status")
            if not isinstance(status, str):
                raise ValueError(f"cycle {index} feed status must be a string")
            if status not in FEED_STATUSES:
                raise ValueError(f"cycle {index} feed {feed_ids[feed_index]} has unsupported status")
            if (
                not isinstance(slot.get("result"), str)
                or type(slot.get("durationMs")) is not int
                or slot["durationMs"] < 0
                or type(slot.get("fetchOffsetMs")) is not int
                or slot["fetchOffsetMs"] < 0
            ):
                raise ValueError(f"cycle {index} feed {feed_ids[feed_index]} timing/result is invalid")
            if "httpStatus" in slot and (
                type(slot["httpStatus"]) is not int or not 100 <= slot["httpStatus"] <= 599
            ):
                raise ValueError(f"cycle {index} feed {feed_ids[feed_index]} HTTP status is invalid")
            if status == "SUCCESS":
                filename = slot.get("file")
                length = slot.get("bytes")
                digest = slot.get("sha256")
                if (
                    not isinstance(filename, str)
                    or Path(filename).name != filename
                    or filename in ("", ".", "..")
                    or type(length) is not int
                    or not 0 < length <= MAX_PAYLOAD_BYTES
                    or not isinstance(digest, str)
                    or len(digest) != 64
                    or any(c not in "0123456789abcdef" for c in digest)
                ):
                    raise ValueError(f"cycle {index} successful feed metadata is invalid")
                total_bytes += length
                if verify_payloads:
                    payload_path = (session_dir / filename).resolve()
                    if payload_path.parent != session_dir:
                        raise ValueError("payload path escapes the capture session")
                    try:
                        payload = payload_path.read_bytes()
                    except OSError as error:
                        raise ValueError(f"cannot read payload {filename}: {error}") from error
                    if len(payload) != length:
                        raise ValueError(f"payload length mismatch for {filename}")
                    if sha256_bytes(payload) != digest:
                        raise ValueError(f"payload SHA-256 mismatch for {filename}")
            elif any(key in slot for key in ("file", "sha256", "bytes")):
                raise ValueError(f"non-success feed slot {feed_ids[feed_index]} references a payload")
        if total_bytes > MAX_SESSION_BYTES:
            raise ValueError("capture session exceeds the 512 MiB replay limit")
    manifest["_sessionDir"] = str(session_dir)
    manifest["_manifestSha256"] = sha256_bytes(manifest_bytes)
    captured_start = parse_utc(manifest["startedUtc"], "manifest startedUtc")
    manifest["_startUnix"] = int(captured_start.timestamp())
    for cycle in cycles:
        for slot in cycle["feeds"]:
            if slot["status"] == "SUCCESS":
                slot["_sessionDir"] = str(session_dir)
    return manifest


def cycle_payload(slot):
    if slot.get("status") != "SUCCESS":
        return b""
    session_dir = Path(slot["_sessionDir"])
    data = (session_dir / slot["file"]).read_bytes()
    if len(data) != slot["bytes"] or sha256_bytes(data) != slot["sha256"]:
        raise ValueError(f"payload integrity check failed for {slot['file']}")
    return data


def run_bridge(executable, manifest, brightness=64, route_mask=0x3FFFF, speed=1.0,
               omitted=(), no_feeds=False, corrupt_feed=None, delay_next_ms=0):
    if type(brightness) is not int or not 0 <= brightness <= 255:
        raise ValueError("brightness must be an integer from 0 to 255")
    if type(route_mask) is not int or not 0 <= route_mask <= 0x3FFFF:
        raise ValueError("route mask must be an 18-bit canonical mask")
    if (
        type(speed) not in (float, int)
        or not math.isfinite(speed)
        or speed <= 0
        or speed > 1000
    ):
        raise ValueError("speed must be greater than zero and at most 1000")
    if type(delay_next_ms) is not int or not 0 <= delay_next_ms <= 0xFFFFFFFF:
        raise ValueError("delay must fit uint32 milliseconds")
    feeds = [item["id"] for item in manifest["feeds"]]
    unknown = set(omitted) - set(feeds)
    if unknown:
        raise ValueError("unknown feed id(s): " + ", ".join(sorted(unknown)))
    if corrupt_feed is not None and corrupt_feed not in feeds:
        raise ValueError(f"unknown corrupt-feed id: {corrupt_feed}")

    process = subprocess.Popen(
        [str(executable), "replay"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=None,
        bufsize=0,
    )
    assert process.stdin is not None and process.stdout is not None

    def send(data):
        view = memoryview(data)
        while view:
            written = process.stdin.write(view)
            if written is None or written == 0:
                raise RuntimeError("host replay bridge input closed unexpectedly")
            view = view[written:]

    send(f"REPLAY 1 {brightness} {route_mask} 1 3 0\n".encode("ascii"))
    frames = []
    previous_elapsed = 0
    delayed = 0
    requested_delay = delay_next_ms
    try:
        for index, cycle in enumerate(manifest["cycles"]):
            elapsed = int(cycle["elapsedMs"] / speed)
            if index > 0 and delay_next_ms:
                delayed = delay_next_ms
                delay_next_ms = 0
            elapsed += delayed
            if elapsed > 0xFFFFFFFF:
                raise ValueError("scaled replay time exceeds uint32 monotonic range")
            if elapsed < previous_elapsed:
                raise ValueError("scaled replay cycle time exceeds uint32 monotonic range")
            previous_elapsed = elapsed
            replay_unix = (manifest["_startUnix"] + int(elapsed / 1000)) & 0xFFFFFFFF
            statuses = []
            for slot in cycle["feeds"]:
                if no_feeds or slot["feedId"] in omitted:
                    statuses.append(("OMITTED", b""))
                    continue
                status = slot["status"]
                payload = cycle_payload(slot)
                if status == "SUCCESS" and corrupt_feed == slot["feedId"] and payload:
                    payload = b"\x0a\xff"
                statuses.append((status, payload if status == "SUCCESS" else b""))
            send(
                f"C {index} {replay_unix} {elapsed} {len(statuses)}\n".encode("ascii")
            )
            for status, payload in statuses:
                send(
                    f"F {FEED_STATUSES[status]} {len(payload)} {payload.hex()}\n".encode("ascii")
                )
            line = process.stdout.readline()
            if not line:
                return_code = process.wait(timeout=5)
                raise RuntimeError(
                    "host replay bridge exited before returning a cycle "
                    f"(status {return_code}, cycle {index})"
                )
            try:
                frame = json.loads(line)
            except json.JSONDecodeError as error:
                raise RuntimeError("host replay bridge returned invalid JSON") from error
            frame["recordedUtc"] = cycle["capturedUtc"]
            frame["captureElapsedMs"] = cycle["elapsedMs"]
            frame["simulatedElapsedMs"] = elapsed
            frame["feeds"]["statuses"] = [
                {"feedId": slot["feedId"], "status": status}
                for slot, (status, _) in zip(cycle["feeds"], statuses)
            ]
            frame["feeds"]["available"] = sum(
                status == "SUCCESS" for status, _ in statuses
            )
            frame["feeds"]["expected"] = 8
            frame["feeds"]["missing"] = [
                feed_id for feed_id, (status, _) in zip(feeds, statuses) if status != "SUCCESS"
            ]
            frame["feeds"]["parseFailures"] = frame["pipeline"]["feedDecodeFailures"]
            frame["feeds"]["completeness"] = (
                "COMPLETE" if frame["feeds"]["available"] == 8
                else "NO_FEEDS" if frame["feeds"]["available"] == 0
                else "PARTIAL"
            )
            if not isinstance(frame.get("observations"), list):
                raise RuntimeError("host replay bridge returned invalid observation metadata")
            frames.append(frame)
        send(b"END\n")
        process.stdin.close()
        process.wait(timeout=60)
        if process.returncode:
            raise RuntimeError(f"host replay bridge exited with status {process.returncode}")
    except BaseException:
        if process.poll() is None:
            process.kill()
        process.wait()
        raise
    output = {
        "schema": FRAME_SCHEMA,
        "schemaVersion": FRAME_VERSION,
        "captureManifestSha256": manifest["_manifestSha256"],
        "captureSessionId": manifest["sessionId"],
        "replayOptions": {
            "brightness": brightness,
            "routeVisibilityMask": route_mask,
            "speed": speed,
            "omittedFeeds": sorted(omitted),
            "noFeeds": no_feeds,
            "corruptFeed": corrupt_feed,
            "delayNextCycleMs": requested_delay,
        },
        "frames": frames,
    }
    return output


def export_replay(path, replay):
    atomic_write(path, canonical_json(replay))


def validate_replay_schema(replay):
    if not isinstance(replay, dict) or replay.get("schema") != FRAME_SCHEMA:
        raise ValueError("unsupported replay export schema")
    if type(replay.get("schemaVersion")) is not int or replay["schemaVersion"] != FRAME_VERSION:
        raise ValueError(f"unsupported replay export version: {replay.get('schemaVersion')}")
    return replay


def temporary_bridge():
    return tempfile.TemporaryDirectory(prefix="brooklyn-gtfsrt-replay-")
