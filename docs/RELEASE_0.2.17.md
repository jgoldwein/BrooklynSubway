# Brooklyn Map 0.2.17

- POWER now toggles the live display OFF and ON after the first explicit web
  enable of that boot. The output task debounces presses; a hold toggles only
  once and a button held at boot cannot enable LEDs. Resume requires healthy,
  fresh data. Existing brightness/current limits and black-first arming remain.
- Reboots, diagnostics, Wi-Fi recovery, firmware installation, explicit web
  disable and faults end the approved session; POWER cannot bypass those stops.
- Add live frame-transmit count, map-data change count, last live-frame age,
  blink-eligible LED count and POWER-pause state to status/JSON and serial logs.
  Map-data changes exclude blink phases and identical repeated snapshots.

The reported board status was healthy: eight feeds, Fresh, 134 trains on 115
LEDs, four approaching. Most lights are expected to be steady, and stopped
occupancy overrides an approaching blink. This snapshot alone does not prove
the map was frozen. New counters help distinguish feed changes from physical
frame transmissions. Remote status access timed out; physical checks remain
with the operator.

Validation: all host runners, POWER toggle/bounce/hold and safety tests, and
versioned network builds. No new dependencies or board/pin changes. GPIO40
remains the active-low POWER input; GPIO1 is the existing output enable.
