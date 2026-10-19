#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Record raw bytes from the firmware's eight public MTA feeds."""

import argparse
import hashlib
import json
import math
import socket
import ssl
import time
import urllib.error
import urllib.request
import uuid
from datetime import datetime, timezone
from pathlib import Path

from core import (
    FEED_STATUSES,
    HERE,
    MANIFEST_NAME,
    MANIFEST_SCHEMA,
    MANIFEST_VERSION,
    MAX_PAYLOAD_BYTES,
    atomic_write,
    bridge_feeds,
    build_bridge,
    temporary_bridge,
)


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def fetch(url, timeout, context=None):
    request = urllib.request.Request(
        url,
        headers={"Accept": "application/x-protobuf", "User-Agent": "BrooklynMap-GTFSRT-Recorder/1"},
        method="GET",
    )
    started = time.monotonic()
    try:
        with urllib.request.urlopen(request, timeout=timeout, context=context) as response:
            status = response.status
            data = bytearray()
            while True:
                chunk = response.read(min(65536, MAX_PAYLOAD_BYTES + 1 - len(data)))
                if not chunk:
                    break
                data.extend(chunk)
                if len(data) > MAX_PAYLOAD_BYTES:
                    return {
                        "status": "UNAVAILABLE",
                        "result": "PAYLOAD_TOO_LARGE",
                        "httpStatus": status,
                        "durationMs": int((time.monotonic() - started) * 1000),
                    }, None
            if not data:
                return {
                    "status": "UNAVAILABLE",
                    "result": "EMPTY_BODY",
                    "httpStatus": status,
                    "durationMs": int((time.monotonic() - started) * 1000),
                }, None
            return {
                "status": "SUCCESS",
                "result": "HTTP_OK",
                "httpStatus": status,
                "durationMs": int((time.monotonic() - started) * 1000),
            }, bytes(data)
    except urllib.error.HTTPError as error:
        return {
            "status": "HTTP_FAILURE",
            "result": "HTTP_FAILURE",
            "httpStatus": int(error.code),
            "durationMs": int((time.monotonic() - started) * 1000),
        }, None
    except (TimeoutError, socket.timeout):
        return {
            "status": "TIMEOUT",
            "result": "TIMEOUT",
            "durationMs": int((time.monotonic() - started) * 1000),
        }, None
    except (urllib.error.URLError, OSError, ssl.SSLError):
        return {
            "status": "DNS_CONNECT_FAILURE",
            "result": "DNS_OR_CONNECT_FAILURE",
            "durationMs": int((time.monotonic() - started) * 1000),
        }, None


def safe_feed_filename(feed_id):
    return "".join(char if char.isalnum() or char in "-_" else "-" for char in feed_id)


def record(cycles, interval, timeout, session_dir=None, opener=None):
    if type(cycles) is not int or cycles < 1 or cycles > 10000:
        raise ValueError("cycles must be from 1 to 10000")
    if type(interval) not in (int, float) or not math.isfinite(interval) or interval < 0 or interval > 86400:
        raise ValueError("interval must be from 0 to 86400 seconds")
    if type(timeout) not in (int, float) or not math.isfinite(timeout) or timeout <= 0 or timeout > 120:
        raise ValueError("timeout must be greater than zero and at most 120 seconds")

    with temporary_bridge() as temp:
        executable = Path(temp) / "brooklyn_replay_host.exe"
        build_bridge(executable)
        feeds = bridge_feeds(executable)

    started_utc = utc_now()
    if session_dir is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        session_dir = HERE.parents[1] / "captures" / f"{stamp}-{uuid.uuid4().hex[:8]}"
    session_dir = Path(session_dir).resolve()
    session_dir.mkdir(parents=True, exist_ok=False)
    monotonic_start = time.monotonic()
    context = ssl.create_default_context()
    fetch_feed = opener or fetch
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "schemaVersion": MANIFEST_VERSION,
        "sessionId": session_dir.name,
        "startedUtc": started_utc,
        "intervalSeconds": interval,
        "requestedCycles": cycles,
        "feeds": [{"id": feed["id"]} for feed in feeds],
        "cycles": [],
    }
    print(f"Capture session: {session_dir}", flush=True)
    atomic_write(session_dir / MANIFEST_NAME, json.dumps(
        manifest, ensure_ascii=False, sort_keys=True, indent=2
    ).encode("utf-8") + b"\n")

    for cycle_index in range(cycles):
        cycle_start = time.monotonic()
        slots = []
        cycle = {
            "index": cycle_index,
            "elapsedMs": int((cycle_start - monotonic_start) * 1000),
            "capturedUtc": utc_now(),
            "feeds": slots,
        }
        for feed in feeds:
            feed_start = time.monotonic()
            result, payload = fetch_feed(feed["url"], timeout, context)
            slot = {
                "feedId": feed["id"],
                "status": result["status"],
                "result": result["result"],
                "fetchOffsetMs": int((feed_start - monotonic_start) * 1000),
                "durationMs": result["durationMs"],
            }
            if "httpStatus" in result:
                slot["httpStatus"] = result["httpStatus"]
            if payload is not None:
                filename = f"cycle-{cycle_index:04d}-{safe_feed_filename(feed['id'])}.pb"
                atomic_write(session_dir / filename, payload)
                slot.update(
                    {
                        "file": filename,
                        "bytes": len(payload),
                        "sha256": hashlib.sha256(payload).hexdigest(),
                    }
                )
            slots.append(slot)
            print(f"cycle {cycle_index + 1}/{cycles} {feed['id']}: {slot['status']}")

        manifest["cycles"].append(cycle)
        atomic_write(session_dir / MANIFEST_NAME, json.dumps(
            manifest, ensure_ascii=False, sort_keys=True, indent=2
        ).encode("utf-8") + b"\n")
        print(f"Completed capture cycle {cycle_index + 1}/{cycles}.")
        if cycle_index + 1 < cycles and interval:
            time.sleep(interval)

    print(f"Completed cycles: {len(manifest['cycles'])}/{cycles}")
    return session_dir


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cycles", type=int, default=10)
    parser.add_argument("--interval", type=float, default=30)
    parser.add_argument("--timeout", type=float, default=12)
    parser.add_argument("--session-dir", type=Path)
    args = parser.parse_args()
    try:
        record(args.cycles, args.interval, args.timeout, args.session_dir)
    except KeyboardInterrupt:
        print("\nRecording stopped; completed cycles remain usable.")
        return 130
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"recording failed: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
