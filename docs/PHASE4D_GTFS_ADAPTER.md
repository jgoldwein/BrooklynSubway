# Phase 4D — GTFS-to-TrainMapPosition adapter

Software-only. The adapter interprets normalized GTFS transit state and
produces a `TrainMapPosition` plus the Phase 4C `ResolveResult`. It never
touches GPIO, LED buffers, setters, or `show()`; physical LED output stays
disarmed (`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`). No network or protobuf code
exists yet.

## Architecture

```
GTFS-RT / protobuf / HTTP   (future, not in this phase)
        |  fills
        v
GtfsTrainState              include/gtfs_adapter.h
        |  mapGtfsTrain()
        v
TrainMapPosition            Phase 4C
        |  resolveTrainPosition()
        v
physical LED identity       (ResolveResult: channel / index / global order)
```

| File | Role |
| --- | --- |
| `include/gtfs_adapter.h`, `src/gtfs_adapter.cpp` | route/stop normalization, `mapGtfsTrain` |
| `data/gtfs_stop_map.csv` | Phase 4 supplemental source: GTFS parent stop -> registry station |
| `data/gtfs_stop_review.csv` | manual-review queue (header only; currently empty) |
| `tools/generate_gtfs_stop_table.py` | validates the CSV, emits `include/generated/gtfs_stop_table.h` and `src/generated/gtfs_stop_table.cpp` |
| `tools/phase4d/build_gtfs_stop_candidates.py` | one-off research tool that proposed the CSV from the MTA static feed |
| `tools/host_tests/gtfs_adapter_test.cpp`, `tools/run_gtfs_adapter_tests.py` | host tests (no hardware, no Internet) |

`segmentPixelCount(route, a, b)` was added (additive) to the Phase 4C resolver.

## Supported routes

`2 3 4 5 A B C D F G J L M N Q R S Z` (18 semantic routes). GTFS shuttle
route IDs `S`, `FS` and `H` all map to `Route::S`. Everything else (`GS`, `W`,
`6`, `7`, `5X`, `SI`, empty, null, wrong case) is `UnsupportedRoute`; it is
never reinterpreted as another route. Matching is case-sensitive.

## Stop normalization

Verified against the MTA static feed: every parent stop ID is exactly 3
characters not ending in `N`/`S`, and all 992 platform stops are exactly
`parent + 'N'|'S'`. The rule is therefore:

1. 3 characters, `[0-9A-Z]`: exact lookup in `kGtfsStops`.
2. 4 characters ending in `N` or `S`: the first 3 must be a known parent.
3. Anything else is malformed. Nothing is stripped speculatively.

The N/S suffix is reported (`PlatformDirection`) but unused: travel direction
comes from the order of previous and current stop.

## Station identity and the supplemental table

The frozen Phase 3B registry carries a GTFS parent ID for only 43 of 188
entities and says not to infer missing IDs. The registry is untouched; the
missing IDs live in `data/gtfs_stop_map.csv`, derived from the official MTA
`stops.txt` / `stop_times.txt` / `trips.txt`:

- all 43 registry IDs are reproduced and verified (name and served route);
- all 188 stations map, via 195 parent stops (`evidence` column per row);
- matching uses names **plus** served routes intersecting the entity's map
  routes, never names alone; multi-parent matches are accepted only for
  registry-noted complexes or when route subsets disambiguate.

Mapping is many parents -> one station, never one parent -> many. Programmatic
checks (generator and host tests): all 194 stop IDs are unique, every registry
ID maps to exactly one entity (the one that carries it), and every one of the
188 stations is covered. Transfer complexes with several parents: Atlantic
Av-Barclays Ctr (235, D24, R31), Broadway Junction (A51, J27, L22), Jay
St-MetroTech (A41, R29), Borough Hall (232, 423), Delancey/Essex (F15, M18).
Route-specific LEDs at such a station are chosen by the resolver from
`(route, station)`, not by the stop ID.

Intentionally excluded: 7 Av (Brighton) maps to D25 only (not D14); 86 St
(Sea Beach) maps to N10 only (not Q04); Canal St (endpoint-only) maps to Q01.

**Cross-system alias: station 170.** The semantic identity is unchanged
(Phase 3B: 170 = G / Lorimer St, LED96). The official GTFS G-route parent is
`G29` "Metropolitan Av". The frozen registry keeps its blank GTFS ID; the alias
exists only in `gtfs_stop_map.csv` (`registry_name` = Lorimer St,
`gtfs_stop_name` = Metropolitan Av). Human-confirmed semantic name is Lorimer St
on the physical map; official GTFS G-route parent is G29 Metropolitan Av at the
Lorimer St–Metropolitan Av transfer complex. This is not a Phase 3B correction;
`stationName(170)` still returns "Lorimer St". `G29`, `G29N` and `G29S` resolve
to LED96 for route G, and every other route is `Unrepresented` there.

## Input

```
GtfsTrainState { routeId, status, stopId, previousStopId, progress }
status: Unknown | IncomingAt | StoppedAt | InTransitTo
progress: 0..1000 permille from previousStopId toward stopId, or kProgressUnknown
```

## Position rules

- `StoppedAt` -> `stationPosition(route, station(stopId))`.
- `IncomingAt` / `InTransitTo` -> segment `previousStopId -> stopId` (A -> B);
  `previousStopId` is required.
- Slot semantics follow Phase 4C (direction-aware): slot counts from the
  previous stop toward the next, so the pixel order flips automatically for the
  reverse direction (B Grand St -> DeKalb: LED285, LED309; DeKalb -> Grand St:
  LED309, LED285; D Grand St <-> Atlantic: LED286/LED308 likewise). No LED
  designators appear in adapter code.

## Multi-pixel progress policy

For an N-pixel segment: `slot = min(progress * N / 1000, N - 1)`, integer only.
Single-pixel segments always use slot 0. If progress is unknown and N > 1 the
result is `InsufficientPosition` (resolver Ambiguous); the adapter never picks
a pixel arbitrarily. Boundaries tested for a 2-pixel segment: 0 and 499 -> slot
0; 500, 999 and 1000 -> slot 1. Real GTFS-RT often provides no fractional
progress, so it is optional and must not be fabricated by the transport layer.
`progress > 1000` (not the sentinel) is `Invalid`.

## Status meanings

| Status | Meaning |
| --- | --- |
| `Match` | one physical map LED (station or segment pixel) |
| `Unrepresented` | valid position, but the map has no LED for it (no explicit segment, or no LED for that route at that station) |
| `InsufficientPosition` | multi-pixel segment and no progress to choose a pixel |
| `UnsupportedRoute` | route_id is not one of the 18 |
| `UnknownStop` | well-formed stop ID not in the table |
| `Invalid` | missing/malformed/contradictory input: null/empty route or stop, status Unknown, missing previous stop, malformed stop, both stops in the same station, progress > 1000 |

A wrong route for a station returns `Unrepresented`: the adapter cannot tell a
feed glitch from a route that simply has no LED there.

## Why no LED fallback

An unrepresented position is not snapped to the nearest station, another route,
a nearby transfer LED, or an invented segment. Doing so would show trains where
they are not. How to render such trains is a later rendering-policy decision;
the result carries enough detail (`position`, `resolved`) for that layer.

## Tests

`python tools/run_gtfs_adapter_tests.py` — 7,309 checks, 0 failures: route
parsing, all 195 stops plus N/S forms, every station LED via every GTFS parent
of its station, all 276 station LEDs and all 26 segment LEDs forward/reverse, B and D bridge progress
boundaries, 104 unrepresented adjacent pairs, and invalid combinations.
