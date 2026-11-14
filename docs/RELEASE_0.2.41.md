# Brooklyn Map 0.2.41

- Title the web pages Brooklyn Subway and add the subtitle
  Specially Designed for Avi Yao directly underneath.
- Apply the same heading to sign-in, Wi-Fi setup, firmware updates,
  diagnostics, restart and notice pages, retaining their action headings.
- Show the device name under Advanced / Device information.

- Add Trains: N between the OLED title and clock, updating from the same
  committed mapped-train count as the web page. Fading location hints do not
  add trains. Show -- while current data is unavailable, and 0 for a valid
  empty frame. Brightness feedback restores this layout afterward.

No wiring, feed-acquisition or authorization changes.

Validation: 504 LAN-page checks, 337 provisioning checks and 24 OLED runtime
scenarios pass. Responsive browser checks pass at 320/390/768/1100 px, and
all four firmware targets build successfully. OLED appearance still needs
confirmation on the physical display after installation.
