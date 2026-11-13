# Brooklyn Map 0.2.26

- Add one read-only, expandable "Why blue?" report on the normal status page.
  It refreshes with the existing live-value updates. No new controls.
- Show per-route steady-blue station candidates: LEDs, stopped trains, movement
  age over 90 seconds, different next predicted stop, future same-stop arrival,
  missing predictions, shared LEDs, and station changes while still STOPPED_AT.
- Include up to eight station examples with PCB LED designator, sharing count,
  movement age, next-stop ID and relevant flags. Counts refer to the latest poll
  and visible routes, so a held/paused display can differ. Segment colors are
  outside this station report. Predictions are evidence, not confirmed dwell.
- Compare at most 256 prior stopped trips across consecutive finished polls up
  to 120 seconds apart. Show comparison coverage and overflow explicitly.
  Deduplicate trips, exclude conflicts and hidden routes from the report.
- Keep rendering, colors, feeds, GPIO pins, OLED and power behavior unchanged.
  Diagnostic state is bounded and heap-free; pipeline storage grows by about
  9.6 KiB, plus read-only snapshot/JSON storage. No new dependencies.

Validation: host regression suite, duplicate/conflict behavior, diagnostic counts,
station-change comparison, reset/idempotence, hidden routes, bounded HTML/JSON,
and production/bench firmware builds. Physical train interpretation remains
under investigation; this release collects evidence rather than changing it.
