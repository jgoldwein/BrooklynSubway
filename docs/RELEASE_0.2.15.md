# Brooklyn Map 0.2.15

- Add an approaching-station representation for moving trains where there is
  no mapped segment LED but an exact next-station route match exists. The
  station uses its route colour and an explicit 500 ms on / 500 ms off phase.
- Preserve steady stopped-train indication. A stopped train sharing a pixel
  takes priority; additional trains never increase brightness. Same-trip
  stopped/approaching conflicts are excluded for the cycle.
- Keep unknown stops, ambiguous predecessors and multi-pixel segments without
  progress unresolved. Retain stale-vehicle filtering, partial-frame grace
  and five-minute hard stale blanking, including approaching-state cleanup.
- Add approaching-train counts and a representation legend to web status,
  cycle representation counters to serial output, and matching replay metadata.

Physical live output remains disabled. Feed polling blocks the main loop and
can pause blink updates; continuous physical blink timing must be addressed
before enabling the live display. No GPIO, board or dependency changes.
Existing channel and route diagnostic tests remain available. Complete-line
diagnostic ordering remains queued after MTA representation work.

Validation: all 25 host test runners and both network firmware builds pass.
Two fresh eight-feed capture cycles decoded successfully. Replaying the same
first-cycle input increased represented trains from 128 to 135 through seven
exact approaching-station indications, with no renderer or invalid-position
errors. Physical observation remains pending.
