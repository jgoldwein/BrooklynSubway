# Live MTA representation

## Current implementation

- Acquisition uses the existing eight public MTA feeds and VehiclePosition
  decoding. Stop IDs, vehicle status and travel direction drive mapping; no
  coordinates, progress estimates or arrival-time interpolation are invented.
- A stopped train occupies its route's station LED steadily.
- A moving train uses its mapped segment when that resolves uniquely.
- If the segment is unrepresented but the next station has an exact route
  match, the train contributes an approaching-station flag instead. That
  station blinks in its route colour: 500 ms on, 500 ms off. The reported
  vehicle status remains moving. This policy was selected by Joel.
- Multi-pixel segments without progress remain unresolved; unknown stops,
  stale vehicles and ambiguous predecessors are not forced onto an LED.
- Occupancy never adds brightness. If a stopped train shares an LED with an
  approaching train, steady display takes priority, independently of record
  order and even when occupancy saturates. Same-trip stopped/approaching
  disagreements are conflicts and are excluded for the cycle.
- The approach bitmap is part of the committed TrainFrame. Partial-cycle
  holds preserve it and hard stale clearing removes it with the occupancy.
  The existing 90-second partial grace and five-minute blanking limit apply.
- The renderer accepts an explicit blink phase. Firmware uses monotonic time;
  replay uses simulated elapsed time. From 0.2.16 the physical live service
  renders a mutex-protected committed snapshot in a separate output task,
  independently of blocking network polling. Actual timing under polling load
  remains an on-board observation step.
- Web status reports approaching trains as a subset of resolved trains.
  Serial diagnostics report station, approaching, segment, unrepresented,
  insufficient, unknown-stop and stale counts for each cycle.

From 0.2.18, the pipeline also reads TripUpdate predictions in a bounded first
pass, then streams VehiclePositions as before. Matching uses the same trip hash
pair, normalized route and exact next-platform stop. Conflicting predictions are
discarded; unknown, canceled, ambiguous and stale position handling is unchanged.
The prediction table is reused per feed and has an overflow counter.

The next station's estimated approach rate is 0.5 Hz beyond 60 seconds, 1 Hz at
31-60 seconds, 2 Hz at 16-30 seconds and about 3 Hz at 0-15 seconds. Missing or
old movement evidence uses the existing fixed 1 Hz blink. Predictions never
declare arrival: STOPPED_AT remains authoritative for a steady station light.
After 90 seconds without detected movement, the rate stops accelerating.

A uniquely identified previous station can carry a departure trail: 2 Hz for
the first 10 seconds, 1 Hz for the next 10, then 0.5 Hz until 30 seconds, when it
clears. Age uses MTA's last-detected-movement timestamp, not measured distance
or a confirmed departure timestamp. Both station cues are estimates selected by
Joel. Stopped occupancy wins, route visibility applies, and overlays never add
to resolved train counts. A mapped segment may remain visible too.

Station timing occupies four bytes per physical index. TrainFrame grows from
372 to 1600 bytes and TrainPipeline to 43,640 bytes; production static RAM grows
by about 32 KiB. No per-feed heap or second raw feed buffer is added. Live
rendering runs at up to 25 frames/second in its existing output task to support
variable pulse rates, retaining brightness/current caps and POWER behavior.

The authenticated MTA update interval control persists a 15-120 second wait
after cycle completion, default 15 seconds. Download duration adds to the
effective interval; cycles never overlap. Older settings migrate without
changing Wi-Fi, routes, brightness or administrator credentials.

From 0.2.19, physical live output starts automatically after healthy startup and
fresh data; GPIO1 remains LOW while waiting. POWER toggles off/on. The redundant web enable/disable controls
are removed in 0.2.20. See LIVE_DISPLAY.md for limits and shutoff behavior.
GPIO, board files and the existing brightness/current limits do not change.

## Validation

Two fresh cycles of all eight feeds were captured locally under ignored
`captures/mta-representation-review/`. Both cycles downloaded and decoded all
eight feeds successfully. Identical inputs were replayed before and after
the change, through the actual firmware pipeline and RGB renderer. In the
first cycle, resolved trains increased from 128 to 135; seven approaching
trains that lacked a mapped segment gained an exact next-station indication.
No invalid positions, renderer rejections or trip-table overflows occurred.

Tests cover incoming/in-transit states in both directions, exact next-station
identity, unchanged segment handling, blink boundaries, mixed occupancy,
saturation, trip conflicts, stale rejection, hard stale clearing, route
visibility and existing output safety. Physical observation is still pending.

## Queued after MTA representation work

Review the full subway-line diagnostic mapping and ordering so each complete
line is tested continuously from end to end before moving to another line.
Joel reports that some lines appear in separate portions. Do not assume
physical chain order equals geographic station order. Keep the existing full
channel diagnostic available independently.

## Reference

- [GTFS-Realtime reference](https://gtfs.org/documentation/realtime/reference/)
- [MTA subway feed reference](https://www.mta.info/document/134521)

## Verified departures (0.2.20)

Some N/R VehiclePositions retain STOPPED_AT while trips advance. A matching
TripUpdate can now establish departure only when its first remaining stop is
a different platform in the same direction, with a unique topology predecessor
matching the old station. The prediction must be newer than the position, its
arrival in the next 600 seconds, the position at most 90 seconds old, and feed
age at most 60 seconds (future-clock tolerance 30 seconds). When the optional
TripUpdate timestamp is absent, its enclosing feed timestamp supplies freshness.
Conflicting, canceled, stale, opposite-direction and ambiguous cases stay
unchanged. This does not invent an arrival or add trains. Serial diagnostics
report the number of verified input departures. Pipeline scratch storage is
now 47,740 bytes; no new dependency or raw feed buffer is added.
