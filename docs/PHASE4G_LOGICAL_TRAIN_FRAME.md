# Phase 4G — Logical train-to-LED frame

Status: implemented, software-only, **uncommitted**. Baseline: commit `a63c00e`
(tag `phase-4f-previous-stop`).

**No physical LED output occurs.** There is no RGB, colour, brightness, blink,
fade, animation or temporal-persistence policy yet. The compositor has no
dependency on any LED driver, GPIO, networking, protobuf or static GTFS.
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT` remains `0`.

## Purpose and boundary

    GTFS-RT -> decoder/state builder -> previous-stop -> Phase 4D adapter -> Phase 4C resolver
        -> resolved trains
        -> Phase 4G logical frame (this phase)
        -> FUTURE visual renderer -> FUTURE physical LED driver

The frame answers only: "given the resolved trains, which of the 302 map LEDs
are logically occupied, and by how many trains?"

## Files

- `include/train_frame.h`, `src/train_frame.cpp` — firmware-capable API.
- `tools/host_tests/train_frame_test.cpp`, `tools/run_train_frame_tests.py` — host tests.
- `tools/host_tests/gtfs_rt_diag.cpp` — new `--frame` live diagnostic (host only; keeps trip IDs).
- `tools/phase4e_host.py` — `src/train_frame.cpp` added to the host core sources.

## Schema (`brooklyn::semantic::TrainFrame`)

| field | type | meaning |
|---|---|---|
| `occupancy[306]` | `uint8_t` | trains on each canonical global LED index (led::map order); saturates at 255 |
| `resolvedTrains` | `uint32_t` | accepted trains, including saturated ones |
| `excessTrains` | `uint32_t` | `resolvedTrains - occupiedLeds` |
| `occupiedLeds` | `uint16_t` | LEDs with occupancy >= 1 |
| `collisionLeds` | `uint16_t` | LEDs with occupancy >= 2 |
| `saturatedTrains` | `uint16_t` | accepted trains not counted because the LED was already at 255 |
| `ignoredTrains` | `uint16_t` | results that were not a map Match |
| `rejectedTrains` | `uint16_t` | claimed Matches that failed identity validation |
| `maxOccupancy` | `uint8_t` | highest per-LED count |

`sizeof(TrainFrame) = 328` bytes. Plain POD, no dynamic allocation.
API: `clearFrame`, `addResolvedTrain`, `addGtfsResult`, `composeTrainFrame`,
`frameOccupancy`, `frameOccupied`.

## Canonical LED identity

The index is the existing `led::map` global physical index (0..305); no new
numbering exists. Route identity is not stored in the frame: it is recovered
from the existing semantic record (`routeForLed(index)` / `kMapLeds`), so there
is no second route table.

Before a Match is counted, `addResolvedTrain` verifies: global index < 306;
`led::map::physicalLed` role is Map (status LEDs rejected); the semantic
pointer is non-null and is the canonical `kMapLeds` entry for that index;
`globalOrder`, `channel`, `channelIndex` and designator agree with `led::map`.
A failure is `Rejected` (counted in `rejectedTrains`), never added.
U23 (GPIO14 `USER_LED`) is a discrete LED, not in the addressable chains, and
cannot appear. The four status LEDs can never be occupied.

## Eligibility

Only a final `Match` contributes. `Unrepresented`, `UnknownStop`,
`UnsupportedRoute`, `Invalid`, `InsufficientPosition`, and non-adapted
observations (`PreviousStopAmbiguous`, `OperationalStop`, Stale, NotStarted,
NoVehicle, ...) are ignored. There is no fallback. For an `Observation`, the
caller feeds `o.result` only when `o.adapted`; `addGtfsResult` then ignores
anything whose adapter status is not `Match`.

## Occupancy and collisions

0 = no train; 1 = one train; >1 = several trains represented by the same LED.
Collisions are legitimate (same station, opposite directions on one segment
pixel, two trains on one pixel) and are not errors. The frame keeps the count,
lights nothing and makes no animation decision; collisions are only reported.
Saturation at 255 is deterministic: further trains still count in
`resolvedTrains`/`excessTrains`/`saturatedTrains` but never wrap the byte.

## Duplicate-train contract (option A)

The frame does **not** deduplicate. Upstream Phase 4E already merges
TripUpdate and VehiclePosition entities by `trip_id` per feed, yielding one
record per trip, so the caller supplies that merged set. The live diagnostic
checks this: duplicate trip_ids among the trains offered = 0.

## Order independence

Counts are commutative, so the same set of trains in any order gives a
byte-identical frame. The test shuffles a result set 50 times and compares
frames with `memcmp`.

## Tests (`python tools/run_train_frame_tests.py`)

**5,232 checks, 0 failures.** Covers: empty frame; station and segment trains;
distinct LEDs; 2 and 3+ trains on one station LED; two trains on one segment
LED; opposite-direction trains on all 22 single-pixel segments; saturation;
every non-Match adapter/resolver status; real upstream UnsupportedRoute,
UnknownStop, OperationalStop (H19), Invalid, Stale, NoVehicle and
PreviousStopAmbiguous observations; all four status LEDs rejected; bad global
indexes, null/mismatched semantic records and channel/index disagreement
rejected; frame reset; order independence; all 302 map LEDs occupied
individually and jointly (276 station + 26 segment; exactly the four status
LEDs remain empty); route of each occupied LED equals its semantic route;
semantic tables hashed before/after and unchanged.

## Live snapshot (host only)

**Single live snapshot**, MTA feed header timestamps 2026-10-14
09:03:30–09:03:37 America/New_York (read from the feed headers), 8 official feeds:

- feed entities decoded 1,429; merged trips 762; supported-route trips 563
- ResolvedStation 131, ResolvedSegment 0, Unrepresented 12, InsufficientPosition 0,
  UnsupportedRoute 199, UnknownStop 157, Invalid 0, Skipped 253, OperationalStop 10
- resolved trains supplied to the compositor: 131 (all added; 0 not added, 0 rejected)
- unique occupied LEDs: 108 (station 108, segment 0)
- collision LEDs: 22; excess trains: 23; maximum occupancy: 3 (LED190, DeKalb Av, Q trains)
- status LEDs occupied: **0**; non-map LEDs occupied: 0
- duplicate trip_ids among the offered trains: 0

This snapshot contained no resolved segment positions, so segment occupancy of
live data is covered only by the synthetic/fixture tests here (and the Phase 4E
live observation of single-pixel segments). One snapshot is not proof of
complete operational correctness. Every collision was a station LED holding
trains in opposite directions or consecutive trains of one route; each is
printed by `python tools/phase4e_diag.py --topology --frame --summary-only`
with LED designator, route, station and trip IDs.

## Memory

- `sizeof(TrainFrame)` = 328 B; instantiated static RAM = 328 B.
- Phase 4G code (`train_frame.cpp.o`): 530 B text, 0 data, 0 bss.
- Forced-link measurement (temporary reference with one static frame, removed):
  brooklyn-s3 RAM 20,456 -> 20,784 B (+328), flash 285,989 -> 292,469 B (+6,480);
  xiao-s3 RAM 25,972 -> 26,308 B (+336), flash 265,025 -> 270,693 B (+5,668).
  The flash figure includes the reference harness itself and the semantic
  tables/lookup code (`semantic_api`, `kMapLeds`) that the frame needs and that
  are otherwise not linked; the compositor's own code is 530 B.
- Default (unlinked) firmware: brooklyn-s3 RAM 20,456 B / flash 285,989 B;
  xiao-s3 RAM 25,972 B / flash 265,025 B. Phase 4G is not referenced by the firmware.
  Flash is 88 B above the Phase 4F figures on both targets; that difference was
  not investigated.

## Remaining before LED rendering

Colour/brightness policy per route, collision display policy, temporal
persistence, a renderer and driver, firmware-side feed transport/scheduling.
