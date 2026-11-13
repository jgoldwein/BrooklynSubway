# Brooklyn Map 0.2.23

- Use SSD1306 128x64 OLED initialization at fixed I2C address 0x3C, matching
  the user's working ESPHome configuration for the same display. Replace the
  SSD1309 profile inferred from the product description.
- Preserve Wire controller 0, SDA GPIO8, SCL GPIO9, 100 kHz bus, title and
  one-second New York clock. Preserve the independent OLED worker, startup
  isolation, 50 ms timeout and latched transfer-failure cleanup from 0.2.22.
- No dependency, board-file, pin assignment or train-rendering changes.

Validation: OLED runtime success/failure scenarios and clock tests, web status
regression, production and bench builds. The driver/profile and address are
verified against the supplied working configuration; physical OLED appearance
and board startup still need operator confirmation after installation.
