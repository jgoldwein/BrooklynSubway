# Phase 4A: Generated Semantic Lookup Tables

Data generation only. The tables are **not** wired into rendering, LED output
stays disarmed, and no runtime behavior changes.

## Source of truth (never edited by hand)

| File | Role |
|---|---|
| `docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv` | 306 addressable LED rows (302 map + 4 status) with semantics |
| `docs/mapping/phase3b/station_registry.csv` | 188 station entities |
| `data/led_chain_map.csv` | Accepted physical chain, cross-checked one-to-one |

Frozen baseline: tag `phase-3b-semantic-complete`, commit
`f6622cf1af7621c3135088eb217841271b7b5f0f`.

## Files

| File | Purpose |
|---|---|
| `tools/generate_semantic_tables.py` | Generator and fail-loud validation gates |
| `tools/validate_semantic_tables.py` | Independent re-parse of generated C++, spot checks, negative tests, size estimate |
| `include/generated/semantic_tables.h` | Generated: types, constants, declarations |
| `src/generated/semantic_tables.cpp` | Generated: data and three lookup functions |

Generated files carry an `AUTO-GENERATED -- DO NOT EDIT` banner, the baseline tag
and commit, and SHA-256 hashes of the inputs (line endings normalized to LF). No
timestamps, so identical inputs give byte-identical output.

## Regenerate and validate

```powershell
python tools\generate_semantic_tables.py           # write generated files
python tools\generate_semantic_tables.py --check   # exit 1 if files are stale
python tools\validate_semantic_tables.py           # full independent validation
```

## Schema (namespace `brooklyn::semantic`)

```cpp
enum class Route : uint8_t { None = 0, Route2 = 1, Route3, Route4, Route5,
  RouteA, RouteB, RouteC, RouteD, RouteF, RouteG, RouteJ, RouteL, RouteM,
  RouteN, RouteQ, RouteR, RouteS, RouteZ = 18 };
enum class Role : uint8_t { Station = 1, Segment = 2 };

struct Station { uint16_t id; const char *name; const char *gtfsParentStopId; };

struct MapLed {            // 14 bytes
  uint16_t designator;     // N in "LEDN"
  uint16_t globalOrder;    // 0..305
  uint16_t stationId;      // STATION only, else 0
  uint16_t endpointA;      // SEGMENT only, else 0
  uint16_t endpointB;      // SEGMENT only, else 0
  uint8_t  channel;        // 1..8
  uint8_t  channelIndex;   // 0-based
  Route route;
  Role role;
};
```

Route order is deterministic: digits, then letters
(`2 3 4 5 A B C D F G J L M N Q R S Z`). `kRouteNames[]` maps enum to text
(`[0] == ""`).

Tables and functions:

- `kStations[188]` indexed by `id - 1` (IDs are contiguous 1..188; the generator
  fails if that ever stops being true). `gtfsParentStopId` is copied from the
  registry, `nullptr` when blank; nothing is invented. `station_key` and
  `notes` are not emitted (debug/doc only).
- `kMapLeds[302]` sorted by global physical order. Segment LEDs have
  `stationId == 0`; they never get a station identity.
- `findStation(id)`, `findMapLed(channel, index)` (binary search on global
  order; `nullptr` for status LEDs/out of range), and
  `stationLedIndices(id, count)` (CSR reverse index; one entry per route that
  serves the station, 0 for endpoint-only stations).
- Reverse segment lookup is intentionally omitted (26 records; linear scan is
  cheap and no runtime need exists yet).

## Counts

Station registry IDs are append-only and sequential. Generated station lookup currently relies on IDs being contiguous beginning at 1.

302 map LEDs = 276 STATION + 26 SEGMENT; 188 registry entities (185 with a
station LED, 3 endpoint-only); 18 routes. The 4 status LEDs (LED251, 252, 306,
307 on channel 8, indexes 0-3) and discrete LED U23 are excluded.

## Generator gates (all fail loudly, exit code 1)

Map count != 302; STATION != 276; SEGMENT != 26; registry != 188 or non-contiguous;
any non-VERIFIED row; duplicate designator, channel/index or global order;
missing/out-of-range channel or index; global order inconsistent with channel
layout `{37,38,37,37,42,37,38,40}`; mismatch with `data/led_chain_map.csv`;
unsupported route; STATION missing registry ID or name/key mismatch; duplicate
route/station pair; SEGMENT with its own station identity, missing endpoint, name
or key mismatch, or endpoint A == B; status LED among map rows; any `U` designator
(U23); accepted-chain map LEDs not represented exactly once.

`validate_semantic_tables.py` re-reads the generated C++, compares it with the
CSVs, `data/led_chain_map.csv` and the designator table compiled into
`src/led_map.cpp`, verifies the reverse index, checks the human-confirmed LEDs
(LED96, 285, 286, 287, 291, 299, 304, 308, 309), and runs negative tests proving
each gate rejects bad input.

## Memory footprint

Measured from a standalone `xtensa-esp32s3-elf-g++ -Os` object (no firmware
build): `.rodata` 9,765 B + 229 B code = **9,994 B (~9.8 KiB) flash, 0 B
RAM, no heap** (the object's 10,443 B total also counts non-loaded metadata
sections). Largest items: `kMapLeds` 4,228 B, `kStations` 2,256 B, strings
2,251 B. The data is `const` and unreferenced, so linker garbage collection
should drop it until a later phase uses it.
