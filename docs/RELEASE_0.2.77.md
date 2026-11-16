# Brooklyn Subway 0.2.77

OLED upgrade progress for manual signed web uploads, manually approved GitHub
updates and scheduled automatic updates, using one shared OTA session path.

- Shows target version, download percentage, verification, restart approval,
  restarting and post-reboot startup checks.
- Keeps Do not unplug power visible during active stages. The warning is also
  explicit on web upload, download, staged and restart pages.
- Shows Update complete only after an actual pending OTA boot passes the
  existing 15-second SDK health gate, then returns to the normal display after
  eight seconds. Failed transfers show Update failed for twelve seconds.
- Temporarily wakes an OLED that was off; retains the requested power state,
  brightness and quiet-hours cap. The overlay takes priority over screen
  browsing and brightness feedback. All I2C remains owned by the OLED worker.

No new storage keys, tasks, libraries, GPIO assignments or signing-key changes.
USB bootloader flashing cannot show an application-managed OLED progress page.
A private 0.2.76 bootstrap installs the overlay on COM6 before its automatic
0.2.77 rehearsal, so the full download-to-completion display can be tested.

Validation: all 34 host suites pass, including 62 OLED runtime scenarios for
priority, text bounds, warnings, power restore, completion gating and rollover.
Four 0.2.76 firmware targets build; 0.2.77 network packages use the same source.
Update controls remain within their 4096-byte buffer. Physical visibility and
actual automatic-update observations are recorded separately after the test.
