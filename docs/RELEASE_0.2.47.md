# Brooklyn Map 0.2.47

- Add live counts of approaching, stopped, leaving and uncertain map lights.
  Counts follow the meaning of each indication and the configured palette,
  after brightness/current limiting and successful transmission; status LEDs
  and hardware-test animations are excluded.
- Record each continuous stopped-light ending: line, station, LED designator,
  start/end uptime, duration, approximate end Unix time, next indication and
  why it ended. Reasons distinguish the configured time limit, original-source
  freshness limit, uncertain/approaching/leaving indication, hidden line,
  zero brightness/color, output shutdown/failure and hardware-test pause.
- Add a live panel to Train monitor with ongoing stops and recent endings.
  The last 64 endings are retained in RAM until restart. The UI shows the most
  recent 12, plus up to 12 ongoing stopped lights and the full ongoing count.
- Add read-only /api/indications. Use ?after=SEQUENCE to collect retained events
  in pages of 12; historyGap/more/nextAfter make overwrites and pagination explicit.
  /api/status and /api/monitor also include the four small live counts.
- Add tools/monitor_indications.py for bounded read-only collection. It follows
  pagination, detects restarts, reports overwritten history and writes JSONL.

This release observes existing display behavior. The 20-second stopped limit,
source qualification, colors, brightness, polling and hardware operation are
unchanged. A light ending is not proof of departure; several trains can share a
light. These are successfully transmitted commands, not an optical sensor.
A future change could extend red while fresh consistent stop evidence remains;
that policy is not changed here.

Validation: all 30 host suites pass (diagnostic test-double include corrected and
that suite rechecked), all four firmware environments build, and browser checks
pass at 320/390/768/1100 pixels. Tests cover a 280 ms indication, a 20 s expiry,
source expiry, same RGB assigned to different states, hidden routes, failed writes,
shutdown/test pauses, history overwrite/pagination, uptime rollover and collector
restart/gap handling. Renderer trace preserves RGB bytes across 120 configurations.
Full history/active JSON pages fit the existing 6144-byte buffer for every map LED.
Production static RAM: 196596 bytes, +3128 bytes over 0.2.46. No new dependency,
flash-history writes, pin changes or PCB edits.

After updating, no verbal description is needed: the event collector can measure
short indications directly. An optional steady one-minute video showing the
whole map and OLED clock helps compare commands with the physical lights.
