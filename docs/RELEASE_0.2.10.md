# Brooklyn Map 0.2.10

- Replace the audible but quiet two-note startup beep with four rising tones:
  2500 Hz / 80 ms, 3000 Hz / 80 ms, 3500 Hz / 100 ms, 4000 Hz / 180 ms.
  Silent gaps are 45 ms; total chime duration is 575 ms.
- Increase 8-bit LEDC duty from 16 to 128 (50% square wave) at the existing
  3.3 V GPIO output. This increases the intended acoustic drive without a
  voltage/pin/PCB change. Actual loudness still requires listening validation.
- The confirmed CPT-9019A-SMT-TR datasheet specifies its current/SPL test
  conditions using 3 Vp-p, 4 kHz, 50% duty. See the hardware context source.
- DIM-at-boot mute, failed-startup silence, bounded note durations, PWM
  failure cleanup, and final GPIO21 LOW remain. Other startup indicators,
  OTA behavior, and the normal map output lock are unchanged.

User confirmed 0.2.9's chime was audible, then requested louder multi-tone
sound. Physical confirmation of this revised pattern is pending.

Validation: all 25 host runners and four PlatformIO builds pass. Startup tests
verify all four pitches/durations, 50% duty, three gaps, mute, failed-check
silence, cleanup on PWM setup failure at each note, and LED_ENABLE LOW.
