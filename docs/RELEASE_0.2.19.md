# Brooklyn Map 0.2.19

- Live physical output now starts automatically after healthy startup checks
  and fresh MTA data. GPIO1 stays LOW during startup and while waiting for data.
  POWER toggles output off/on; holding POWER at boot suppresses automatic start.
- A normal TEST press/release runs All subway lines at 25 ms per colour (about
  35 seconds including transmission overhead). The web dropdown stays at
  250 ms per colour. All 302 map LEDs receive RGB checks; POWER/recovery abort.
  A prior live session resumes after a successful fast test when data is fresh;
  an aborted/failed test stays OFF. Other maintenance/fault stops remain OFF.
- Change live station colours: approaching orange to blue, stopped steady blue,
  departing blue to orange. Approach colour interpolates over the last 120
  predicted seconds; departure colour follows the existing 30-second trail.
- Preserve blink-rate changes, prediction freshness rules, stopped-train
  priority, route selections, POWER toggle and all brightness/current limits.
  Missing/stalled predictions stay orange with the fixed blink.
- Segment LEDs keep route-family colours; status indicators and manual RGB
  diagnostics retain their existing colours. Static rendering remains available
  to host tools. No board, GPIO or dependency changes.
- Set default brightness to 5/255 when no valid setting is saved. Preserve all
  existing saved brightness settings. Gradients are calculated before scaling;
  visible colour steps are possible at very low brightness.

Validation: automatic startup, POWER suppression/toggle, TEST press handling,
normal/fast complete RGB walks, colour endpoints, monotonic transitions, unknown/stalled timing,
stopped priority, low-brightness output, saved-setting migration, regression
suite and firmware builds. Physical colour appearance remains an operator check.
