# Brooklyn Map 0.2.25

- Add a persisted OLED I2C address setting under OLED on the administrator page.
  Default 0x38 matches the display the user confirmed working. Accept explicit
  7-bit hex addresses 0x08-0x77; reject malformed/reserved/out-of-range input.
- Require administrator authorization and CSRF protection to save changes.
  Preserve the previous value if storage fails. Expose the saved address in
  read-only status/JSON. Reboot Device to apply a changed address.
- Pass the saved address to the OLED task at startup. Use only that address;
  remove automatic fallback. Missing/corrupt NVS entries use default 0x38.
- Expand the status-page buffer by 4 KiB to retain space for the new form
  and escaped network/device text.
- Preserve GPIO8 SDA / GPIO9 SCL, 100 kHz bus, SSD1306 initialization, title,
  New York clock, independent worker and bounded failure handling.

Validation: host regression suite, default/migration/persistence/save-failure
checks, malformed/reserved hex rejection, authenticated form/endpoint policy,
configured OLED addresses and invalid-address rejection; production and bench
builds. Changing a setting requires reboot and physical verification.
