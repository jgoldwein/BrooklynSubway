# Brooklyn Map 0.2.20

- Approaching stations stay orange until the final 30 predicted seconds, then
  transition to cyan with the existing accelerating blink. At-station trains
  remain steady blue. Departures transition blue to magenta while blinking more
  slowly and fading out within 30 seconds. Unknown/stalled predictions stay
  orange at fixed blink rate; stopped occupancy wins shared lights.
- Reconcile retained N/R STOPPED_AT positions with verified newer TripUpdates
  only when direction, freshness, future arrival and unique adjacent topology
  agree. Uncertain cases remain unchanged. Add serial departure diagnostics.
- Dimly light the separate U23/GPIO14 indicator at 8/255 PWM duty when complete
  fresh MTA data is working; clear it for stale/partial data, setup and maintenance.
  This indicator remains independent of the map's POWER toggle.
- Remove the redundant web Enable/Disable Live Display control and the already
  completed rail/polarity confirmation prompts from diagnostic tests. POWER
  controls live output. Fresh healthy data permits a physical POWER retry after
  maintenance; it does not automatically restart a stopped session.
- Preserve brightness/current limits, automatic healthy startup, fast TEST and
  slower web tests, Wi-Fi settings, signed updates and rollback. No board-file,
  GPIO assignment or dependency changes. Complete-line geographic test ordering
  remains queued separately.

Validation: 27 host test suites, production/bench network builds, palette and
fade boundaries, stopped priority, departure evidence rejection, POWER restart,
U23 PWM allocation/cleanup and existing OTA/output safety checks. Physical color
appearance and U23 brightness remain operator checks after installation.
