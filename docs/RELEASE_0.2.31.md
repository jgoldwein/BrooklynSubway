# Brooklyn Map 0.2.31

- Limit a train's blue station indication to 20 seconds by default, measured
  from the first observed report at that station. Repeated feed reports and
  refreshed vehicle timestamps do not restart the visit timer. After expiry,
  show steady dim amber; do not invent a departure.
- Add administrator setting **Station blue hold**, accepting 1-300 seconds.
  Persist it across reboot and expose `blueHoldSeconds` in `/api/status`.
  Apply edits on the next poll using the existing visit start times.
- Expire blue between feed polls, including during slow downloads. A changed
  logical stop or verified movement starts a new visit. Temporarily missing
  polls retain visit history; records unseen for 30 minutes may be reclaimed.
  History capacity exhaustion stays amber rather than restarting a timer.
- A shared LED can stay blue while another train has an unexpired, recent
  matching stop report. The displayed countdown uses the longest remaining
  valid contribution; train counts and brightness do not increase.
- Add independent **OLED brightness** slider, 0-255. Save it across reboot and
  apply it through the OLED worker within one second without reinitializing the
  display or changing its address. Default 207 preserves the driver's existing
  SSD1306 contrast. Zero means minimum contrast, not display power off. Web
  handlers only publish an atomic request; all I2C stays on the OLED worker.
- Preserve OLED initialization and addresses, beeps, wiring, buttons, live
  output caps, OTA verification, and existing verified movement animations.

This is a display timeout, not a measured dwell time. Genuine long stops also
turn amber. The first poll after startup establishes new visit history.
Diagnostic blue/amber totals describe the latest completed poll snapshot;
the live timer may expire afterward, before the next diagnostic update.

Validation includes timer expiry at exactly 20 seconds between polls, repeated
reports, missing polls, stop changes, editing duration without resetting visits,
settings parsing/persistence/save failures, web authentication/CSRF forms, the
complete host regression suite, OLED saved/runtime contrast changes and bounded
transfer failures, and all four supported firmware build targets.
Physical visual validation of this revision remains pending.
