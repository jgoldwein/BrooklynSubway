# Brooklyn Map 0.2.28 — display recovery

- Restore the exact 0.2.26 station-rendering, pipeline and frame code after the
  widespread uncertainty-pulse behavior in 0.2.27 was unacceptable on the board.
  Remove the new corroboration rules, uncertainty bitmap, synchronized orange
  pulses and 30-second blue-support expiry. Restore the smaller prior memory
  layout. This returns the earlier blue-light interpretation limitations.
- Preserve the Why blue? diagnostic report, configured OLED address (default
  0x38), title/clock, auto-refreshing web status, POWER controls and output caps.
- OLED initialization/address code was unchanged by 0.2.27; its blank-screen
  cause is not established. This release does not claim an OLED-specific fix.
  Check OLED startup messages if it remains blank after a complete power cycle.
- No GPIO, board-file, dependency or persisted-setting changes.

Validation: source restored byte-for-byte from 0.2.26 for the affected rendering/
pipeline modules and tests, host regressions and production/bench builds. Board
confirmation is still required. The problematic 0.2.27 animation is withdrawn;
future interpretation changes should be evaluated before enabling broadly.
