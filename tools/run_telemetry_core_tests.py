#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the telemetry core host tests, then validate every generated payload
against the frozen JSON schemas and re-check the time_synced/UTC invariant."""

import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phase4e_host as h  # noqa: E402
from tools.telemetry.schema_validate import load_schema, validate  # noqa: E402


def main():
    failures = 0
    checks = 0
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "telemetry_core_test.exe"
        command = h.compiler() + [
            "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1", "-fno-exceptions",
            f"-I{h.ROOT / 'include'}",
            f"-I{h.ROOT / 'tools/host_tests'}",
            str(h.ROOT / "tools/host_tests/telemetry_core_test.cpp"),
            str(h.ROOT / "src/telemetry_core.cpp"),
            "-o", str(exe),
        ]
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout, end="")
            print(build.stderr, end="")
            return build.returncode
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        lines = run.stdout.splitlines()
        for line in lines:
            if not line.startswith("SAMPLE "):
                print(line)
        print(run.stderr, end="")
        if run.returncode != 0:
            failures += 1

    status_schema = load_schema("brooklyn-telemetry-v1.schema.json")
    event_schema = load_schema("brooklyn-event-v1.schema.json")
    samples = 0
    for line in lines:
        if not line.startswith("SAMPLE "):
            continue
        _, kind, body = line.split(" ", 2)
        doc = json.loads(body)
        schema = status_schema if kind == "status" else event_schema
        errors = validate(doc, schema)
        checks += 1
        samples += 1
        if errors:
            failures += 1
            print(f"schema FAIL ({kind}): {errors[:3]} :: {body[:120]}")
        if doc["time_synced"] != (doc["device_time_utc"] is not None):
            failures += 1
            print(f"time_synced/UTC mismatch: {body[:120]}")

    # Regression: the invariant must be enforced by the schema itself.
    sample = next(json.loads(l.split(" ", 2)[2]) for l in lines
                  if l.startswith("SAMPLE event ") and '"time_synced":true' in l)
    cases = [
        ("synced + valid UTC", True, "2026-05-28T20:26:40Z", True),
        ("synced + null", True, None, False),
        ("unsynced + null", False, None, True),
        ("unsynced + UTC", False, "2026-05-28T20:26:40Z", False),
        ("synced + malformed UTC", True, "not-a-time", False),
    ]
    for name, synced, utc, expect_ok in cases:
        doc = dict(sample, time_synced=synced, device_time_utc=utc)
        ok = not validate(doc, event_schema)
        checks += 1
        if ok != expect_ok:
            failures += 1
            print(f"schema regression FAIL: {name}")

    print(f"telemetry core schema checks={checks} (payload samples={samples}) failures={failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
