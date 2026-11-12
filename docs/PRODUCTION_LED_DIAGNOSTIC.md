# Integrated first-LED diagnostic

Scope approved on 2026-11-12: user confirmed correct measured rails and a
5 V, 3 A center-positive supply, then authorized the integrated guarded test.
This supersedes the USB-only restriction solely for the following manual test;
it does not authorize normal map output or multi-LED/full-brightness tests.

Affected hardware: GPIO1 LED_ENABLE/Q1/U14/U15; GPIO4 CH1 data through U14/R17
to LED42 (CH1 index 0). All eight existing data GPIOs and chains receive black
frames; only LED42 receives nonzero data. No pin, topology, or PCB changes.

Normal builds retain BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0. The production
environment enables BROOKLYN_ALLOW_LED_DIAGNOSTIC=1 for this fixed test only.
Normal arm/transmit APIs remain locked. GPIO1 stays LOW at boot and after the
test. Initialization/mapping failures refuse the test and disable output.

## Procedure

1. Install the integrated diagnostic firmware with external power disconnected.
2. Verify the running version and Fresh feed state. Confirm the measured rails,
   barrel polarity, and regulated 5 V, 3 A supply. Power down before connecting
   the barrel supply; then reconnect USB for diagnostics if desired.
3. On the administrator page, open Hardware diagnostics (or `/hardware-test`).
   Check the electrical confirmation and click Run two-second LED42 test.
   Alternatively press and hold TEST (GPIO47/SW5) for at least three seconds,
   then release. A TEST button held at startup does not trigger the test.
4. Observe exactly LED42 dim red for two seconds, followed by darkness. The
   serial log reports sequence completion, not visual/electrical success.
5. Stop and disconnect power safely on heating, unexpected light, resets, or
   failure to extinguish. Do not repeat a failed test to work around a defect.
6. Record the observed LED, color, duration, and any measured current. Return
   to USB-only operation after observation. Further channels need a later step.

The synchronous test finishes before feed polling or OTA processing resumes,
so network stalls cannot extend the two-second dwell. It uses the existing
black-first interlock, a dedicated all-black frame with only LED42 red=4/255,
then black again before disabling GPIO1. It never uses the live map framebuffer.

The nominal supply rating is now 3000 mA and the LED estimate budget 2000 mA,
leaving nominal headroom for the MCU/peripherals. These are software estimates,
not measured current protection; no full-map physical output is enabled.

Automated fake-hardware tests verify startup refusal, allocation failure,
black/color/black order, exactly one low-level red pixel, 2000 ms dwell,
final enable LOW, and normal-output APIs remaining locked.
