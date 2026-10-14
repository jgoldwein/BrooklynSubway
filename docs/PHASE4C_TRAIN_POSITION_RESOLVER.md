# Phase 4C - Train-Position Resolver

## Purpose

A deterministic, software-only resolver that answers: *which physical map LED, if any, represents this train position?* It consumes the Phase 4A generated tables (through the Phase 4B conventions) and returns a physical identity. **It never illuminates an LED**, touches no LED buffer, calls no setter/show routine, and is not yet connected to GTFS or any renderer. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT` remains `0`; GPIO1 / `LED_ENABLE` behavior is unchanged.

Files:

- `include/train_position_resolver.h`, `src/train_position_resolver.cpp` - resolver (namespace `brooklyn::semantic`).
- `tools/host_tests/resolver_test.cpp`, `tools/run_resolver_tests.py` - hardware-free host tests.

## Normalized input model

```cpp
struct TrainMapPosition {
  Route route;            // generated route enum, Route::None is invalid
  PositionKind kind;      // Station | Segment (Invalid = 0)
  uint16_t stationId;     // Station: registry ID (1..188)
  uint16_t endpointA;     // Segment: registry ID
  uint16_t endpointB;     // Segment: registry ID
  uint8_t segmentSlot;    // Segment: display slot, kAnySegmentSlot (0xFF) = not specified
};
```

Helpers `stationPosition(route, id)` and `segmentPosition(route, a, b, slot = kAnySegmentSlot)` build these. No strings are used at runtime.

## API

```cpp
ResolveResult resolveTrainPosition(const TrainMapPosition&);
ResolveResult resolveTrainPositionIn(const MapLed* table, uint16_t count,
                                     const TrainMapPosition&);  // test seam
struct ResolveResult {
  ResolveStatus status;           // Match | NoMatch | Ambiguous | Invalid
  uint16_t globalIndex;           // kNoGlobalIndex (0xFFFF) unless Match
  uint8_t channel, channelIndex;  // 0 unless Match
  const MapLed* semantic;         // nullptr unless Match (read-only)
};
```

## Status semantics

| Status | Meaning |
| --- | --- |
| `Match` | Exactly one map LED satisfies the query. Result carries its physical identity. |
| `NoMatch` | Query is well-formed but no LED represents it (wrong route, endpoint-only station, adjacent stations with no segment LED). |
| `Ambiguous` | More than one LED satisfies the query (e.g. a multi-pixel segment queried without a slot). The first one is never silently chosen; result carries no LED. |
| `Invalid` | Malformed query: `Route::None`/out-of-range route, unknown `PositionKind`, station ID outside 1..188, segment endpoint outside 1..188 or equal endpoints, segment slot >= the segment's pixel count (other than `kAnySegmentSlot`), null table. |

## Lookup behavior

- **Station:** route + role `STATION` + station ID. Transfer stations have one LED per route, so each route resolves to its own LED. Segment LEDs are never returned for a station query, even if they mention the station.
- **Segment:** route + role `SEGMENT` + endpoint pair, treated as an **unordered** pair, plus an optional slot (see below). With reversed endpoints the slot is mirrored.
- **No fallback:** no nearest-station, nearest-segment or transfer-LED approximation. The caller decides fallback policy in a later phase.
- Linear scan over 302 records; no heap, no extra reverse table.
- A `Match` is cross-checked in tests through `led::map::toPhysicalAddress` (channel/index/global order must agree); no second physical numbering exists.

## Segment slots

Several physical LEDs may share one route and endpoint pair: a long physical span
(e.g. the Manhattan Bridge for B and D) is drawn with more than one pixel. Each
SEGMENT record therefore carries `segmentSlot` (zero-based position along the
stored endpointA -> endpointB direction) and `segmentSlotCount` (LEDs in that
segment). Single-pixel segments are slot 0 of 1. STATION records carry 0/0.

Source: `data/segment_pixel_order.csv` (all 26 SEGMENT LEDs, keyed by designator),
validated and compiled in by `tools/generate_semantic_tables.py` (see Phase 4A doc).
Phase 3B meaning is unchanged. The two multi-pixel groups use the human-confirmed
artwork order (285/286 are the earlier Manhattan-side pixels):

| Group (stored direction) | Slot 0 | Slot 1 |
| --- | --- | --- |
| B: Grand St -> DeKalb Av | LED285 | LED309 |
| D: Grand St -> Atlantic Av-Barclays Ctr | LED286 | LED308 |

**Direction-aware lookup.** The requested slot counts pixels from the caller's first
endpoint. If the caller's endpoints match the stored direction, slot N is stored
slot N. If reversed, slot N is stored slot `slotCount - 1 - N`. So DeKalb -> Grand St
slot 0 = LED309 and slot 1 = LED285. "First pixel after the station" therefore means
the same thing in either travel direction.

**`kAnySegmentSlot`.** Matches every pixel of the segment: a single-pixel segment gives
`Match`; a multi-pixel segment gives `Ambiguous`. With a specific slot the query must
resolve to exactly one LED, or `Invalid` if the slot is out of range. Later
GTFS/train-progress logic is responsible for choosing the slot.
## Test results (host, no hardware)

`python tools/run_resolver_tests.py` (g++/clang++ if present, else `python -m ziglang cc -x c++`; `-Wall -Wextra -Werror`):

- 276/276 STATION records: route + stationId -> exactly the originating LED.
- 26/26 SEGMENT records: route + endpoints + slot -> exactly the originating LED; reversed endpoints + reversed slot -> the same LED.
- 22 single-pixel segments with `kAnySegmentSlot` -> `Match`; the 4 LEDs of the two two-pixel groups -> `Ambiguous`.
- Explicit B (LED285/LED309) and D (LED286/LED308) tests in both directions; out-of-range slot -> `Invalid`.
- 62 stations with >= 2 route LEDs (incl. Broadway Junction): each route resolves to its own LED.
- Full route x station and route x unordered-pair cross products (3108 and 316,380 NoMatch cases) agree with an independent oracle; 104 adjacent station pairs with no segment LED return `NoMatch`.
- Invalid cases and synthetic-table ambiguity cases (including duplicate stored slots) pass.
- Total 958,614 checks, 0 failures.
## Relationship to other phases

Phase 3B: frozen CSV truth. Phase 4A: generated tables. Phase 4B: read-only semantic API. Phase 4C: this resolver. Planned next: a GTFS adapter that normalizes live feed data into `TrainMapPosition` and a renderer policy for `NoMatch`/`Ambiguous`; physical output stays disarmed until explicitly approved.
