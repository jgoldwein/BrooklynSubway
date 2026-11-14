# Brooklyn Map 0.2.40

- BRIGHT and DIM adjust the display that is on by itself: LED brightness in
  LED-only mode, OLED brightness in OLED-only mode. Both-on and all-off ignore
  these buttons. Web brightness settings remain available independently.
- One step per debounced release: LEDs 1–16 (existing production cap), OLED
  0–255 in steps of 16. Save changes using the existing brightness settings.
- A brief 45 ms beep rises in pitch with the level. The OLED shows the display
  name and numeric level for about three seconds, briefly waking if necessary,
  then returns to the selected POWER mode. A POWER change clears the message.
- Ignore buttons held at boot, opposing-button chords, presses carried across
  POWER modes, and diagnostic/recovery input. A bounded queue captures presses
  during downloads; changes apply once the active main-loop operation completes.
- Preserve startup/fault LED interlocks, brightness/current caps, OLED address,
  contrast and normal title/clock behavior. No hardware changes or new dependency.

Validation: mode/debounce/step tests, OLED temporary-wake/timeout/cancel tests,
passive-buzzer bounds and failure cleanup. All 29 host suites and all four
firmware targets pass. Responsive browser checks pass at 320/390/768/1100 px.
Physical button feel, sound and brightness remain to be confirmed on the PCB.
