# Integrated guarded LED diagnostics

## Optional full sequence added in 0.2.13

Joel requested a manually started **All channels** option with channel-number
beeps: CH1 gets one beep, CH2 two, through CH8 eight. The existing GPIO21
piezo emits 4000 Hz / 50% PWM, 80 ms beeps with 80 ms gaps and a 300 ms
pause before each channel walk. LEDs remain disabled during the cue. The
selected channel then walks one red=4/255 pixel at a time at 500 ms per pixel,
clears, and disables output before the next cue. Total is about 2 min 40 sec
plus transmission overhead; allow about three minutes.

The web page acknowledges the request immediately, then the sequence runs.
It reopens diagnostics after three minutes and reports completion or stop;
physical visual success still requires observation. Hold POWER to stop the
entire sequence, including cues. Cue waits check POWER every 20 ms; pixel
walks check between pixels. Startup DIM mute applies only to startup, not
these explicitly requested identifying cues. No test starts automatically
at boot, and normal live-map physical output remains locked.
The manufactured 10-second rotary-push Wi-Fi recovery hold remains active:
it stops the diagnostic, clears/disables LED outputs, then enters setup.

## All-channel walks added in 0.2.12

After successful LED42/CH1 observations, Joel authorized the remaining
channel tests. Use `/hardware-test` to select CH2, then CH3 through CH8,
one test at a time. Counts are CH1 37, CH2 38, CH3 37, CH4 37, CH5 42,
CH6 37, CH7 38, CH8 40. Every walk lights only one dim red pixel at a time
for 500 ms, takes approximately 19–21 seconds, and ends with black plus
GPIO1 LOW. Hold POWER to abort between pixels. Record visual results before
moving to the next channel; software completion alone is not visual success.

CH8 starts with four front status indicators: SYSTEM (LED252), NETWORK
(LED251), MTA FEED (LED306), MODE (LED307), then 36 map pixels. Red during
this diagnostic is a test color, not a live network/feed error indication.
U23 remains a separate discrete indicator and is not part of this walk.

The existing GPIO data paths are 4/5/6/7/10/11/12/13 through U14/U15 and
R17–R24. No pin/current/brightness changes. No all-channels automatic sweep
or full-map output is enabled. Full-map/load validation remains a later step.

## Observed results — 2026-11-12

Joel reported both the LED42-only test and CH1 walk ran correctly. This is
user-observed low-brightness test success; current and timing were not measured.
Other channels and normal full-map physical output remain unvalidated.

LED42 is the F-line 18 Av station light between Ditmas Av and Avenue I in the
lower-middle map area. User reported no visible light with barrel power in the
first physical test, then subsequently observed 18 Av light red briefly. This
confirms visible first-pixel operation, not validation of the full chain.

## CH1 walk added in 0.2.7

After reporting LED42 lit red, Joel authorized walking CH1 one LED at a time.
Select **CH1 walk** on `/hardware-test`, check the electrical confirmation,
and click **Run selected test**. It visits all 37 physical CH1 indexes in data
chain order at red=4/255 for 500 ms each (about 19 seconds total). Only one
pixel is lit at a time; all other channels remain black. It finishes by
clearing the LEDs and disabling GPIO1. Hold POWER to abort at the next pixel
boundary (within approximately 0.5 seconds). Feed/page servicing pauses for
the test. TEST-button hold/release still performs the original LED42-only test.
Serial output identifies each physical index/designator; this is chain order,
not a logical route animation. Observe and record any missing or unexpected
pixels. Other channels and normal map illumination remain outside this scope.

From 0.2.6, the separate yellow U23 Avi-location indicator briefly flashes twice
after local startup checks. It works from either logic supply and returns LOW.
It does not enable addressable output or indicate network/feed readiness. The
four addressable status LEDs require the separate barrel-fed 5 V LED rail.

Scope approved on 2026-11-12: user confirmed correct measured rails and a
5 V, 3 A center-positive supply, then authorized the integrated guarded test.
This supersedes the USB-only restriction solely for the following manual test;
it does not authorize normal map output, simultaneous multi-LED illumination,
or full-brightness tests. The CH1 extension above is separately authorized.

Affected hardware: GPIO1 LED_ENABLE/Q1/U14/U15; GPIO4 CH1 data through U14/R17
to LED42 (CH1 index 0). All eight existing data GPIOs and chains receive black
frames; only the selected test pixel receives nonzero data. No pin, topology,
or PCB changes. POWER abort reads the existing active-low GPIO40 switch.

Normal builds retain BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0. The production
environment enables BROOKLYN_ALLOW_LED_DIAGNOSTIC=1 for these fixed tests only.
Normal arm/transmit APIs remain locked. GPIO1 stays LOW at boot and after the
test. Initialization/mapping failures refuse the test and disable output.

## Procedure

1. Install the integrated diagnostic firmware with external power disconnected.
2. Verify the running version and Fresh feed state. Confirm the measured rails,
   barrel polarity, and regulated 5 V, 3 A supply. Power down before connecting
   the barrel supply; then reconnect USB for diagnostics if desired.
3. On the administrator page, open Hardware diagnostics (or `/hardware-test`).
   Select LED42 only, check the electrical confirmation, and click Run selected test.
   Alternatively press and hold TEST (GPIO47/SW5) for at least three seconds,
   then release. A TEST button held at startup does not trigger the test.
4. Observe exactly LED42 dim red for two seconds, followed by darkness. The
   serial log reports sequence completion, not visual/electrical success.
5. Stop and disconnect power safely on heating, unexpected light, resets, or
   failure to extinguish. Do not repeat a failed test to work around a defect.
6. Record the observed LED, color, duration, and any measured current. Return
   to USB-only operation after observation. Further channels need a later step.

The synchronous test finishes before feed polling or OTA processing resumes,
so network stalls cannot extend the pixel dwell. It uses the existing
black-first interlock, a dedicated black frame with only the test pixel red=4/255,
then black again before disabling GPIO1. It never uses the live map framebuffer.

The nominal supply rating is now 3000 mA and the LED estimate budget 2000 mA,
leaving nominal headroom for the MCU/peripherals. These are software estimates,
not measured current protection; no full-map physical output is enabled.

Automated fake-hardware tests verify startup refusal, allocation failure,
black/color/black order, exactly one low-level red pixel, 2000 ms dwell,
final enable LOW, and normal-output APIs remaining locked. Walk tests also
verify all 37 indexes in order, 500 ms per pixel, and clean POWER abort before
the first pixel or partway through the chain.

## Separate full subway-line RGB test

The hardware diagnostics page now offers All subway lines in addition to the
unchanged full channel test and individual channel tests. The route sequence is
2, 3, 4, 5, A, B, C, D, F, G, J, L, M, N, Q, R, S, Z, using the generated
semantic mapping across physical channels. Within each route, LEDs follow the
stable physical-map order, not geographic station order.

Each of the 302 map LEDs cycles red, green, blue at 4/255, 250 ms per color,
one LED at a time. A black frame separates pixels; the four status LEDs are
excluded. Allow about four minutes. Serial diagnostics name the line, designator,
channel and index. Channel-number beeps remain exclusive to the channel test.
POWER and rotary Wi-Fi recovery abort the route test; startup checks and semantic
mapping validation must pass before output is enabled. Each route ends black with
GPIO1 LOW. Normal live-map output stays locked. Firmware cannot verify the visual
result. Feeds and HTTP servicing pause during this manual diagnostic.
