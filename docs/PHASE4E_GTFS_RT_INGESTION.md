# Phase 4E - GTFS-RT ingestion and software-only live train-state validation

Status: software-only. No LED output, no GPIO/`LED_ENABLE` change, no upload.
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` is unchanged. The ESP32 is not required.

## Pipeline

```
transport (host Python fetch)      tools/phase4e_fetch_feeds.py
  -> protobuf decode               include/gtfs_rt_decoder.h, src/gtfs_rt_decoder.cpp
  -> train-state extraction        include/gtfs_rt_state.h,   src/gtfs_rt_state.cpp
  -> Phase 4D GtfsTrainState / mapGtfsTrain
  -> Phase 4C resolver -> semantic station/segment + LED designator/channel/index
```

Transport is separate from interpretation. The decoder and state builder are
firmware-capable: a hand-written protobuf wire parser with no heap, no STL, no
network and no protobuf runtime.

## Firmware-capability and the previous-stop limitation

- Protobuf decoding and train-state extraction are firmware-capable.
- The previous stop of an in-transit train is NOT in the realtime feed (TripUpdates
  list only future stops). It is derived from a host-side static-GTFS pattern
  index (`tools/host_tests/pattern_index.h`, built by
  `tools/phase4e_build_pattern_index.py`).
- Therefore complete between-station state extraction is NOT yet self-contained
  on the ESP32. Station (`STOPPED_AT`) positions need no previous stop and do not
  depend on the index.
- Solving this (for example compact predecessor tables) is deferred to the next
  phase/design discussion. The full static GTFS must not be embedded in firmware.

## Official feeds

Base: `https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs` + suffix.
There are 8 feeds. No API key was required (HTTP 200, protobuf `FeedMessage`).
The mta.info documentation pages were unreachable, so the route groups below
were verified empirically from live data, not from documentation.

| Suffix | Routes observed |
|---|---|
| (none) | 1-7, GS |
| `-ace` | A, C, E, H |
| `-bdfm` | B, D, F, M, FS |
| `-g` | G |
| `-jz` | J, Z |
| `-nqrw` | N, Q, R, W |
| `-l` | L |
| `-si` | SI, SS |

Static schedule: `https://rrgtfsfeeds.s3.amazonaws.com/gtfs_subway.zip`.

Shuttles: `H` (ACE feed, Rockaway Park Shuttle) and `FS` (BDFM feed, Franklin Av
Shuttle) map to semantic `S` through the Phase 4D aliases. `GS` (42 St shuttle),
W, E, 1, 6, 7, SI and SS are not on the map and give UnsupportedRoute.

## GTFS-RT fields used

- FeedHeader.timestamp(3); FeedEntity id(1), is_deleted(2), trip_update(3), vehicle(4)
- TripUpdate: trip(1), stop_time_update(2), timestamp(4); StopTimeUpdate arrival(2)/departure(3)/stop_id(4)
- TripDescriptor: trip_id(1), schedule_relationship(4), route_id(5), NYCT extension 1001 (train_id, direction)
- VehiclePosition: trip(1), current_stop_sequence(3), current_status(4), timestamp(5), stop_id(7), vehicle.id(8.1)

TripUpdate and VehiclePosition entities are merged by `trip_id`. Direction comes
from the platform suffix (N/S), then the NYCT extension, then the `..N`/`..S`
trip_id marker, else `?`.

## Train identity

Keyed by `trip_id` (with vehicle/train id kept when present). Route alone is
never an identity. Duplicate trains are never discarded; a repeated trip_id of
the same entity kind overwrites (merge), while distinct trips are all kept.

## Conversion rules (in order)

1. Unsupported route -> UnsupportedRoute (even with no vehicle).
2. Canceled -> Skipped. No vehicle -> Skipped(NoVehicle). No current_status -> Skipped(NotStarted).
3. No stop id -> Invalid(MissingStop).
3a. Route H at H19/H19N/H19S -> OperationalStop (no LED, not UnknownStop).
4. Vehicle timestamp older than `maxVehicleAgeSec` (default 300 s, 0 disables) -> Skipped(Stale).
5. `STOPPED_AT` -> `StoppedAt` at the stop.
6. `INCOMING_AT` / `IN_TRANSIT_TO` -> segment previous stop -> vehicle stop with
   `progress = kProgressUnknown`. Missing previous stop -> Skipped(NoPreviousStop).
7. Phase 4D then yields ResolvedStation, ResolvedSegment (single-pixel Match),
   InsufficientPosition (multi-pixel segment), Unrepresented, UnknownStop or Invalid.

## Progress limitation

TripUpdates carry only future stop predictions and VehiclePosition gives no
geographic fraction. Progress is always unknown; nothing is estimated from
elapsed time. Multi-pixel segments (B/D) return InsufficientPosition rather than
a fabricated slot. No experimental estimate is implemented.

