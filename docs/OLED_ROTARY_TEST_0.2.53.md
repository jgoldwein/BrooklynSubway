# Private COM6 build 0.2.53 - rotary response and clicks

- Change the rotary policy from four to two valid Gray-code transitions per
  step, following the user's report that two physical notches were needed.
  Both resting phases are supported; reversal bounce cancels and invalid
  two-bit jumps reset partial movement. Physical one-notch behavior still
  needs operator confirmation on this encoder.
- OLED worker checks for UI changes every 20 ms, instead of waiting a full
  second. A revision counter requests redraw after navigation, data, power,
  contrast or feedback changes. Idle redraw remains once per second. Actual
  input-to-visible latency also includes the existing 100 kHz I2C frame write.
  Brightness feedback remains three real seconds, independent of redraw rate.
- Each accepted rotary step requests a nominal 15 ms, 2000 Hz, 50%-duty click
  on existing BUZZER3 GPIO21 / LEDC channel 7 (timer 3), with a 15 ms gap.
  A bounded eight-click pending queue and try-lock service avoid delay calls
  or waiting behind priority sounds in input sampling. Main-loop brightness
  and feed-ready cues cancel/preempt an active click under the shared lock;
  diagnostics clear it before their cue. Recovery/test paths clear feedback.
- Feed-indicator POWER gating publishes the latest allowed state and retries
  without waiting on the sound lock; normal main-loop feed updates also use
  that latest state. The separate U23 timer and brightness remain unchanged.
- User reserved MODE for a future function. Remove its alternate screen action;
  it only has existing serial press/release logging. MODE LED307 still shows
  the selected screen. Rotary screen/line context and ROUTE Auto/Pause remain.

All 31 host suites and four firmware targets pass. New tests verify an event
per half-step in both phases, immediate screen/line redraw before one second,
idle redraw frequency, real-time brightness duration, nonblocking click/gap,
queue saturation, cancellation, clock rollover, PWM failure cleanup and priority
brightness ownership. Existing feed-ready/startup/diagnostic/power/recovery
checks pass. Production static RAM 198244 bytes (+16 over 0.2.52). No dependency,
pin assignment, train-rendering/palette/polling or web UI changes.

Frozen dist/0.2.53/factory uploaded only to COM6 after verifying serial
28:84:85:34:1F:74; hashes verified. Startup confirmed OLED first frame at 0x3C,
8/8 feeds and live output. Serial also captured an operator rotary screen change.
COM5 master remains 0.2.50, confirmed Fresh 8/8 after upload. No public stable
release/tag is created, preserving the overnight stability baseline.
