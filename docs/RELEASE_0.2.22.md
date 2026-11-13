# Brooklyn Map 0.2.22

- Fix a startup dependency introduced with the OLED clock: probing, display
  initialization and the first frame now run entirely inside the OLED task.
  Start the live LED service before queuing optional OLED work. Application
  startup no longer waits for an OLED response.
- Reuse the board-initialized I2C bus instead of calling Wire.begin again from
  the graphics library. Keep SDA GPIO8 / SCL GPIO9 and the SSD1309 driver.
- Use a bounded transport with 50 ms I2C timeout. Latch the first failed transfer
  and skip remaining commands/framebuffer writes. Clean up a short-write
  transaction. Stop the OLED task on failure without stopping the map/web.
- Detect held-low bus lines and missing OLED addresses; skip the display and log
  the reason. Add serial markers before initialization and after the first frame.
- Preserve OLED title/New York clock, browser live-value refresh, train rendering,
  brightness limits and all manufactured pin assignments.

Validation: 29 host runners including OLED success, held-low bus, absent device,
initialization failure, short writes, first-frame failure and task allocation
failure. Verify that begin() returns without I2C work and subsequent transfers
stop after a failure. Production and bench builds pass. The reported physical
startup hang is consistent with the old blocking path; board-side confirmation
and OLED image verification remain necessary after installation.
