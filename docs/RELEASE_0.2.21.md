# Brooklyn Map 0.2.21

- Add the confirmed 2.42-inch SSD1309 128x64 I2C OLED: centered "Brooklyn Subway"
  with the current New York time on the line below (12-hour time with seconds
  and AM/PM). Show "Syncing time..." until the existing SNTP clock is valid.
  Apply US daylight-saving transitions without changing the UTC network clock.
- Probe only OLED addresses 0x3C/0x3D on the manufactured SDA GPIO8 / SCL GPIO9.
  A missing display is logged and skipped. Use an independent 1-second task so
  blocking feed downloads do not freeze the clock. No separate reset is wired.
- Pin U8g2 2.36.18 for SSD1309 rendering, production environments only. Bench
  XIAO builds do not initialize an OLED or infer production pin assignments.
- Refresh status-page MTA counts, cycle outcome, data ages, operational display
  and uptime in place about every five seconds after a response. Preserve login
  fields and unsaved settings. Pause background requests in hidden tabs and show
  a delay notice on failed requests. This does not increase MTA polling frequency.
- Preserve existing train representation and color rules. The remaining excess
  steady-blue interpretation issue is still under investigation; this release
  does not claim to fix it. No board-file or pin assignment changes.

Validation: host regression suite including clock synchronization, DST spring/
fall boundaries, winter/summer, noon/midnight and buffer limits; status-page
rendering; production and bench builds. Physical OLED appearance needs an
operator check after installation.
