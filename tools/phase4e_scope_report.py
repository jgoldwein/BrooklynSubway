#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Phase 4E: split live UnknownStop results into inside / outside map scope.

Usage: python tools/phase4e_scope_report.py --gtfs DIR_OR_ZIP [--route R]

Every UnknownStop row from the live diagnostic is classified using the static
MTA GTFS and the Phase 4 supplemental stop map (data/gtfs_stop_map.csv):

  STATIC_GTFS_MISMATCH  stop id absent from static stops.txt (cannot be located)
  INSIDE_MAP_SCOPE      within 150 m of a mapped parent whose registry station
                        carries the train's route (a missing alias; should be 0)
  BOUNDARY_SEGMENT      stop is on the map but the previous stop is not (segment\n                        crosses the map edge; no LED exists for it)\n  OUTSIDE_MAP_SCOPE     otherwise (no map station nearby)

Host-only; reads local data, touches no hardware.
"""

import argparse
import csv
import io
import math
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NEAR_M = 150.0
SHUTTLE_ALIAS = {"H": "S", "FS": "S"}


def read_static(src: Path):
    if src.is_dir():
        text = (src / "stops.txt").read_text(encoding="utf-8-sig")
    else:
        with zipfile.ZipFile(src) as z:
            text = z.read("stops.txt").decode("utf-8-sig")
    return {r["stop_id"]: r for r in csv.DictReader(io.StringIO(text))}


def dist_m(lat1, lon1, lat2, lon2):
    k = 111320.0
    dy = (lat1 - lat2) * k
    dx = (lon1 - lon2) * k * math.cos(math.radians(lat1))
    return math.hypot(dx, dy)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gtfs", type=Path, required=True)
    ap.add_argument("--route")
    args = ap.parse_args()

    static = read_static(args.gtfs)
    with open(ROOT / "data/gtfs_stop_map.csv", encoding="utf-8", newline="") as f:
        mapped = list(csv.DictReader(f))

    cmd = [sys.executable, str(ROOT / "tools/phase4e_diag.py"), "--category", "UnknownStop", "--all"]
    if args.route:
        cmd += ["--route", args.route]
    out = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", check=True).stdout

    def locate(route, stop):
        """Returns (key, note) for one stop id; key is STATIC_GTFS_MISMATCH/INSIDE/OUTSIDE."""
        parent = stop[:-1] if stop[-1:] in "NS" and stop[:-1] in static else stop
        rec = static.get(stop) or static.get(parent)
        if rec is None:
            return "STATIC_GTFS_MISMATCH", ""
        lat, lon, name = float(rec["stop_lat"]), float(rec["stop_lon"]), rec["stop_name"]
        best = min(mapped, key=lambda m: dist_m(lat, lon, float(m["lat"]), float(m["lon"])))
        d = dist_m(lat, lon, float(best["lat"]), float(best["lon"]))
        # Names repeat across boroughs, so proximity plus a shared route is required.
        sr = SHUTTLE_ALIAS.get(route, route)
        near = [m for m in mapped
                if dist_m(lat, lon, float(m["lat"]), float(m["lon"])) <= NEAR_M
                and sr in m["registry_routes"].split("/")]
        key = "INSIDE_MAP_SCOPE" if near else "OUTSIDE_MAP_SCOPE"
        return key, f"{name} ({d:.0f} m from {best['gtfs_parent_stop_id']})"

    states = ("STOPPED_AT", "INCOMING_AT", "IN_TRANSIT_TO")
    rows = []
    for line in out.splitlines():
        p = line.split()
        if len(p) < 6 or "UnknownStop" not in p[-2:]:
            continue
        idx = next((i for i, t in enumerate(p) if t in states), None)
        if idx:
            prev = p[idx - 2] if idx >= 5 else None  # columns: rt trip d [prev] stop
            rows.append((p[0], p[idx - 1], prev))

    keys = ("STATIC_GTFS_MISMATCH", "INSIDE_MAP_SCOPE", "BOUNDARY_SEGMENT", "OUTSIDE_MAP_SCOPE")
    buckets = {k: {} for k in keys}
    for route, stop, prev in rows:
        key, note = locate(route, stop)
        if key == "INSIDE_MAP_SCOPE" and prev:
            # The stop itself is on the map; if the previous stop is not, the
            # segment crosses the map boundary and has no LED (not a missing alias).
            pkey, pnote = locate(route, prev)
            if pkey != "INSIDE_MAP_SCOPE":
                key, note = "BOUNDARY_SEGMENT", f"{note}; previous {prev}: {pnote or pkey}"
        buckets[key].setdefault((route, stop), [0, note])[0] += 1
    print(f"UnknownStop rows: {len(rows)}")
    for key, items in buckets.items():
        total = sum(v[0] for v in items.values())
        print(f"{key}: {total} trains, {len(items)} distinct (route, stop)")
        if key != "OUTSIDE_MAP_SCOPE":
            for (route, stop), (n, note) in sorted(items.items()):
                print(f"    {route:3} {stop:5} x{n}  {note}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
