#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Phase 4I host audit: feed sizes, VehiclePosition vs TripUpdate, duplicate trips.

Usage: python tools/phase4i_audit.py DIR [DIR ...]   (each DIR holds feed*.pb)
"""

import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from google.transit import gtfs_realtime_pb2 as pb

SUPPORTED = set("2 3 4 5 A C B D F M G J Z L N Q R S FS GS H".split())  # H/FS/GS are S-route feed ids


def main():
    maxima = defaultdict(int)
    for d in map(Path, sys.argv[1:]):
        files = sorted(d.glob("feed*.pb"))
        trips = defaultdict(list)  # trip_id -> [(feed, route, stop, status, ts)]
        tu_only = veh_only = both = 0
        tot = defaultdict(int)
        hdr = 0
        veh_no_route = veh_no_dir = veh_no_stop = 0
        for f in files:
            raw = f.read_bytes()
            m = pb.FeedMessage()
            m.ParseFromString(raw)
            hdr = max(hdr, m.header.timestamp)
            ents = len(m.entity)
            nveh = sum(e.HasField("vehicle") for e in m.entity)
            ntu = sum(e.HasField("trip_update") for e in m.entity)
            sup = sum(e.HasField("vehicle") and e.vehicle.trip.route_id in SUPPORTED for e in m.entity)
            print(f"  {f.name:14} bytes {len(raw):6} entities {ents:4} vehicles {nveh:4} tripUpdates {ntu:4} supportedVehicles {sup:4}")
            for k, v in (("bytes", len(raw)), ("entities", ents), ("vehicles", nveh), ("tu", ntu), ("sup", sup)):
                maxima[k] = max(maxima[k], v)
                tot[k] += v
            vt, tt = set(), set()
            for e in m.entity:
                if e.HasField("vehicle"):
                    v = e.vehicle
                    vt.add(v.trip.trip_id)
                    veh_no_route += not v.trip.route_id
                    veh_no_stop += not v.stop_id
                    trips[v.trip.trip_id].append((f.name, v.trip.route_id, v.stop_id, v.current_status, v.timestamp))
                if e.HasField("trip_update"):
                    tt.add(e.trip_update.trip.trip_id)
            both += len(vt & tt)
            veh_only += len(vt - tt)
            tu_only += len(tt - vt)
        t = datetime.fromtimestamp(hdr, ZoneInfo("America/New_York")).strftime("%Y-%m-%d %H:%M:%S %Z")
        dups = {k: v for k, v in trips.items() if len(v) > 1}
        within = sum(1 for v in dups.values() if len({x[0] for x in v}) == 1)
        across = len(dups) - within
        supdups = {k: v for k, v in dups.items() if any(x[1] in SUPPORTED for x in v)}
        ident = sum(1 for v in dups.values() if len(set(v)) == 1)
        print(f"{d}: header {t}; files {len(files)} bytes {tot['bytes']} entities {tot['entities']} "
              f"vehicles {tot['vehicles']} tripUpdates {tot['tu']} supportedVehicles {tot['sup']}")
        print(f"  trips with vehicle&TU {both}, vehicle-only {veh_only}, TU-only {tu_only}; "
              f"vehicle missing route {veh_no_route}, missing stop {veh_no_stop}")
        print(f"  duplicate vehicle trip_ids: {len(dups)} (within a feed {within}, across feeds {across}, "
              f"supported {len(supdups)}, identical {ident}, conflicting {len(dups) - ident})")
        for k, v in list(dups.items())[:5]:
            print("   dup", k, v)
    print("MAX per feed:", dict(maxima))


if __name__ == "__main__":
    main()
