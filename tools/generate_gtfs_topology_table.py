#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Generate the firmware GTFS stop-predecessor topology table (Phase 4F).

GTFS-RT never reports past stops, so the previous stop of an in-transit train
must come from static topology. This tool condenses the official static GTFS
into a small heap-free table:

    (GTFS route id, direction N/S, destination parent stop)
        -> set of distinct predecessor parent stops (+ "is a pattern origin")

It has no LED / semantic knowledge. Destinations are restricted to the GTFS
parent stops in data/gtfs_stop_map.csv (the only stops the adapter can place);
predecessors are stored verbatim even when they are off-map so boundary
behaviour is preserved.

Two stages:
  --gtfs DIR_OR_ZIP   rebuild data/gtfs_topology.csv from static GTFS
                      (records sha256 of trips/stop_times/stops + feed_version)
  (default)           generate the C++ table from data/gtfs_topology.csv
  --check             verify the generated files match the CSV (offline)

Output is deterministic and timestamp-free.
"""

import argparse
import csv
import hashlib
import io
import re
import sys
import zipfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOPO_CSV = ROOT / "data/gtfs_topology.csv"
STOP_MAP = ROOT / "data/gtfs_stop_map.csv"
OUT_H = ROOT / "include/generated/gtfs_topology_table.h"
OUT_CPP = ROOT / "src/generated/gtfs_topology_table.cpp"

ROUTES = ["2", "3", "4", "5", "A", "B", "C", "D", "F", "FS", "G", "H", "J", "L",
          "M", "N", "Q", "R", "S", "Z"]
STOP_RE = re.compile(r"[0-9A-Z]{3}")
SHAPE_DIR = re.compile(r"^[A-Za-z0-9]+\.+([NS])")
HASHED_FILES = ["trips.txt", "stop_times.txt", "stops.txt"]
MAX_PREDS = 4


class GenError(Exception):
    pass


def norm(data):
    return data.replace(b"\r\n", b"\n")


def read_lf(path):
    return norm(path.read_bytes()).decode("utf-8")


def mapped_stops():
    with open(STOP_MAP, newline="", encoding="utf-8") as f:
        return {r["gtfs_parent_stop_id"] for r in csv.DictReader(io.StringIO(read_lf(STOP_MAP)))}


def open_member(src, name):
    if src.is_dir():
        return (src / name).read_bytes()
    with zipfile.ZipFile(src) as z:
        return z.read(name)


def parent(stop_id):
    if len(stop_id) == 4 and stop_id[3] in "NS":
        return stop_id[:3]
    return stop_id


def rebuild_csv(gtfs):
    raw = {n: open_member(gtfs, n) for n in HASHED_FILES}
    hashes = {n: hashlib.sha256(norm(raw[n])).hexdigest().upper() for n in HASHED_FILES}
    feed_version = ""
    try:
        info = list(csv.DictReader(io.StringIO(open_member(gtfs, "feed_info.txt").decode("utf-8-sig"))))
        if info:
            feed_version = info[0].get("feed_version", "")
    except (KeyError, FileNotFoundError):
        pass

    trips = {}
    for r in csv.DictReader(io.StringIO(raw["trips.txt"].decode("utf-8-sig"))):
        trips[r["trip_id"]] = (r["shape_id"], r["route_id"])
    seqs = defaultdict(list)
    for r in csv.DictReader(io.StringIO(raw["stop_times.txt"].decode("utf-8-sig"))):
        if r["trip_id"] in trips:
            seqs[r["trip_id"]].append((int(r["stop_sequence"]), parent(r["stop_id"])))

    patterns = set()
    for tid, items in seqs.items():
        shape, route = trips[tid]
        m = SHAPE_DIR.match(shape)
        if route not in ROUTES or not m:
            continue
        stops = []
        for _, s in sorted(items):
            if not stops or stops[-1] != s:
                stops.append(s)
        patterns.add((route, m.group(1), tuple(stops)))

    on_map = mapped_stops()
    preds = defaultdict(set)
    origin = defaultdict(bool)
    for route, d, stops in patterns:
        for i, s in enumerate(stops):
            if s not in on_map:
                continue
            key = (route, d, s)
            if i == 0:
                origin[key] = True
            else:
                preds[key].add(stops[i - 1])
            preds[key]  # ensure key exists
    keys = sorted(set(preds) | set(origin), key=lambda k: (ROUTES.index(k[0]), k[1], k[2]))

    lines = [f"# feed_version={feed_version}",
             f"# patterns={len(patterns)}"]
    for n in HASHED_FILES:
        lines.append(f"# sha256_{n}={hashes[n]}")
    lines.append("route,direction,dest,origin,predecessors")
    for key in keys:
        p = sorted(preds.get(key, ()))
        lines.append(",".join([key[0], key[1], key[2], "1" if origin.get(key) else "0", "|".join(p)]))
    TOPO_CSV.write_bytes(("\n".join(lines) + "\n").encode("utf-8"))
    print(f"wrote {TOPO_CSV.relative_to(ROOT)}: {len(keys)} entries from {len(patterns)} patterns")


def load_csv():
    text = read_lf(TOPO_CSV)
    meta = {}
    rows = []
    body = []
    for line in text.split("\n"):
        if line.startswith("#"):
            k, _, v = line[1:].strip().partition("=")
            meta[k] = v
        elif line:
            body.append(line)
    if body[0] != "route,direction,dest,origin,predecessors":
        raise GenError("unexpected gtfs_topology.csv header")
    seen = set()
    for r in csv.DictReader(io.StringIO("\n".join(body))):
        route, d, dest = r["route"], r["direction"], r["dest"]
        if route not in ROUTES or d not in "NS" or len(d) != 1 or not STOP_RE.fullmatch(dest):
            raise GenError(f"bad key {route}/{d}/{dest}")
        preds = [p for p in r["predecessors"].split("|") if p]
        if any(not STOP_RE.fullmatch(p) for p in preds) or preds != sorted(set(preds)):
            raise GenError(f"bad predecessors for {route}/{d}/{dest}")
        if len(preds) > MAX_PREDS:
            raise GenError(f"too many predecessors for {route}/{d}/{dest}")
        origin = r["origin"] == "1"
        if not preds and not origin:
            raise GenError(f"{route}/{d}/{dest} has neither predecessor nor origin")
        key = (ROUTES.index(route), d, dest)
        if key in seen:
            raise GenError(f"duplicate key {route}/{d}/{dest}")
        seen.add(key)
        rows.append((key, origin, preds))
    rows.sort(key=lambda x: (x[0][0], x[0][1], x[0][2]))
    return meta, rows


def b36(code):
    v = 0
    for ch in code:
        v = v * 36 + "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ".index(ch)
    return v


def render(meta, rows):
    ents, pool = [], []
    for (ri, d, dest), origin, preds in rows:
        ents.append((ri * 2 + (1 if d == "S" else 0), b36(dest), (0x80 if origin else 0) | len(preds), len(pool), dest))
        pool.extend(b36(p) for p in preds)
    # sort key must match the C++ binary search: (routeDir, dest packed value)
    ents.sort(key=lambda e: (e[0], e[1]))
    if len(pool) > 0xFFFF:
        raise GenError("predecessor pool exceeds uint16 offsets")
    n_amb = sum(1 for _, o, p in rows if len(p) > 1)
    n_origin = sum(1 for _, o, p in rows if not p)
    banner = ["// AUTO-GENERATED -- DO NOT EDIT",
              "// Generator: tools/generate_gtfs_topology_table.py",
              f"// Source: {TOPO_CSV.relative_to(ROOT).as_posix()}",
              f"// Static GTFS feed_version: {meta.get('feed_version', '')}"]
    for n in HASHED_FILES:
        banner.append(f"// sha256({n}): {meta.get('sha256_' + n, '')}")
    banner.append("// Regenerate: python tools/generate_gtfs_topology_table.py")
    banner = "\n".join(banner) + "\n"
    h = f"""{banner}
