# Brooklyn Subway 0.2.78

Adds a gentle pre-update warning to the shared OTA installer used by signed
manual web uploads and GitHub manual/automatic updates.

- Two descending notes, 1200 then 900 Hz, each 100 ms with a 120 ms gap.
  Uses GPIO21 on existing LEDC channel 7/timer 3 at 64/255 duty for a softer cue.
- Quiet hours suppress the tones. PWM setup failures leave the buzzer LOW and
  do not prevent the software update.
- OLED shows Update starting / Do not unplug power for three seconds before
  package body processing and flash writes. Quiet hours retain the visual
  announcement; brightness and power restoration follow the existing overlay.
- No repeated warning during downloading, verification or reboot. Normal
  post-reboot readiness sounds retain their existing policy.

The first update installing this feature is still run by the previous firmware.
The cue applies to subsequent updates initiated by 0.2.78 or newer. USB
bootloader programming remains outside the application's OLED/audio control.

Validation: all 34 host suites and four firmware targets pass. Buzzer tests
cover exact pitches, duration/gap, quarter-duty, quiet suppression and PWM
failure cleanup. All 63 OLED runtime scenarios pass, including the starting
screen/warning and temporary wake of an OLED that was off. Same signing key,
settings storage, power caps and pin map; no new dependency or task.
