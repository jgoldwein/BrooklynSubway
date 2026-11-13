# Brooklyn Map 0.2.13

- Add **All channels** to Hardware diagnostics. It runs CH1–CH8 once, with
  one beep before CH1, two before CH2, through eight before CH8. Cues use the
  existing GPIO21 piezo: 4 kHz, 50% PWM, 80 ms on / 80 ms between beeps,
  followed by 300 ms before the channel lights. Cues run with LED_ENABLE LOW.
- Retain one red=4/255 pixel at a time, 500 ms per pixel, black-first/last,
  channel preflight, and GPIO1 LOW between channels/at completion. POWER
  aborts the whole sequence, including beeps. Any cue/LED failure stops it.
  The 10-second rotary-push hold also stops testing and enters Wi-Fi setup,
  preserving the manufactured recovery contract during the long sequence.
- The page acknowledges immediately and returns to diagnostics after three
  minutes, showing the last sequence result. Feeds and web servicing pause
  during the bounded sequence (~2 min 40 sec plus output overhead). Individual
  channel and LED42 tests remain available. Tests never start at boot.
- Lower the startup ding-dong pair one octave, to 1310/1060 Hz. The existing
  four-note opening, durations, 50% PWM, and DIM-at-boot mute remain. Startup
  mute does not suppress explicitly requested channel-identification cues.

Normal live-map output remains locked. No pin/board/dependency changes.
Physical all-sequence/beep-count and lower-pitch observations remain pending.

Validation: all 25 host runners and four firmware builds pass. Expanded tests
verify cues 1–8 and exact timings, GPIO21/LEDC cleanup, POWER abort during
sound, full 306-pixel ordered sequence, stop-on-cue-failure, stop-on-pixel
abort, startup checks, strict selection, and lowered startup pitches.
