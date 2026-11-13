# Live display (0.2.20)

1. Connect the verified 5 V, 3 A center-positive barrel supply; the rail and
   polarity checks must have passed. USB may remain connected for serial logs.
2. Install the update and confirm version 0.2.20 on the status page.
3. Live output starts automatically after healthy startup and fresh MTA data.
4. POWER toggles the lights off/on. Holding it toggles only once. Holding POWER
   at boot cancels automatic start; release and press again when data is fresh.
5. A normal TEST press/release runs All subway lines quickly (about 35 seconds).
   The web dropdown keeps the slower, roughly four-minute version. The separate web live enable/disable control has been removed.

Live output is capped at 16/255 brightness (about 6%), with the existing 2000 mA
LED-load estimate budget. These are estimates, not measured current protection.
The logical brightness setting can reduce brightness further; route selections
apply. GPIO1 stays LOW through startup checks and while awaiting data. Default
startup intent is ON; no separate saved enable setting is used. Enable requires a
healthy startup, fresh or usable partial data, a released POWER button and no
pending recovery. It is unavailable on XIAO and non-network production builds.

The service uses an 8192-byte-stack FreeRTOS task on core 1 at priority 2,
serviced every 20 ms. It copies committed TrainFrame/status/settings snapshots
under a mutex and renders station-motion/segment-route colours at the physical cap. The output
task owns live rendering; main-loop feed retrieval never owns its working
frame. Blink phase comes from monotonic time, independent of new feed commits.
Main-loop logical rendering remains separate. No new library dependency.

From 0.2.18, approach pulses speed up using predicted arrival times. Previous
stations can show a short trail that slows and clears within 30 seconds. These
are estimated movement cues, not measured distance; overlays do not add trains.
Missing/stalled predictions use a fixed blink. STOPPED_AT stays steady. Set the
MTA update wait on the signed-in status page (15-120 seconds; default 15).

The task debounces active-low POWER for 30 ms independently of network polling.
A held-at-boot button cannot enable the display. POWER pause preserves that
boot's approval, and the next press can resume with healthy fresh data. It also
detects the manufactured 10-second rotary
hold, stops output, and queues Wi-Fi recovery for the main loop. The main-loop
recovery path and diagnostic rotary recovery remain in place. The latest frame's
commit timestamp travels with its snapshot; a stalled main loop cannot keep a
frame physically lit beyond the existing five-minute hard-stale limit.

Diagnostics, Wi-Fi setup, LAN-server shutdown and OTA installation stop the
live session before using the LEDs, reconfiguring Wi-Fi or writing firmware.
Driver/render failures stop it too. All stops send black before GPIO1 LOW;
startup/arming reuses the existing post-enable black-frame interlock. The task
and stop operations share one mutex; after stopping, diagnostics can own the
backend without the task touching it. These maintenance/fault stops revoke the
automatic restart intent. Once healthy fresh data returns, a physical POWER
press can request a checked restart; maintenance never automatically resumes.

Exception: a successful physical TEST-button sequence restores its prior live
intent once data is fresh. A previously POWER-paused display remains paused
and retains POWER resume. Aborted/failed tests stay OFF; a later POWER press can
request a fresh-data, driver-checked restart unless recovery is pending. TEST is sampled in the
background task, queued for the main loop after an active feed cycle, and
suppressed during diagnostics to avoid duplicate or overlapping tests. Fast mode
uses 25 ms per colour, versus 250 ms for the web version. The same 302 map LEDs,
RGB levels, black-first/last and POWER/recovery abort rules apply.

From 0.2.17, status/JSON and the serial heartbeat report successful live frame
transmissions, occupancy/approach-data change count, last live-frame age (HTML),
and blink-eligible LEDs. Data-change counts exclude repeated identical snapshots
and blink phase changes. They count data received, including while paused;
transmit counters count successful driver writes, not optical confirmation.
Most station lights can remain steady even with fresh changing data. A stopped
train sharing a pixel suppresses an approaching blink.

