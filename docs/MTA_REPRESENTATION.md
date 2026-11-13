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
  replay uses simulated elapsed time. Network polling still blocks the main
  loop, so physical blink timing under polling load needs validation before
  live physical output is enabled.
- Web status reports approaching trains as a subset of resolved trains.
  Serial diagnostics report station, approaching, segment, unrepresented,
  insufficient, unknown-stop and stale counts for each cycle.

Normal physical map output remains disabled. These are logical renderer and
state changes; GPIO, board files, brightness caps and diagnostic tests do not
change. Publishing and on-board observation are separate steps.

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
