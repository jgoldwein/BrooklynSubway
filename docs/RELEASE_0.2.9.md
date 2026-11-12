# Brooklyn Map 0.2.9

BUZZER3 verified from user-supplied part C20181991: Same Sky/CUI
CPT-9019A-SMT-TR, an externally driven piezo transducer. Manufacturer specifies
3 Vp-p rated, 1 to 25 Vp-p operating range, and 1 mA at 3 Vp-p, 4 kHz,
50% duty. This supports the limited 3.3 V GPIO startup test; actual sound and
current remain unmeasured.

[Manufacturer datasheet](https://www.mouser.com/datasheet/2/1628/cpt_9019a_smt_tr-3510475.pdf)

- Add a brief rising two-note startup chime to the production passive buzzer,
  BUZZER3 pad 2 / GPIO21, with pad 1 at GND. User confirmed the passive type.
- Notes are 3000 Hz and 4000 Hz, 80 ms each, separated by 60 ms of silence.
  Use low-duty PWM (16/255 at 8-bit resolution); actual loudness is unmeasured.
- Hold DIM (GPIO39, active LOW) while powering on or resetting to mute that
  startup. The mute input is sampled during safe initialization.
- Buzzer starts LOW, plays only after local self-checks pass and the U23
  startup flashes, then PWM stops/detaches and GPIO21 returns LOW. Failed
  startup checks stay silent. A PWM setup failure is logged and cleaned up.
- Reserve LEDC channel 7 / timer 3; no new dependency or tone task. The XIAO
  remains software-only and does not drive production buzzer GPIOs.

The chime means local startup checks passed, not Wi-Fi or feed readiness.
It runs once per firmware startup, including reset and OTA restart, from
either USB or barrel logic power. Addressable startup output remains disabled.

Validation: all 25 host runners and four firmware builds pass. Startup tests
verify exact note order/duration/duty, silent gap, DIM mute, failed-check
silence, cleanup after first/second PWM setup failure, and LED_ENABLE LOW.
Physical sound/loudness observation remains pending. No board/pin-map changes.
