#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build a stop-pattern index from the static MTA subway GTFS (host only).

The GTFS-RT feed carries no past stops, so the previous stop of an in-transit
train must come from static schedule data. This tool writes one TSV line per
distinct (shape_id, route_id, direction, ordered parent-stop list):

    shape_id <TAB> route_id <TAB> N|S <TAB> A01,A02,A03,...

shape_id is the static trips.txt shape_id; it equals the part of a realtime
trip_id after the first '_' (e.g. "B..S65R" for "032600_B..S65R"). Direction comes
from the shape id ("..N"/"..S"), never from direction_id.

Usage: python tools/phase4e_build_pattern_index.py --gtfs DIR_OR_ZIP [--out FILE]
Default out: local_reference/phase4e/pattern_index.tsv (git-ignored).
"""

import argparse
import csv
import io
import re
import sys
import zipfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHAPE_DIR = re.compile(r"^[A-Za-z0-9]+\.+([NS])")


def open_table(src, name):
    if src.is_dir():
        return open(src / name, newline="", encoding="utf-8-sig")
    z = zipfile.ZipFile(src)
    return io.TextIOWrapper(z.open(name), encoding="utf-8-sig", newline="")


def parent(stop_id):
    if len(stop_id) == 4 and stop_id[3] in "NS":
        return stop_id[:3]
    return stop_id


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gtfs", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=ROOT / "local_reference/phase4e/pattern_index.tsv")
    args = ap.parse_args()

    trips = {}  # static trip_id -> (shape, route)
    with open_table(args.gtfs, "trips.txt") as f:
        for r in csv.DictReader(f):
            shape = r["shape_id"]
            trips[r["trip_id"]] = (shape, r["route_id"])

    seqs = defaultdict(list)
    with open_table(args.gtfs, "stop_times.txt") as f:
        for r in csv.DictReader(f):
            if r["trip_id"] in trips:
                seqs[r["trip_id"]].append((int(r["stop_sequence"]), parent(r["stop_id"])))

    lines = set()
    for tid, items in seqs.items():
        shape, route = trips[tid]
        m = SHAPE_DIR.match(shape)
        if not m:
            continue
        stops = [s for _, s in sorted(items)]
        lines.add((shape, route, m.group(1), ",".join(stops)))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        for line in sorted(lines):
            f.write("\t".join(line) + "\n")
    print(f"wrote {len(lines)} patterns from {len(trips)} trips to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
