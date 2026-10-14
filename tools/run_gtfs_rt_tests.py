#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the host-side Phase 4E GTFS-RT tests (no hardware, no network).

1. C++ synthetic-protobuf tests of the decoder + state builder.
2. Optional oracle cross-check: if `gtfs-realtime-bindings` is installed and
   snapshots exist in local_reference/phase4e/feeds, the independent Google
   protobuf decoder must agree with the firmware decoder on entity and trip
   counts. Skipped (and reported as skipped) otherwise.

Usage: python tools/run_gtfs_rt_tests.py
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def oracle(feeds, exe):
    try:
        from google.transit import gtfs_realtime_pb2 as pb
    except ImportError:
        print("oracle cross-check: SKIPPED (gtfs-realtime-bindings not installed)")
        return 0
    if not feeds:
        print("oracle cross-check: SKIPPED (no local snapshots in local_reference/phase4e/feeds)")
        return 0
    bad = 0
    for f in feeds:
        m = pb.FeedMessage()
        m.ParseFromString(f.read_bytes())
        tu = sum(e.HasField("trip_update") for e in m.entity)
        veh = sum(e.HasField("vehicle") for e in m.entity)
        trips = {e.trip_update.trip.trip_id for e in m.entity if e.HasField("trip_update")}
        trips |= {e.vehicle.trip.trip_id for e in m.entity if e.HasField("vehicle")}
        out = subprocess.run([str(exe), "--summary-only", str(f)], capture_output=True,
                             text=True, encoding="utf-8", errors="replace").stdout
        g = re.search(r"decoded:\s+(\d+) \(trip_update (\d+), vehicle (\d+)", out)
        t = re.search(r"trips \(merged by id\):\s+(\d+)", out)
        ok = (g and t and (int(g[1]), int(g[2]), int(g[3]), int(t[1])) ==
              (len(m.entity), tu, veh, len(trips)))
        print(f"oracle {f.name}: entities={len(m.entity)} tu={tu} veh={veh} trips={len(trips)} "
              f"-> {'OK' if ok else 'MISMATCH'}")
        bad += 0 if ok else 1
    return bad


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "gtfs_rt_test.exe"
        if h.build(h.ROOT / "tools/host_tests/gtfs_rt_test.cpp", exe):
            return 1
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        rc = run.returncode
        diag = Path(tmp) / "diag.exe"
        if h.build(h.ROOT / "tools/host_tests/gtfs_rt_diag.cpp", diag):
            return 1
        feeds = sorted((h.ROOT / "local_reference/phase4e/feeds").glob("*.pb"))
        return 1 if (rc or oracle(feeds, diag)) else 0


if __name__ == "__main__":
    sys.exit(main())