`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains set: generic arm/transmit APIs
stay locked. Only production network builds add
`BROOKLYN_ALLOW_LIVE_LED_OUTPUT=1`. Dedicated live backend entry points check
startup/mapping health, frame size, per-component cap and total estimated
current. Board pins and manufactured files are unchanged.

Host tests cover simulated network stalls with continued blink updates,
POWER/recovery stop, stale deadlines, millisecond rollover, full-map current
and brightness limits, startup and driver failures, black-first/black-last
hardware sequencing, existing diagnostic compatibility and authenticated POST
endpoint policy. Physical colour, blink timing, POWER response and current
remain to be observed on the production board.

The separate complete-line diagnostic ordering correction remains queued
after the MTA representation work; this release preserves both full tests.

From 0.2.20, station colours encode movement: orange (#FF6000) transitions to
cyan (#00FFFF) during the final 30 predicted seconds before arrival. A stopped
train is steady blue. The departure trail blends blue to magenta while fading out over its bounded
30-second lifetime. Missing/stalled predictions stay orange with the existing
fixed blink; no colour progress is invented. Stopped occupancy wins. Route-family
colours remain on mapped segment LEDs, and status/diagnostic colours are unchanged.
Interpolation happens before brightness/current limiting. Low brightness such
as 5/255 has only a few available channel levels, so visible steps are possible.
The new unsaved brightness default is 5/255; existing NVS settings are retained.

U23 is the separate discrete Avi-location LED on GPIO14, active HIGH through
R60 (1 kOhm). From 0.2.20 it glows at 8/255 PWM duty (2 kHz) while a complete
fresh frame is available, data age is at most 90 seconds, networking is Ready,
and startup checks are healthy. It clears during setup, stale/partial data,
maintenance and OTA. Map POWER pause leaves this feed-health indication active.
LEDC channel 4/timer 2 is separate from the buzzer channel 7/timer 3.

## Status page live values

From 0.2.21, the page updates MTA counts, cycle outcome, display state, data ages and
uptime in place from `/api/status`, about every five seconds after each response.
It preserves sign-in fields and unsaved settings, pauses requests in hidden tabs,
and reports delayed refreshes rather than silently presenting old values as live.
This browser refresh does not change the MTA polling interval.

## OLED clock (0.2.21)

The user-confirmed SSD1309 128x64 four-pin I2C module displays "Brooklyn Subway"
and New York local time (hours, minutes, seconds, AM/PM), with US daylight saving.
Before valid SNTP time it displays "Syncing time...". A separate low-priority
4096-byte-stack task refreshes once per second, independently of feed polling.
Only the OLED task uses I2C after initialization. U8g2 2.36.18 supplies the
SSD1309 driver and fonts; the framebuffer is 1024 bytes. Missing hardware is
logged and skipped; I2C uses the manufactured GPIO8 SDA / GPIO9 SCL pins.

### OLED startup isolation (0.2.22)

Application startup queues the OLED task after starting the live service; it
never probes or initializes the panel inline. The worker logs each initialization
stage and reuses the board's existing Wire bus. Held-low lines, missing devices
and transfer failures stop only the OLED worker. Transactions time out at 50 ms;
a latched error suppresses the remaining initialization/framebuffer writes.
If upgrading from a board that stalls with the OLED connected, disconnect power,
unplug the OLED, boot and install the update, then disconnect power before
reconnecting the display. Serial output at 115200 identifies the failed stage.

### Proven OLED initialization profile (0.2.23)

The user supplied a working ESPHome configuration for this display using
SSD1306 128x64 at 0x3C. Firmware now uses that initialization profile instead of
the SSD1309 profile inferred from the product listing. The address is fixed to
0x3C. Keep Wire controller 0, SDA GPIO8, SCL GPIO9, 100 kHz, the one-second clock,
and all startup isolation and bounded-transfer behavior from 0.2.22. Physical
image verification remains necessary; this change does not prove the cause of
the previously reported startup stall.

### OLED bus ownership (0.2.24)

U8g2's generic GPIO initialization sets its assigned I2C pins to OUTPUT. With
our pre-initialized Wire bus and custom transport, this can disrupt communication
between a successful address probe and the display commands. The graphics
object now receives no GPIO pins; Wire retains ownership of manufactured GPIO8/9.
Try requested 7-bit address 0x38 first, falling back to the known ESPHome 0x3C.
Serial output records each probe result and the first transfer error code.
