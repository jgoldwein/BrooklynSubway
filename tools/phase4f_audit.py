#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Phase 4F step 1 (host only): can GTFS-RT alone identify the previous stop?

For every supported-route train in INCOMING_AT / IN_TRANSIT_TO the VehiclePosition
is joined with its TripUpdate (by trip_id) and classified:

  REALTIME_EXACT         a stop_time_update precedes the vehicle's stop, so the
                         previous stop is stated by the feed itself
  REALTIME_INSUFFICIENT  the vehicle's stop is the first stop_time_update (past
                         stops are not sent), previous stop is not in the feed
  REALTIME_CONFLICT      the vehicle stop_id is not in the TripUpdate stop list
  NO_MATCHING_TRIP_UPDATE the VehiclePosition has no TripUpdate with stop times

Usage: python tools/phase4f_audit.py [--snapshots 3 --interval 25] [--dir DIR]
Snapshots are written to local_reference/phase4f/snap<N>/ (git-ignored).
Times are reported in America/New_York.
"""

import argparse
import sys
import time
import urllib.request
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

from google.transit import gtfs_realtime_pb2 as pb

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from phase4e_fetch_feeds import BASE, FEEDS, fetch  # noqa: E402

SUPPORTED = set("2 3 4 5 A B C D F G J L M N Q R S Z H FS".split())


def ny(ts):
    try:
        from zoneinfo import ZoneInfo
        return datetime.fromtimestamp(ts, ZoneInfo("America/New_York")).strftime("%Y-%m-%d %H:%M:%S %Z")
    except Exception:  # tzdata missing: EDT/EST by US rule is not guessed
        return datetime.fromtimestamp(ts, timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")


def parent(s):
    return s[:-1] if len(s) == 4 and s[3] in "NS" else s


def take_snapshot(out):
    out.mkdir(parents=True, exist_ok=True)
    for suffix in FEEDS:
        fetch(BASE + suffix, out / f"feed{suffix}.pb")


def classify_dir(d):
    counts = Counter()
    examples = {}
    seq_in_stu = 0
    stu_total = 0
    stamps = []
    for fn in sorted(d.glob("*.pb")):
        m = pb.FeedMessage()
        m.ParseFromString(fn.read_bytes())
        stamps.append(m.header.timestamp)
        stus, vehicles = {}, {}
        for e in m.entity:
            if e.HasField("trip_update"):
                tu = e.trip_update
                stus.setdefault(tu.trip.trip_id, []).extend(tu.stop_time_update)
                for u in tu.stop_time_update:
                    stu_total += 1
                    seq_in_stu += u.HasField("stop_sequence") if hasattr(u, "stop_sequence") else 0
            if e.HasField("vehicle"):
                vehicles[e.vehicle.trip.trip_id] = e.vehicle
        for tid, v in vehicles.items():
            route = v.trip.route_id
            if route not in SUPPORTED or not v.HasField("current_status") or not v.stop_id:
                continue
            if v.current_status not in (0, 2):  # INCOMING_AT, IN_TRANSIT_TO
                continue
            updates = stus.get(tid)
            if not updates:
                cls = "NO_MATCHING_TRIP_UPDATE"
            else:
                ids = [parent(u.stop_id) for u in updates]
                vs = parent(v.stop_id)
                if vs not in ids:
                    cls = "REALTIME_CONFLICT"
                elif ids.index(vs) > 0:
                    cls = "REALTIME_EXACT"
                else:
                    cls = "REALTIME_INSUFFICIENT"
            counts[cls] += 1
            examples.setdefault(cls, []).append(
                (route, tid, v.stop_id, v.current_stop_sequence if v.HasField("current_stop_sequence") else None,
                 [u.stop_id for u in (updates or [])][:4]))
    return counts, examples, stamps, stu_total, seq_in_stu


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snapshots", type=int, default=3)
    ap.add_argument("--interval", type=int, default=25)
    ap.add_argument("--dir", type=Path, default=ROOT / "local_reference/phase4f")
    ap.add_argument("--reuse", action="store_true", help="do not fetch; use existing snapshots")
    args = ap.parse_args()

    total = Counter()
    for n in range(args.snapshots):
        d = args.dir / f"snap{n}"
        if not args.reuse:
            if n:
                time.sleep(args.interval)
            take_snapshot(d)
        counts, ex, stamps, stu_total, seq_in_stu = classify_dir(d)
        total.update(counts)
        print(f"snapshot {n}: feed times {ny(min(stamps))} .. {ny(max(stamps))}")
        print(f"   {dict(counts)}  (stop_time_updates {stu_total}, with stop_sequence {seq_in_stu})")
        for cls, rows in sorted(ex.items()):
            if n == 0:
                for r in rows[:2]:
                    print(f"   e.g. {cls}: route {r[0]} trip {r[1]} vehicle stop {r[2]} seq {r[3]} stu {r[4]}")
    print("TOTAL", dict(total), "trains", sum(total.values()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