The host previous-stop provider uses an exact static `shape_id` match with the
realtime trip_id suffix, else a unique route+direction predecessor fallback,
else none.

## Rockaway Park Shuttle (route H) stop coverage

Live H trips use these stop IDs. Static GTFS (`stops.txt`) parent data was
checked for each; the Phase 4 supplemental map (`data/gtfs_stop_map.csv`) already
contained every mapped one, so no alias was added and Phase 3B is untouched.

| GTFS parent | Static name | Registry station | Class | Result for H |
|---|---|---|---|---|
| H04 | Broad Channel | 154 | REPRESENTED_ON_MAP | S station LED |
| H12 | Beach 90 St | 155 | REPRESENTED_ON_MAP | S station LED |
| H13 | Beach 98 St | 156 | REPRESENTED_ON_MAP | S station LED |
| H14 | Beach 105 St | 157 | REPRESENTED_ON_MAP | S station LED |
| H15 | Rockaway Park-Beach 116 St | 158 | REPRESENTED_ON_MAP | S station LED |
| H01, H02, H03 | Aqueduct Racetrack, Aqueduct-N Conduit Av, Howard Beach-JFK Airport | 92, 93, 62 | stations mapped with A-only LEDs | Unrepresented (no S LED) |
| H19 | not in static GTFS | none | REALTIME_ONLY_OPERATIONAL_STOP | OperationalStop (no LED) |

### H19: known realtime-only operational stop (deliberately NOT mapped)

`H19N`/`H19S` appear in the realtime H feed but are absent from the official static
`stops.txt` and `stop_times.txt`. Evidence: static H trips begin/end at H04 Broad
Channel; realtime sequences run H19 -> H04 -> H12 -> ... -> H15 (southbound) and the
reverse; most observed H19 reports are layover/stale train positions.

Decision (user-directed): H19 is NOT aliased to station 154 / Broad Channel, no
semantic station or GTFS parent is created for it, and H04 remains the authoritative
Broad Channel GTFS identity. Instead `isRealtimeOnlyOperationalStop(route, stop)`
recognises exactly route `H` with stop `H19`, `H19N` or `H19S`. Such a train becomes
category `OperationalStop`: it never reaches the adapter, resolves no LED, is excluded
from `UnknownStop` statistics and is counted separately. The check applies after the
missing-stop check and before the stale check, so old H19 reports are still
`OperationalStop`, not `Stale`. It is route-specific: route A reporting `H19N` is
still UnknownStop. (Trains with no `current_status` still stop at `NotStarted`.)

Earlier notes suggested "H stops other than H02 are UnknownStop". That was wrong:
all other mapped H stops resolve. Host tests (`testRockawayShuttleStops`) prove, for
H04/H12/H13/H14/H15 in parent, N and S forms:
`H + stop -> Route S -> that station's S LED`, that H04/H04N/H04S still resolve to
Broad Channel (station 154), and that H19/H19N/H19S give OperationalStop with no LED.

### Secondary GTFS parents added in Phase 4E

A scope check of live UnknownStop rows against the registry found two real gaps,
where the realtime stop is the second GTFS parent (same name, separate platform
complex) of a registry station that already carries that route's LED. They were added
to `data/gtfs_stop_map.csv` (Phase 3B untouched; evidence `name+route`):

| GTFS parent | Static name | Registry station | Route LED |
|---|---|---|---|
| R33 | 4 Av-9 St (N/R platform) | 86 (F23 stays the F/G parent) | R |
| M08 | Myrtle-Wyckoff Avs (M platform) | 70 (L17 stays the L parent) | M |

The supplemental map now has 197 parent stops (195 at the Phase 4D freeze); the
generator and adapter test expectations and `PHASE4D_GTFS_ADAPTER.md` were updated.
`testSecondaryParents` proves M08/M08N/M08S -> station 70 and R33/R33N/R33S -> station 86.
## Diagnostic statuses

Categories: ResolvedStation, ResolvedSegment, Unrepresented, InsufficientPosition,
UnsupportedRoute, UnknownStop, Invalid, Skipped, OperationalStop. Skip reasons: NoVehicle,
NotStarted, Stale, NoPreviousStop, Canceled.

```
python tools/phase4e_fetch_feeds.py             # to local_reference/phase4e/feeds (git-ignored)
python tools/phase4e_build_pattern_index.py --gtfs <static dir or zip>
python tools/phase4e_diag.py [--route A --category ResolvedStation --verbose --summary-only]
python tools/phase4e_scope_report.py --gtfs <static dir or zip>   # UnknownStop inside vs outside map
python tools/run_gtfs_rt_tests.py               # synthetic checks + optional Python oracle cross-check
```

Raw `.pb` files are not committed.

## Live validation - single live snapshot

