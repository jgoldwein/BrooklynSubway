# Brooklyn Map 0.2.24

- Probe user-requested OLED address 0x38 first, then fall back to 0x3C from the
  working ESPHome configuration. Log each probe's acknowledgement/error code.
- Fix conflicting I2C pin initialization: the graphics library must not set
  GPIO8/9 to ordinary OUTPUT mode after Wire has configured hardware I2C.
  Construct U8g2 without assigned GPIO pins and reuse the existing Wire bus
  through the bounded transport. Preserve the manufactured SDA/SCL assignments.
- Log short writes and the first I2C transfer error code. Retain isolated OLED
  startup, SSD1306 128x64 profile, 100 kHz bus, title and New York clock.
- No board-file, dependency or train-representation changes.

Validation: eight OLED runtime scenarios including 0x38 success, 0x3C fallback,
absent display, stuck lines and failed transfers; enforce that the graphics
constructor receives no GPIO pins. Clock and web regression tests, production
and bench builds. Physical display/startup confirmation remains necessary.
