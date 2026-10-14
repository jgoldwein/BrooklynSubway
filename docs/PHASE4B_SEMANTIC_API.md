# Phase 4B: Semantic API

Integrates the Phase 4A generated tables into the normal firmware build and adds a
thin read-only API. Semantic lookup **does not illuminate LEDs**: nothing here
calls LED output routines, touches GPIO, or changes `LED_ENABLE`. Display and
rendering behavior is unchanged, and `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT` stays `0`
in both environments.

## Data source and ownership

- Source of truth: the Phase 3B CSVs (`phase3b_semantic_mapping_worksheet.csv`,
  `station_registry.csv`), cross-checked against `data/led_chain_map.csv`.
- `include/generated/semantic_tables.h` and `src/generated/semantic_tables.cpp`
  are owned by `tools/generate_semantic_tables.py`. Never edit them by hand; see
  [PHASE4A_SEMANTIC_TABLES.md](PHASE4A_SEMANTIC_TABLES.md).
- `include/semantic_api.h` / `src/semantic_api.cpp` are hand-written and are the
  only intended entry point for application code.
- Station registry IDs are append-only and sequential. Generated station lookup
  currently relies on IDs being contiguous beginning at 1.

## API (`brooklyn::semantic`, `semantic_api.h`)

| Function | Behavior |
|---|---|
| `semanticForLed(globalIndex)` | Record for a global physical index (0..305); `nullptr` for status LEDs or out of range |
| `semanticForPhysicalLed(channel, index)` | Same, by channel 1..8 and 0-based channel index |
| `stationById(id)` | `Station` (id, name, GTFS parent) or `nullptr` |
| `isStationLed(g)` / `isSegmentLed(g)` | Role tests; false for status/unknown |
| `routeForLed(g)` | `Route`, `Route::None` when not a map LED |
| `routeName(route)` / `stationName(id)` | Display text (`""` / `nullptr` when invalid) |
| `selfCheck()` | Cross-checks all 306 global indexes against `led::map`; not called at boot |

Station records carry `stationId`; segment records carry `endpointA`/`endpointB`
and never a station identity of their own.

## Physical to semantic identity

There is no second numbering system. `semanticForLed` converts the global index
with the existing `led::map::toPhysicalAddress` (the project's authoritative
channel/index order) and then looks up that channel/index in the generated table.
`selfCheck()` verifies designator, channel, channel index and global order for
every map LED against `led::map::physicalLed`, and that channel 8 indexes 0-3
(status LEDs) have no semantic record. No existing mapping code was changed.

## Compile-time checks

`static_assert`s in `semantic_api.cpp`: 302 map LEDs, 188 registry stations,
station + segment == map LEDs, generated physical count == `led::map::TOTAL_LED_COUNT`
(306), exactly 4 non-map LEDs, and `Route::RouteZ == kRouteCount` (18). The
generator and `tools/validate_semantic_tables.py` still enforce content
correctness (the project has no host test framework, so none was added).

## Build results (no upload)

| Env | RAM | Flash |
|---|---|---|
| brooklyn-s3 (before, no API) | 20,456 B | 285,501 B |
| brooklyn-s3 (after) | 20,456 B | 285,709 B |
| xiao-s3 (after) | 25,972 B | 264,745 B |

Both builds succeed. Nothing calls the API yet, so the linker discards the
generated tables: no `brooklyn::semantic` symbols are in the linked ELF.
Measured with a temporary (removed) static initializer that referenced
`selfCheck()`: flash rose to 295,609 B (+10,108 B vs. baseline), RAM unchanged
at 20,456 B. `kMapLeds` (4,228 B) and `kStations` (2,256 B) are then retained
in flash-mapped read-only data.

## Next phase

Use the semantic records to resolve train positions to LEDs, still behind the
output-disarmed policy until explicitly enabled.
