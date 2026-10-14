# Phase 4F — Firmware-self-contained previous-stop resolution

Status: implemented, software-only, **uncommitted**. No LED output, GPIO,
`LED_ENABLE`, hardware or upload changes. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`.

## Problem

Phase 4E derived the "previous stop" of an in-transit train (needed to pick a
SEGMENT LED) from a host-side static-GTFS pattern index. That is not available
on the ESP32. Phase 4F removes that dependency without embedding static GTFS.

## Audit: does GTFS-RT alone supply the previous stop?

`tools/phase4f_audit.py`, 3 snapshots, 2026-10-14 ≈08:40 America/New_York:

- 73 supported-route in-transit trains.
- **0 realtime-exact, 0 conflicting, 73 realtime-insufficient.**
- The VehiclePosition stop always equals the first `stop_time_update`; past
  stops are never sent. A fallback is therefore required.

## Design

A small generated, heap-free, const table maps

    (GTFS route, direction N/S, destination parent stop) -> predecessor parent stops

- Source: `data/gtfs_topology.csv`, built by
  `tools/generate_gtfs_topology_table.py --gtfs <dir|zip>` from the official
  static feed (feed_version `20260826-X-long-term-supplement-trip-ids`; sha256
  of trips/stop_times/stops recorded in the CSV header).
- Generated C++: `include/generated/gtfs_topology_table.h`,
  `src/generated/gtfs_topology_table.cpp` (deterministic, no timestamps).
  `--check` is offline and verifies CSV vs generated code.
- Destinations are limited to the 197 on-map parent stops; predecessors are
  stored verbatim (including off-map stops). Route list: 2 3 4 5 A B C D F FS G H J L M N Q R S Z.
- Entry: 6 B `{uint16 dest, uint8 routeDir, uint8 flags(origin|count), uint16 predOff}`
  plus a uint16 predecessor pool (base-36 packed 3-char ids).
- Size: 672 entries, 691 predecessor words, ≈5.4 KB table; 169 distinct
  supported stop patterns. 40 keys are ambiguous (2–3 predecessors),
  22 are origin-only.

### API (`include/previous_stop.h`)

`previousStopForRealtimeState(route, stop, direction, optional realtime earlier stop)`
returns Match / NoPreviousStop / Ambiguous / UnknownStop / UnsupportedRoute / Invalid.

- A well-formed realtime earlier stop wins (source Realtime) and flags
  `disagreesWithStatic` if the table excludes it. This path has **never been
  observed live**; it is covered by synthetic tests only.
- Unknown key (including off-map destinations and H19) -> UnknownStop.
- Origin-only key -> NoPreviousStop.
- Ambiguous returns sorted candidates (≤4).

### State-builder integration

- Ambiguous: each candidate is run through the Phase 4D adapter. If all give
  the same status (and same LED for Match) the result is accepted as
  `StaticEquivalent`; otherwise the train is `Skipped(PreviousStopAmbiguous)`.
  No guess is made.
- UnknownDestination -> `UnknownStop`; other failures -> `Skipped(NoPreviousStop)`.
- StaticEquivalent means: the exact predecessor remains ambiguous, but every valid predecessor candidate produces the same map result (same adapter status and, for a Match, the same resolved LED global index, which fixes channel, channel index and semantic result). It does NOT mean the exact physical predecessor was determined.
- Progress remains unknown; multi-pixel segments still yield InsufficientPosition.

## Validation

Static (`tools/run_previous_stop_tests.py`): **8,098 checks, 0 failures.**
201 shape-patterns, 2,533 on-map transitions: 2,263 exact, 270 ambiguous (all
contain the true predecessor), 0 unresolved, 0 predecessors lacking static
evidence; 3,352 off-map stops -> UnknownStop. Builder sweep over all 672
entries: 35 unique ResolvedSegment, 29 ambiguous-but-equivalent, 11
ambiguous-and-skipped, 21 UnknownStop.

Live (`phase4e_diag.py --compare`, host pattern index vs firmware table),
single snapshots on 2026-10-14. The ≈08:5x America/New_York times are APPROXIMATE, inferred from the earlier known observation time (≈08:40); they were NOT read from a feed header;
four snapshots, **0 conflicts in all**:

| snapshot | compared | identical | off-map dest | both unresolved | host/fw ResolvedSegment |
|---|---|---|---|---|---|
| 1 | 30 | 11 | 16 | 3 | 1 / 1 |
| 2 | 21 | 10 | 9 | 2 | 0 / 0 |
| 3 | 22 | 11 | 10 | 1 | 0 / 0 |
| 4 | 24 | 13 | 11 | 0 | 1 / 1 |

In every snapshot: firmware-unresolved-but-host-resolved = 0, firmware-only = 0,
realtime-exact previous stops = 0. Ambiguous live cases did not occur, so the
live ambiguity path is exercised by tests only. Live data confirm only
single-pixel segments; multi-pixel B/D behaviour remains synthetic-test-only.

## Memory

Phase 4F linked cost (forced-link measurement, reference removed from tree): flash +6,692 B on both targets
(brooklyn-s3 285,789 -> 292,481 B; xiao-s3 264,825 -> 271,517 B); RAM +0
(20,456 / 25,972 B). Table is const (flash). The default firmware does not link Phase 4F, so final clean builds match the Phase 4E baseline.

## Limitations / rejected options

- Table is a build-time snapshot of static GTFS; regenerate when MTA publishes a
  new feed (hash in the CSV header).
- Ambiguity is irreducible from a single snapshot; rejected disambiguators:
  upcoming-stop suffix, `current_stop_sequence` (trip-relative, shapes unseen in
  static), trip_id shape suffix.
- Future option: a stateful tracker remembering each trip's last STOPPED_AT stop.

## Final clean builds (default firmware, Phase 4F not force-linked)

- brooklyn-s3: RAM 20,456 B, flash 285,901 B (+112 B vs Phase 4E 285,789 B).
- xiao-s3: RAM 25,972 B, flash 264,937 B (+112 B vs 264,825 B).

The clean default build was 112 bytes larger than the Phase 4E checkpoint on both targets. The cause of this small difference was not investigated.