Snapshot taken 2026-10-14, approximately 08:34 America/New_York (12:34 UTC; feed
header timestamps 12:34:28-12:34:41 UTC), 8 feeds, 0 decode errors. This is one
snapshot and not proof of operational correctness. Figures change on every fetch.

| Item | Count |
|---|---|
| Feeds | 8 |
| Entities decoded | 1,690 (925 trip updates, 764 vehicles, 0 deleted) |
| Malformed entities | 0 |
| Merged trips | 915 |
| Supported-route trips | 642 |
| ResolvedStation | 142 |
| ResolvedSegment | 3 |
| InsufficientPosition | 0 |
| Unrepresented | 13 |
| UnsupportedRoute | 273 |
| UnknownStop | 158 |
| Invalid | 0 |
| Skipped | 317 (NotStarted 245, NoVehicle 60, Stale 8, NoPreviousStop 4) |
| OperationalStop (H19) | 9 |

145 resolved trains (142 station + 3 segment) landed on LEDs; 20 LEDs held more than
one train (at most 3 trains on one LED). Previous-stop source (in-transit trains):
exact 12, fallback 8, none 4.

ResolvedStation by route: A 16, C 7, H 1, B 10, D 8, F 19, M 8, FS 2, G 8, Z 3, J 9,
L 12, N 8, Q 11, R 8, 2 4, 3 5, 5 3.

UnknownStop scope (`phase4e_scope_report.py`, static GTFS plus
`data/gtfs_stop_map.csv`; inside = within 150 m of a mapped parent that carries the train's route):

- Unknown stops inside map scope: 0
- Boundary segments (stop on the map, previous stop off-map): 1 (L at `L08S`, previous
  `L06` is off-map, so no segment LED exists)
- Unknown stops outside map scope: 157
- Static-GTFS mismatch (stop id absent from static data): 0

H route: 17 trains listed: 1 ResolvedStation (H at H04N -> Broad Channel);
0 outside scope; 0 UnknownStop; 9 OperationalStop (all `H19N`, reported separately);
7 Skipped NotStarted (4 `H19S`, 3 `H15N`).

### Segments: partially validated live, multi-pixel not validated

Of 20 trains evaluated in `INCOMING_AT`/`IN_TRANSIT_TO`: 3 ResolvedSegment
(single-pixel Match: two segment LED301/LED302 reports, route 3 and 4), 8 Unrepresented
(adjacent stations with no segment LED) and 9 UnknownStop (off-map stops). Live
InsufficientPosition (multi-pixel B/D segments) was NOT observed in this snapshot, so
multi-pixel behavior is supported only by synthetic/fixture tests. Three resolved
segments are not proof of correct segment behavior in general.

Examples: A STOPPED_AT H04S -> Broad Channel LED269; A at A59S -> 80 St LED260;
4 INCOMING_AT 250S from 239 -> segment LED302; 3 INCOMING_AT 239N from 248 -> segment LED301;
H STOPPED_AT H04N -> Broad Channel (S route).

## Tests and firmware

- Phase 4E host tests: 429 checks, 0 failures (includes the Python oracle
  cross-check: entity/trip-update/vehicle/trip counts match on all 8 feeds).
- Prior Phase 4 checks: semantic tables current; validator 302 map LEDs
  (276 STATION, 26 SEGMENT), 188 registry entities; resolver 958,614 checks,
  0 failures; GTFS table current; adapter 7,383 checks, 0 failures (276 station LEDs
  and 26 segment LEDs tested, 197 parent stops).
- Previous-stop limitation: protobuf decoding is firmware-capable, but previous-stop
  derivation currently depends on the host-side static GTFS pattern index. Complete
  between-station extraction is therefore NOT yet self-contained on the ESP32. This is
  deferred to the next design phase; the full static GTFS must not be embedded.
- Firmware, new code unreferenced (dropped by the linker): brooklyn-s3 RAM
  20,456 B / flash 285,789 B; xiao-s3 RAM 25,972 B / flash 264,825 B. Both are
  40 B of flash above the Phase 4D figures (285,749 / 264,785); the cause is not
  identified. The two added stop-map rows did not change the sizes (the table is
  not referenced by firmware).
- Linked cost, measured earlier with a temporary reference file (since removed):
  about +10 KB flash and +8 to +16 B RAM (brooklyn-s3 295,833 B flash / 20,464 B RAM;
  xiao-s3 274,853 B flash / 25,988 B RAM).
## Before LED rendering

- Firmware-side previous-stop source (compact predecessor data; design discussion).
- Review whether other realtime-only stops exist (H19 is the only one known).
- Rendering policy (shared LEDs, staleness, segment display).
- Wi-Fi/TLS transport, polling and caching.
- Live observation of real between-station positions to validate segment behavior.
- Optional, clearly separated experimental progress estimate.