#pragma once

#include <stdint.h>

namespace brooklyn::gtfs_topology {{

constexpr uint16_t kTopologyEntryCount = {len(ents)};
constexpr uint16_t kTopologyPredecessorCount = {len(pool)};
constexpr uint8_t kTopologyRouteCount = {len(ROUTES)};
constexpr uint8_t kTopologyOriginFlag = 0x80;
constexpr uint8_t kTopologyCountMask = 0x0F;

// GTFS route ids covered, indexed by routeDir >> 1.
extern const char *const kTopologyRoutes[kTopologyRouteCount];

// Key (routeDir, dest); sorted ascending for binary search.
//   routeDir : (route index << 1) | (direction == 'S')
//   dest     : GTFS parent stop id packed base-36 (3 chars, 0-9A-Z)
//   flags    : kTopologyOriginFlag if the stop starts some pattern, low
//              nibble = number of distinct predecessors
//   predOff  : first index into kTopologyPredecessors (packed base-36)
struct TopologyEntry {{
  uint16_t dest;
  uint8_t routeDir;
  uint8_t flags;
  uint16_t predOff;
}};

extern const TopologyEntry kTopology[kTopologyEntryCount];
extern const uint16_t kTopologyPredecessors[kTopologyPredecessorCount];

}}  // namespace brooklyn::gtfs_topology
"""
    out = [banner, '#include "generated/gtfs_topology_table.h"\n',
           "namespace brooklyn::gtfs_topology {\n",
           "const char *const kTopologyRoutes[kTopologyRouteCount] = {"
           + ", ".join(f'"{r}"' for r in ROUTES) + "};\n",
           "const TopologyEntry kTopology[kTopologyEntryCount] = {"]
    for rd, dest, flags, off, name in ents:
        out.append(f"    {{{dest}, {rd}, 0x{flags:02X}, {off}}},  // {ROUTES[rd >> 1]} {'NS'[rd & 1]} {name}")
    out.append("};\n")
    out.append("const uint16_t kTopologyPredecessors[kTopologyPredecessorCount] = {")
    for i in range(0, len(pool), 12):
        out.append("    " + ", ".join(str(v) for v in pool[i:i + 12]) + ",")
    out.append("};\n")
    out.append("}  // namespace brooklyn::gtfs_topology\n")
    cpp = "\n".join(out)
    stats = (len(ents), len(pool), n_amb, n_origin)
    return h, cpp, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gtfs", type=Path, help="rebuild the CSV from static GTFS (dir or zip)")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    try:
        if args.gtfs:
            rebuild_csv(args.gtfs)
        meta, rows = load_csv()
        h, cpp, (n, npool, namb, norig) = render(meta, rows)
    except GenError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    h_b, c_b = h.replace("\r\n", "\n").encode(), cpp.replace("\r\n", "\n").encode()
    if args.check:
        ok = (OUT_H.exists() and OUT_CPP.exists() and norm(OUT_H.read_bytes()) == h_b
              and norm(OUT_CPP.read_bytes()) == c_b)
        print(f"gtfs topology table: {'up to date' if ok else 'STALE'} "
              f"({n} entries, {npool} predecessors, {namb} ambiguous, {norig} origin-only)")
        return 0 if ok else 1
    OUT_H.write_bytes(h_b)
    OUT_CPP.write_bytes(c_b)
    flash = n * 6 + npool * 2
    print(f"generated {n} entries, {npool} predecessor words, {namb} ambiguous, "
          f"{norig} origin-only; ~{flash} bytes flash")
    return 0


if __name__ == "__main__":
    sys.exit(main())
