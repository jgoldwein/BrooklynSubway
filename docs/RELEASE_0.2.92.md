# Brooklyn Subway 0.2.92

Adds brief monochrome sound icons and labels to OLED **page 1/8**:

- Checkmark: Saved / System ready.
- Wi-Fi arcs: Connected.
- Train: Feed ready.
- Down arrow: Service lost.
- Up arrow: Restored.

The icon and label briefly replace the train-count line; the clock and frame
remain visible. Counts return four seconds after the actual cue started.
Other pages are not interrupted or switched. Setup, firmware updates,
diagnostics and brightness feedback retain priority. POWER-off remains off.
Routine knob ticks and muted sounds do not create an icon.

Uses five small 8x8 bitmaps with the existing OLED worker and I2C driver. No new
task, dependency, wiring, audio pattern or saved setting is introduced.

Validation: all 76 OLED runtime scenarios pass, including exact expiry,
millisecond rollover, clock retention, new-event replacement and overlay
priorities. Actual bitmap pixels were rendered for visual inspection. Production
and bench network builds pass. Physical OLED readability still needs confirmation.

Installing this signed update preserves settings. Keep power connected during
installation and use **brooklyn-s3-network** for the production PCB.
