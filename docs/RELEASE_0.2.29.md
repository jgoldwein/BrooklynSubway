# Brooklyn Map 0.2.29

- Restore OLED address fallback removed when the address setting was added.
  Try the saved address first, then distinct 0x38/0x3C/0x3D candidates. Retry the
  bounded probe pass once after 100 ms for late-ready devices. Keep the saved
  preference unchanged and show which address actually acknowledged.
- Add read-only live OLED status to the web page and /api/status: startup
  address, active address, initialization/communication phase and I2C error code.
  Readers use an atomic snapshot and never touch the bus. Frame writes OK means
  communication succeeded; it is not optical confirmation of the image.
- Preserve GPIO8 SDA / GPIO9 SCL, 100 kHz, SSD1306 128x64 profile, independent
  worker, pin-ownership fix, bounded transfer failures and map startup isolation.
- Add the requested one-time feed-ready beep when U23 first lights after boot:
  GPIO21/BUZZER3, 2000 Hz, 100 ms, 50% duty. Respect DIM startup mute. No repeated
  beeps for network recoveries/tests; U23 PWM or buzzer setup failures stay safe.
- Retain recovered 0.2.28 train rendering and diagnostics. Do not reintroduce
  the withdrawn station-uncertainty animation. No board-file or dependency changes.

Validation: OLED preferred/fallback/late-ready detection, missing device, held
bus, init/runtime failure status, active-versus-saved address display/JSON,
feed-ready cue ordering/mute/cleanup, regression suite and all firmware targets.
Physical OLED recovery remains to be verified on the board; the prior confirmed
serial evidence was NACK at 0x38, not evidence identifying a working address.
