#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Generate firmware semantic lookup tables from the frozen Phase 3B dataset.

Inputs (authoritative, never modified):
  docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv
  docs/mapping/phase3b/station_registry.csv
Cross-checked against the accepted physical chain:
  data/led_chain_map.csv

Outputs (deterministic, no timestamps):
  include/generated/semantic_tables.h
  src/generated/semantic_tables.cpp

Usage:
  python tools/generate_semantic_tables.py          # (re)generate
  python tools/generate_semantic_tables.py --check  # fail if files are stale
"""

import argparse
import csv
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKSHEET = ROOT / "docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv"
REGISTRY = ROOT / "docs/mapping/phase3b/station_registry.csv"
CHAIN = ROOT / "data/led_chain_map.csv"
OUT_H = ROOT / "include/generated/semantic_tables.h"
OUT_CPP = ROOT / "src/generated/semantic_tables.cpp"

BASELINE_TAG = "phase-3b-semantic-complete"
BASELINE_COMMIT = "f6622cf1af7621c3135088eb217841271b7b5f0f"

EXPECT_MAP = 302
EXPECT_STATION = 276
EXPECT_SEGMENT = 26
EXPECT_REGISTRY = 188
EXPECT_STATUS = 4
CHANNEL_COUNTS = [37, 38, 37, 37, 42, 37, 38, 40]
TOTAL_LEDS = sum(CHANNEL_COUNTS)
STATUS_DESIGNATORS = {"LED252": (8, 0), "LED251": (8, 1),
                      "LED306": (8, 2), "LED307": (8, 3)}

# Deterministic route order: digits then letters. Enum value = position + 1.
ROUTES = ["2", "3", "4", "5", "A", "B", "C", "D", "F", "G", "J", "L", "M",
          "N", "Q", "R", "S", "Z"]
ROUTE_ENUM = {r: f"Route{r}" for r in ROUTES}


class GenerationError(Exception):
    pass


def fail(message):
    raise GenerationError(message)


def read_text_normalized(path):
    return path.read_bytes().decode("utf-8-sig").replace("\r\n", "\n")


def sha256_normalized(path):
    return hashlib.sha256(read_text_normalized(path).encode("utf-8")).hexdigest()


def read_csv(path):
    text = read_text_normalized(path)
    return list(csv.DictReader(text.splitlines()))


def c_string(value):
    out = []
    for byte in value.encode("utf-8"):
        ch = chr(byte)
        if ch == '"' or ch == "\\":
            out.append("\\" + ch)
        elif 0x20 <= byte < 0x7F:
            out.append(ch)
        else:
            out.append("\\%03o" % byte)
    return '"' + "".join(out) + '"'


def designator_number(designator):
    match = re.fullmatch(r"LED(\d+)", designator)
    if not match:
        fail(f"bad designator {designator!r}")
    return int(match.group(1))


def load_registry():
    rows = read_csv(REGISTRY)
    if len(rows) != EXPECT_REGISTRY:
        fail(f"registry entity count {len(rows)} != {EXPECT_REGISTRY}")
    registry = {}
    for row in rows:
        sid = int(row["station_id"])
        if sid in registry:
            fail(f"duplicate registry station_id {sid}")
        if not row["display_name"].strip():
            fail(f"registry id {sid} has empty display_name")
        registry[sid] = row
    ids = sorted(registry)
    if ids != list(range(1, EXPECT_REGISTRY + 1)):
        fail("registry ids must be contiguous 1..188 (runtime lookup indexes by id-1)")
    return registry


def load_chain():
    chain = {}
    for row in read_csv(CHAIN):
        key = int(row["global_physical_order"])
        chain[key] = (int(row["channel"]), int(row["channel_index"]),
                      row["designator"], row["role"])
    if len(chain) != TOTAL_LEDS:
        fail(f"accepted chain has {len(chain)} LEDs, expected {TOTAL_LEDS}")
    return chain


def build():
    registry = load_registry()
    chain = load_chain()
    chain_by_designator = {v[2]: (k, v) for k, v in chain.items()}
    worksheet = read_csv(WORKSHEET)

    map_rows = [r for r in worksheet if r["role"] == "map"]
    other_rows = [r for r in worksheet if r["role"] != "map"]

    if len(map_rows) != EXPECT_MAP:
        fail(f"map LED count {len(map_rows)} != {EXPECT_MAP}")
    if len(other_rows) != EXPECT_STATUS:
        fail(f"non-map row count {len(other_rows)} != {EXPECT_STATUS}")

    for row in other_rows:
        des = row["designator"]
        if row["role"] not in ("status_system", "status_network",
                               "status_mta_feed", "status_mode") or \
                des not in STATUS_DESIGNATORS:
            fail(f"unexpected non-map row {des} role={row['role']}")
        if (int(row["channel"]), int(row["channel_index"])) != STATUS_DESIGNATORS[des]:
            fail(f"status LED {des} address mismatch")
    for row in worksheet:
        if row["designator"].upper().startswith("U"):
            fail(f"discrete LED {row['designator']} present in addressable worksheet")

    stations = [r for r in map_rows if r["element_type"] == "STATION"]
    segments = [r for r in map_rows if r["element_type"] == "SEGMENT"]
    if len(stations) + len(segments) != EXPECT_MAP:
        fail("map rows with element_type other than STATION/SEGMENT exist")
    if len(stations) != EXPECT_STATION:
        fail(f"STATION count {len(stations)} != {EXPECT_STATION}")
    if len(segments) != EXPECT_SEGMENT:
        fail(f"SEGMENT count {len(segments)} != {EXPECT_SEGMENT}")

    seen_designator = set()
    seen_address = set()
    seen_global = set()
    seen_station_route = set()
    records = []

    for row in map_rows:
        des = row["designator"]
        if row["semantic_status"] != "VERIFIED":
            fail(f"{des} semantic_status={row['semantic_status']!r}, not VERIFIED")
        if des in STATUS_DESIGNATORS:
            fail(f"status LED {des} included as map LED")
        if des in seen_designator:
            fail(f"duplicate designator {des}")
        seen_designator.add(des)

        if not row["channel"].strip() or not row["channel_index"].strip() \
                or not row["global_physical_order"].strip():
            fail(f"{des} missing channel/index/global order")
        channel = int(row["channel"])
        index = int(row["channel_index"])
        global_order = int(row["global_physical_order"])
        if not 1 <= channel <= 8 or not 0 <= index < CHANNEL_COUNTS[channel - 1]:
            fail(f"{des} channel/index out of range: {channel}/{index}")
        if (channel, index) in seen_address:
            fail(f"duplicate physical channel/index {channel}/{index}")
        seen_address.add((channel, index))
        if global_order in seen_global:
            fail(f"duplicate global order {global_order}")
        seen_global.add(global_order)
        if global_order != sum(CHANNEL_COUNTS[:channel - 1]) + index:
            fail(f"{des} global order inconsistent with channel/index")

        accepted = chain.get(global_order)
        if accepted is None or accepted != (channel, index, des, "map"):
            fail(f"{des} does not match accepted physical chain "
                 f"(worksheet {(channel, index, des)} vs chain {accepted})")

        route = row["route"]
        if route not in ROUTE_ENUM:
            fail(f"{des} unsupported route {route!r}")

        station_id = endpoint_a = endpoint_b = 0
        if row["element_type"] == "STATION":
            if row["from_station_id"] or row["to_station_id"]:
                fail(f"{des} STATION row carries endpoint IDs")
            station_id = int(row["station_id"] or 0)
            if station_id not in registry:
                fail(f"{des} references missing registry station {row['station_id']!r}")
            reg = registry[station_id]
            if reg["station_key"] != row["station_key"]:
                fail(f"{des} station_key {row['station_key']!r} != registry")
            if reg["display_name"] != row["station_name"]:
                fail(f"{des} station_name {row['station_name']!r} != registry "
                     f"{reg['display_name']!r}")
            if (route, station_id) in seen_station_route:
                fail(f"{des} duplicate route/station assignment {route}/{station_id}")
            seen_station_route.add((route, station_id))
            role = "Station"
        else:
            if row["station_id"] or row["station_key"]:
                fail(f"{des} SEGMENT row has a station identity of its own")
            if not row["from_station_id"] or not row["to_station_id"]:
                fail(f"{des} segment missing endpoint IDs")
            endpoint_a = int(row["from_station_id"])
            endpoint_b = int(row["to_station_id"])
            for sid, key_col, name_col in ((endpoint_a, "from_station_key", "from_station"),
                                           (endpoint_b, "to_station_key", "to_station")):
                if sid not in registry:
                    fail(f"{des} segment endpoint {sid} missing from registry")
                if registry[sid]["station_key"] != row[key_col]:
                    fail(f"{des} {key_col} mismatch for endpoint {sid}")
                if registry[sid]["display_name"] != row[name_col]:
                    fail(f"{des} {name_col} {row[name_col]!r} != registry "
                         f"{registry[sid]['display_name']!r}")
            if endpoint_a == endpoint_b:
                fail(f"{des} segment endpoints are identical ({endpoint_a})")
            role = "Segment"

        records.append({
            "designator": designator_number(des), "des": des,
            "global": global_order, "channel": channel, "index": index,
            "route": route, "role": role, "station": station_id,
            "a": endpoint_a, "b": endpoint_b,
        })

    # Every accepted-chain map LED must be represented exactly once.
    chain_map = {v[2] for v in chain.values() if v[3] == "map"}
    if chain_map != seen_designator:
        fail("generated map LEDs are not one-to-one with the accepted chain map LEDs: "
             f"missing={sorted(chain_map - seen_designator)} "
             f"extra={sorted(seen_designator - chain_map)}")

    records.sort(key=lambda r: r["global"])
    return registry, records


def render(registry, records):
    src_hash_ws = sha256_normalized(WORKSHEET)
    src_hash_reg = sha256_normalized(REGISTRY)
    src_hash_chain = sha256_normalized(CHAIN)

    station_leds = {sid: [] for sid in registry}
    for i, rec in enumerate(records):
        if rec["role"] == "Station":
            station_leds[rec["station"]].append(i)
    offsets = [0]
    flat = []
    for sid in range(1, EXPECT_REGISTRY + 1):
        flat.extend(station_leds[sid])
        offsets.append(len(flat))
    if len(flat) != EXPECT_STATION:
        fail("station->LED index size mismatch")

    banner = f"""\
