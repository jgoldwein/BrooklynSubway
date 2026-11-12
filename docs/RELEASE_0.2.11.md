# Brooklyn Map 0.2.11

- Preserve the existing four rising startup notes, then append a 250 ms pause
  and a synthesized, descending subway-style ding-dong.
- Appended notes: 2620 Hz for 600 ms, 90 ms silence, 2120 Hz for 750 ms.
  The complete sound sequence is bounded to 2265 ms and retains 50% PWM.
- Reference: John CS Keston's original NYC subway field recording,
  [Classic MTA Closing Doors Chime](https://audiocookbook.org/classic-mta-closing-doors-chime/).
  Local spectral analysis of repeated chimes estimated approximately 655 Hz
  followed by 530 Hz. The firmware shifts these estimates up two octaves for
  the small piezo. This is an approximation, not an official MTA pitch/timing
  specification or a recording playback. No recorded audio is distributed.
- DIM-at-boot mute suppresses the entire sequence. Startup-failure silence,
  PWM failure cleanup, final GPIO21 LOW, U23 flashes, and addressable output
  lock remain unchanged. Hardware is the same GPIO21/BUZZER3 piezo.

Validation: all 25 host runners and four firmware builds pass. Startup tests
verify the original motif remains first, the separating pause, both appended
notes, exact total duration, mute, and safe cleanup at every note failure.
Physical listening validation of the appended pattern remains pending.
