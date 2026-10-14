#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Generate the firmware GTFS parent-stop -> registry-station table (Phase 4D).

Inputs (never modified by this tool):
  data/gtfs_stop_map.csv       Phase 4 supplemental map, built from the official
                               MTA static GTFS stops.txt (see docs/PHASE4D_GTFS_ADAPTER.md)
  data/gtfs_stop_review.csv    registry stations deliberately left unmapped
  docs/mapping/phase3b/station_registry.csv   frozen Phase 3B registry

Outputs (deterministic, no timestamps):
  include/generated/gtfs_stop_table.h
  src/generated/gtfs_stop_table.cpp

Usage:
  python tools/generate_gtfs_stop_table.py          # (re)generate
  python tools/generate_gtfs_stop_table.py --check  # fail if files are stale
"""

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_semantic_tables as sem  # noqa: E402

ROOT = sem.ROOT
STOP_MAP = ROOT / "data/gtfs_stop_map.csv"
STOP_REVIEW = ROOT / "data/gtfs_stop_review.csv"
OUT_H = ROOT / "include/generated/gtfs_stop_table.h"
OUT_CPP = ROOT / "src/generated/gtfs_stop_table.cpp"

STOP_MAP_COLUMNS = ["gtfs_parent_stop_id", "station_id", "registry_name",
                    "gtfs_stop_name", "gtfs_routes", "registry_routes", "lat",
                    "lon", "evidence"]
REVIEW_COLUMNS = ["station_id", "registry_name", "corridor_or_line",
                  "registry_routes", "reason", "candidate_stop_ids"]
EXPECT_STOPS = 197
EXPECT_STATIONS_MAPPED = 188
EXPECT_REVIEW = 0
STOP_ID_RE = re.compile(r"[0-9A-Z]{3}")


def fail(message):
    raise sem.GenerationError(message)


def header_of(path):
    return sem.read_text_normalized(path).split("\n", 1)[0].split(",")


def load():
    registry = sem.load_registry()
    if header_of(STOP_MAP) != STOP_MAP_COLUMNS:
        fail(f"{STOP_MAP.name} header is not {STOP_MAP_COLUMNS}")
    if header_of(STOP_REVIEW) != REVIEW_COLUMNS:
        fail(f"{STOP_REVIEW.name} header is not {REVIEW_COLUMNS}")
    rows = sem.read_csv(STOP_MAP)
    review = sem.read_csv(STOP_REVIEW)

    entries = {}
    mapped = set()
    for row in rows:
        stop = row["gtfs_parent_stop_id"]
        if not STOP_ID_RE.fullmatch(stop):
            fail(f"GTFS parent stop id {stop!r} must be 3 chars of 0-9A-Z")
        if stop[-1] in "NS":
            fail(f"GTFS parent stop id {stop!r} ends in N/S; the platform "
                 "suffix rule would be ambiguous")
        if stop in entries:
            fail(f"duplicate GTFS parent stop id {stop}")
        if not row["station_id"].isdigit() or int(row["station_id"]) not in registry:
            fail(f"{stop}: station_id {row['station_id']!r} not in registry")
        sid = int(row["station_id"])
        if registry[sid]["display_name"] != row["registry_name"]:
            fail(f"{stop}: registry_name {row['registry_name']!r} != registry "
                 f"{registry[sid]['display_name']!r}")
        entries[stop] = sid
        mapped.add(sid)

    for stop in entries:
        if stop[:-1] in entries:
            fail(f"{stop} is a prefix-extension of another parent ({stop[:-1]})")

    # The frozen registry's own GTFS IDs must be reproduced exactly.
    for sid, reg in registry.items():
        have = reg["gtfs_parent_stop_id"].strip()
        if have and entries.get(have) != sid:
            fail(f"registry station {sid} GTFS id {have} missing/different in "
                 f"{STOP_MAP.name}")

    unmapped = set()
    for row in review:
        sid = int(row["station_id"])
        if sid not in registry:
            fail(f"review row references unknown station {sid}")
        if sid in mapped:
            fail(f"station {sid} is both mapped and listed for manual review")
        if registry[sid]["gtfs_parent_stop_id"].strip():
            fail(f"review row {sid} but the registry already has a GTFS id")
        unmapped.add(sid)
    if mapped | unmapped != set(registry):
        fail("stations neither mapped nor in review: "
             f"{sorted(set(registry) - mapped - unmapped)}")

    if len(entries) != EXPECT_STOPS:
        fail(f"GTFS stop rows {len(entries)} != {EXPECT_STOPS}")
    if len(mapped) != EXPECT_STATIONS_MAPPED:
        fail(f"mapped stations {len(mapped)} != {EXPECT_STATIONS_MAPPED}")
    if len(unmapped) != EXPECT_REVIEW:
        fail(f"manual-review stations {len(unmapped)} != {EXPECT_REVIEW}")
    return registry, sorted(entries.items())


def render(entries):
    h_stop = sem.sha256_normalized(STOP_MAP)
    h_review = sem.sha256_normalized(STOP_REVIEW)
    h_reg = sem.sha256_normalized(sem.REGISTRY)
    banner = f"""\
// AUTO-GENERATED -- DO NOT EDIT
// Generator: tools/generate_gtfs_stop_table.py
// Source SHA-256 (line endings normalized to LF):
//   {STOP_MAP.relative_to(ROOT).as_posix()}: {h_stop}
//   {STOP_REVIEW.relative_to(ROOT).as_posix()}: {h_review}
//   {sem.REGISTRY.name}: {h_reg}
// Regenerate with: python tools/generate_gtfs_stop_table.py
"""
    header = f"""{banner}
#pragma once

#include <stdint.h>

namespace brooklyn::semantic {{

constexpr uint16_t kGtfsStopCount = {len(entries)};

// One GTFS *parent* stop (3 chars, e.g. "A41") -> Phase 3B registry station ID.
// Sorted by stopId (byte order) so lookups may binary-search. Several parent
// stops may map to one station (transfer complexes); a stop maps to one station.
struct GtfsStop {{
  char stopId[4];  // 3 characters + NUL
  uint16_t stationId;
}};

extern const GtfsStop kGtfsStops[kGtfsStopCount];

}}  // namespace brooklyn::semantic
"""
    body = ",\n".join(f'    {{{sem.c_string(stop)}, {sid}}}' for stop, sid in entries)
    source = f"""{banner}
#include "generated/gtfs_stop_table.h"

namespace brooklyn::semantic {{

const GtfsStop kGtfsStops[kGtfsStopCount] = {{
{body},
}};

static_assert(sizeof(GtfsStop) == 6, "GtfsStop layout changed");

}}  // namespace brooklyn::semantic
"""
    return header, source


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        registry, entries = load()
        header, source = render(entries)
    except sem.GenerationError as error:
        print(f"GENERATION FAILED: {error}", file=sys.stderr)
        return 1

    if args.check:
        stale = [p for p, t in ((OUT_H, header), (OUT_CPP, source))
                 if not p.exists() or p.read_bytes().decode("utf-8") != t]
        if stale:
            print("STALE: " + ", ".join(str(p.relative_to(ROOT)) for p in stale),
                  file=sys.stderr)
            return 1
        print("generated GTFS stop table is current")
        return 0

    sem.write(OUT_H, header)
    sem.write(OUT_CPP, source)
    print(f"GTFS parent stops={len(entries)} stations="
          f"{len({s for _, s in entries})} registry={len(registry)}")
    print(f"wrote {OUT_H.relative_to(ROOT)}")
    print(f"wrote {OUT_CPP.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