// AUTO-GENERATED -- DO NOT EDIT
// Source: Phase 3B semantic mapping
// Baseline tag: {BASELINE_TAG}
// Baseline commit: {BASELINE_COMMIT}
// Generator: tools/generate_semantic_tables.py
// Source SHA-256 (line endings normalized to LF):
//   {WORKSHEET.name}: {src_hash_ws}
//   {REGISTRY.name}: {src_hash_reg}
//   {CHAIN.relative_to(ROOT).as_posix()}: {src_hash_chain}
// Regenerate with: python tools/generate_semantic_tables.py
"""

    n_stations_with_leds = sum(1 for s in station_leds.values() if s)
    route_enum_lines = ["  None = 0,"] + [
        f"  {ROUTE_ENUM[r]} = {i + 1}," for i, r in enumerate(ROUTES)]

    header = f"""{banner}
#pragma once

#include <stdint.h>

namespace brooklyn::semantic {{

constexpr uint16_t kMapLedCount = {len(records)};
constexpr uint16_t kStationLedCount = {sum(1 for r in records if r['role'] == 'Station')};
constexpr uint16_t kSegmentLedCount = {sum(1 for r in records if r['role'] == 'Segment')};
constexpr uint16_t kRegistryCount = {len(registry)};
constexpr uint16_t kRouteCount = {len(ROUTES)};
constexpr uint16_t kPhysicalLedCount = {TOTAL_LEDS};

// Registry station IDs are contiguous 1..kRegistryCount. 0 means "none".
constexpr uint16_t kNoStation = 0;

enum class Route : uint8_t {{
{chr(10).join(route_enum_lines)}
}};

enum class Role : uint8_t {{
  Station = 1,
  Segment = 2,
}};

struct Station {{
  uint16_t id;
  const char *name;           // canonical display name (UTF-8)
  const char *gtfsParentStopId;  // nullptr when the registry has none
}};

// One record per physical map LED, sorted by global physical order.
// STATION: stationId != 0, endpointA == endpointB == 0.
// SEGMENT: stationId == 0, endpointA/endpointB are registry station IDs.
struct MapLed {{
  uint16_t designator;    // N in "LEDN"
  uint16_t globalOrder;   // 0..305 across channels 1..8
  uint16_t stationId;
  uint16_t endpointA;
  uint16_t endpointB;
  uint8_t channel;        // 1..8
  uint8_t channelIndex;   // 0-based within the channel
  Route route;
  Role role;
}};

extern const char *const kRouteNames[kRouteCount + 1];  // [0] == ""
extern const Station kStations[kRegistryCount];
extern const MapLed kMapLeds[kMapLedCount];

// Returns nullptr for unknown IDs.
const Station *findStation(uint16_t stationId);

// Returns nullptr for status LEDs and out-of-range addresses.
const MapLed *findMapLed(uint8_t channel, uint8_t channelIndex);

// Indices into kMapLeds of the STATION LEDs for a registry station
// (one per route that serves it). count == 0 for endpoint-only stations.
const uint16_t *stationLedIndices(uint16_t stationId, uint16_t &count);

}}  // namespace brooklyn::semantic
"""

    L = [banner, '#include "generated/semantic_tables.h"\n',
         "namespace brooklyn::semantic {\n"]

    L.append("namespace {\n")
    L.append(f"constexpr uint16_t kChannelOffsets[8] = "
             f"{{{', '.join(str(sum(CHANNEL_COUNTS[:i])) for i in range(8))}}};")
    L.append(f"constexpr uint8_t kChannelCounts[8] = "
             f"{{{', '.join(str(c) for c in CHANNEL_COUNTS)}}};")
    L.append("}  // namespace\n")

    L.append("const char *const kRouteNames[kRouteCount + 1] = {\"\", " +
             ", ".join(c_string(r) for r in ROUTES) + "};\n")

    L.append("const Station kStations[kRegistryCount] = {")
    for sid in range(1, EXPECT_REGISTRY + 1):
        row = registry[sid]
        gtfs = row["gtfs_parent_stop_id"].strip()
        gtfs_c = c_string(gtfs) if gtfs else "nullptr"
        L.append(f"    {{{sid}, {c_string(row['display_name'])}, {gtfs_c}}},")
    L.append("};\n")

    L.append("const MapLed kMapLeds[kMapLedCount] = {")
    L.append("    // designator, global, station, endpointA, endpointB, ch, idx, route, role")
    for r in records:
        L.append(f"    {{{r['designator']}, {r['global']}, {r['station']}, {r['a']}, "
                 f"{r['b']}, {r['channel']}, {r['index']}, Route::{ROUTE_ENUM[r['route']]}, "
                 f"Role::{r['role']}}},  // {r['des']} {r['route']} {r['role'].upper()}")
    L.append("};\n")

    L.append("namespace {")
    L.append("// CSR-style reverse index: leds for station id live at "
             "[kStationLedOffsets[id-1], kStationLedOffsets[id]).")
    L.append(f"constexpr uint16_t kStationLedOffsets[kRegistryCount + 1] = {{")
    for i in range(0, len(offsets), 16):
        L.append("    " + ", ".join(str(v) for v in offsets[i:i + 16]) + ",")
    L.append("};")
    L.append(f"constexpr uint16_t kStationLedRecords[kStationLedCount] = {{")
    for i in range(0, len(flat), 16):
        L.append("    " + ", ".join(str(v) for v in flat[i:i + 16]) + ",")
    L.append("};")
    L.append("}  // namespace\n")

    L.append(f"""\
