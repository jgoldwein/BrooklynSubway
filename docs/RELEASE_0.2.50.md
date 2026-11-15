# Brooklyn Map 0.2.50

- Redesign the setup Wi-Fi/captive portal to match the main Brooklyn Subway UI,
  with the Avi Yao dedication, clear map-password / Wi-Fi / open-map steps,
  phone-friendly network cards and Advanced administrator-password reset.
- Scan up to 24 networks without reloading the form or clearing typed passwords.
  Keep manual network-name entry available, including without JavaScript. Show
  password controls remain scoped to their own form. All assets are local.
- Show connection progress and clear success/retry pages, including this board's
  individual LAN address. Prevent scans from interrupting an active connection.
- While setup Wi-Fi is active, show OLED instructions: Wi-Fi setup,
  Join BrooklynMap-Avi, Open in browser:, 192.168.4.1. Restore the title,
  train count and clock when setup ends. Existing POWER and brightness feedback
  still apply. U22 uses the existing SDA GPIO8 / SCL GPIO9 bus and OLED driver.

Validation: all 30 host suites and four firmware targets pass. OLED tests cover
setup instructions fitting within 128x64 and returning to the normal display.
Setup rendering checks cover every result state, escaped network names,
production 6144-byte page bounds, JSON and small-buffer failure. Real browser
checks pass at 320/390/768/1100 pixels, including network selection, password
visibility, background scan field preservation and real form submission.
Production static RAM remains 197812 bytes; no added dependency or pin changes.
0.2.50 was flashed to the verified second board on COM6; esptool verified hashes
and reset it, and serial heartbeats were observed. Physical OLED visibility and
phone captive-portal appearance still need the operator's visual check.
The original COM5 board was not flashed.
