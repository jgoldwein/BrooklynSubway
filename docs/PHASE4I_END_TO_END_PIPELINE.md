# Phase 4I - End-to-end firmware train pipeline

Raw GTFS-RT bytes -> logical `TrainFrame` -> `RgbFrame`, entirely in memory.

**There is no networking, no heap use, and no physical LED output.**
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT` stays 0, `LED_ENABLE` (GPIO1) is untouched, and
no LED set/show call exists. The firmware links the real pipeline (`src/main.cpp`
instantiates it and runs a startup self-check on an empty cycle), but no feed source
exists yet, so no trains are ever produced on the device.

## Architecture

```
raw feed buffer(s)  --visitVehicles-->  RtTrain (one at a time, stack)
   --evaluateTrain (static topology previous stop)-->  per-trip table (deduplication)
   --finishCycle-->  TrainFrame  --renderTrainFrame-->  RgbFrame
```

API (`include/train_pipeline.h`, `brooklyn::pipeline::TrainPipeline`):

- `beginCycle(nowUnix, RenderConfig)` - full reset of frame, RGB, trip table, counters.
- `processFeed(data, length)` - once per feed, any order; the caller owns the buffer and
  nothing is copied or retained.
- `finishCycle()` - applies the deduplicated trips to the TrainFrame, renders the RgbFrame and
  freezes counters (idempotent).

## Which realtime records are required

VehiclePosition alone is sufficient; TripUpdate entities are counted
(`tripUpdatesIgnored`) but never decoded in firmware.

| Needed value | Source |
|---|---|
| trip_id, route_id | `VehiclePosition.trip` |
| stop_id, current_status, timestamp | `VehiclePosition` |
| direction | stop-id platform suffix, NYCT extension, or `..N/S` in trip_id |
| previous stop | Phase 4F static topology table in flash |

The only TripUpdate consumer in the state builder is the optional "realtime earlier stop"
path, which the Phase 4F audit never observed (0 of 73 in-transit trains). With no
TripUpdate data it correctly falls back to the topology table.

Expected, documented differences from the merged host pipeline: TripUpdate-only trips
("NoVehicle" skips, roughly 90-150 per snapshot) do not exist in the firmware path, and
CANCELED handling (merged from TripUpdates) is not applied. CANCELED was observed 0 times
in every audited snapshot. Neither changes the TrainFrame.

## Streaming and storage

`visitVehicles` walks the buffer once and hands each VehiclePosition to the pipeline as a
short-lived `RtTrain` (132 B on the stack). Nothing is accumulated per feed. The decoder
needs the **whole raw feed resident in memory**; the largest observed single feed was
**239,678 B** (numbered-lines feed). Phase 4I allocates none of it; a future network phase
must supply that buffer (the ESP32-S3 has 8 MB PSRAM).

Fixed storage: `sizeof(TrainPipeline)` = **13,664 B** =
`TrainFrame` 328 + `RgbFrame` 918 + trip table 12,288 (1,024 x 12 B `TripEntry`) +
`Counters` 120 + state. (Before the identity correction: 5,460 B with a 4,096 B table, so the
table grew by 8,192 B.) Largest scratch structure: the 132 B `RtTrain` (plus the evaluator's
stack locals).

## Trip identity and deduplication contract

Audit of `trip_id` lengths (static GTFS `gtfs_subway.zip` and every recorded realtime feed):

| Source | Count | Length min / max / typical |
|---|---|---|
| Realtime (all recorded feeds, 1,700 distinct ids) | 1,700 | 11 / 17 / 13.5 (all `NNNNNN_R..D...`) |
| Static GTFS `trips.txt` (schedule ids, never sent in realtime) | 20,621 | 31 / 44 / 38.0 |

Realtime ids follow one stable format (6-digit origin time, `_`, route, `..`, direction, suffix).
The decoder buffer is 32 B (`kTripIdMax`); an exact table would need 32 B x 896 = 28,672 B
(about 15 KB even at the observed 17-char maximum), so identity is a 64-bit pair of independent
hashes (`tripHashPrimary` FNV-1a, `tripHashSecondary`) held in a 12 B `TripEntry`.

- Different trip_ids with equal primary but different secondary hash (e.g. `C449599` /
  `C612382`, which collide under FNV-1a) are **distinct trips**, never a duplicate, and are
  reported in `tripIdentityCollision`.
- **Residual limitation.** Identity is the compound fingerprint of two independent 32-bit
  hashes (64 bits total). This is **not** mathematically exact trip identity. Two *different*
  trip_id strings that collide on **both** hashes cannot be distinguished by the table:
  - if their resolved outcomes differ, the conflict handling rejects the trip for the cycle,
    so a wrong train is never rendered and no different LED can be selected;
  - if their outcomes are identical, they would be treated as one duplicate
    (`duplicateIdentical`), undercounting trains/occupancy at that same map result.
  This residual risk is accepted because no compound collision has been observed, primary-hash
  collisions (`C449599` / `C612382`) are demonstrably handled, the compound collision
  probability is extremely small (about 2e-14 per cycle at 896 trips), and exact trip strings
  would materially increase fixed pipeline RAM (about 15-29 KB versus the current 12 KB table).
- A trip's state is its resolved outcome (category and, for resolved trains, the map LED).
  Same trip_id with identical outcome: counted once, extra records are `duplicateIdentical`.
  Same trip_id with differing outcomes: the trip is rejected for the whole cycle
  (`conflictedTrips`; all its records in `duplicateConflict`) and draws nothing (policy B).
  No record order influences the result.
- Contributions are applied to the TrainFrame in `finishCycle()`, and supported-trip category
  counters are filled there. Results and counters are independent of feed order (tested for
  all permutations) while the table is not full. Past 896 distinct trips, which trips are
  dropped depends on arrival order.
- Unsupported-route vehicles bypass the table.

Audit: 0 duplicates among supported routes and across feeds in all audited snapshots. One
snapshot had a within-feed duplicate on route 7 (unsupported, never drawn).

## Capacity and overflow

`kTripTableSlots` = 1,024, `kMaxTrackedTrips` = 896 (observed maximum 581 supported
vehicles across all feeds). Beyond 896 distinct supported trips, further trips count as
`tripCapacityOverflow` and are not drawn; processing continues safely. Counters saturate and
never wrap; occupancy saturates at 255. After `finishCycle()` every vehicle lands in exactly
one bucket, so the category counters + `duplicateIdentical` + `duplicateConflict` +
`tripCapacityOverflow` + `frameRejected` sum to `vehiclesSeen` (tested, including under 2,000
corruption-fuzz iterations).

## Time

The pipeline uses Unix timestamps only (`nowUnix`, feed header/vehicle timestamps). New York
time appears only in these docs for presentation. `nowUnix == 0` falls back to each feed's
header timestamp. Staleness limit: 300 s.

## Previous stop, collisions, colours

Unchanged from Phases 4F-4H: ambiguous previous stops are skipped unless every candidate gives
the same map result (`StaticEquivalent`); H19 is an `OperationalStop`; multi-pixel segments
are `InsufficientPosition` (progress is unknown from GTFS-RT) when the previous stop is known; collisions share one LED colour
regardless of occupancy; the four status LEDs are never occupied or coloured.

## Validation

Host tests (`python tools/run_train_pipeline_tests.py [--replay DIR[@UNIX_NOW]]...`):
4,538 synthetic checks, 0 failures. They cover station, single-pixel segment in both
directions, the two frozen multi-pixel groups, unknown, unrepresented, unsupported, H19,
ambiguous previous stop, stale, malformed entities, empty/truncated/corrupt feeds, fuzzed
corruption, 2/3-train collisions, multi-feed, cycle reset, overflow past 896 trips, the
startup self-check, and the duplicate cases: identical, conflicting, split across two
`processFeed()` calls, three-record permutations, mixed three-feed sets in every order,
and different trip_ids colliding under the old hash.

Multi-pixel segments (identities checked against the canonical map records):
B Grand St..DeKalb = LED285 slot 0 / LED309 slot 1; D = LED286 slot 0 / LED308 slot 1; both
groups share GTFS D22 as one endpoint (B other end R30, D other end R31).
- B, raw feed, both directions (D22N, R30S): `InsufficientPosition`, no LED occupied.
- D, raw feed, both directions (D22N, R31S): with VehiclePosition-only data the static topology
  has two predecessors with different outcomes, so the result is `PreviousStopAmbiguous`
  (nothing drawn); the pipeline does not guess. Raw feed with a known previous stop (D22N from
  R31, R31S from D22) gives `InsufficientPosition`, preserving the Phase 4C behaviour.

Replay: 8 snapshots x 8 feeds were run through the pipeline and compared with the merged
host pipeline (full decode; the snapshots contain no supported duplicates). In every one the
TrainFrame occupancy and RgbFrame bytes were **byte-identical**, and identical again with
the feeds processed in reverse order.

Fresh live snapshot (feed headers 2026-10-14 10:24:26-10:24:37 America/New_York, 8 feeds,
722,695 B, largest feed 204,893 B, 1,490 entities, 689 vehicles, 543 supported):
resolvedStation 138, resolvedSegment 0, unrepresented 13, insufficient 0, unsupported 146,
unknown 140, invalid 0, previousStopAmbiguous 0, operational 11, stale 15, notStarted 226,
skipped 0, duplicates 0, malformed 0; frame: 138 resolved trains, 117 occupied LEDs, 19
collision LEDs, max occupancy 3, 117 non-black pixels, 0 status LEDs lit, 0 renderer
rejections. This snapshot contained no live segment resolution.

## Firmware size

Phase 4H clean baselines -> Phase 4I (pipeline genuinely linked via `main.cpp`):

| Target | RAM | Flash |
|---|---|---|
| brooklyn-s3 | 20,456 -> 34,120 B (+13,664) | 286,029 -> 306,121 B (+20,092) |
| xiao-s3 | 25,972 -> 39,644 B (+13,672) | 265,065 -> 284,329 B (+19,264) |

The flash increase includes the decoder, state builder, topology and GTFS stop tables,
frame/renderer code, and the startup `printf`.

## Remaining dependency

A network transport that fetches the eight feeds into resident buffers and calls
`beginCycle` / `processFeed` / `finishCycle`, followed by (separately approved) LED output.