static_assert(sizeof(MapLed) == 14, "MapLed layout changed");
static_assert(kChannelOffsets[7] + kChannelCounts[7] == kPhysicalLedCount,
              "channel layout must total kPhysicalLedCount");

const Station *findStation(uint16_t stationId) {{
  if (stationId == kNoStation || stationId > kRegistryCount) {{
    return nullptr;
  }}
  return &kStations[stationId - 1];
}}

const MapLed *findMapLed(uint8_t channel, uint8_t channelIndex) {{
  if (channel < 1 || channel > 8 || channelIndex >= kChannelCounts[channel - 1]) {{
    return nullptr;
  }}
  const uint16_t global = kChannelOffsets[channel - 1] + channelIndex;
  uint16_t low = 0;
  uint16_t high = kMapLedCount;
  while (low < high) {{
    const uint16_t mid = static_cast<uint16_t>((low + high) / 2);
    if (kMapLeds[mid].globalOrder < global) {{
      low = static_cast<uint16_t>(mid + 1);
    }} else {{
      high = mid;
    }}
  }}
  if (low < kMapLedCount && kMapLeds[low].globalOrder == global) {{
    return &kMapLeds[low];
  }}
  return nullptr;
}}

const uint16_t *stationLedIndices(uint16_t stationId, uint16_t &count) {{
  if (stationId == kNoStation || stationId > kRegistryCount) {{
    count = 0;
    return nullptr;
  }}
  const uint16_t begin = kStationLedOffsets[stationId - 1];
  count = static_cast<uint16_t>(kStationLedOffsets[stationId] - begin);
  return &kStationLedRecords[begin];
}}

}}  // namespace brooklyn::semantic""")

    return header, "\n".join(L) + "\n", n_stations_with_leds


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="verify generated files on disk are current")
    args = parser.parse_args()
    try:
        registry, records = build()
        header, source, with_leds = render(registry, records)
    except GenerationError as error:
        print(f"GENERATION FAILED: {error}", file=sys.stderr)
        return 1

    if args.check:
        stale = [p for p, t in ((OUT_H, header), (OUT_CPP, source))
                 if not p.exists() or p.read_bytes().decode("utf-8") != t]
        if stale:
            print("STALE: " + ", ".join(str(p.relative_to(ROOT)) for p in stale),
                  file=sys.stderr)
            return 1
        print("generated files are current")
        return 0

    write(OUT_H, header)
    write(OUT_CPP, source)
    stations = sum(1 for r in records if r["role"] == "Station")
    print(f"map LEDs={len(records)} STATION={stations} "
          f"SEGMENT={len(records) - stations} registry={len(registry)} "
          f"stations_with_leds={with_leds}")
    print(f"wrote {OUT_H.relative_to(ROOT)}")
    print(f"wrote {OUT_CPP.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
