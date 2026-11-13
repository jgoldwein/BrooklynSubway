# Manual live display (0.2.16)

1. Install the update and confirm version 0.2.16 on the status page.
2. Connect the verified 5 V, 3 A center-positive barrel supply; the rail and
   polarity checks must have passed. USB may remain connected for serial logs.
3. Wait for fresh MTA data, sign in, and follow Enable / disable live display.
4. Check the supply confirmation and select Enable live display.
5. Observe route colours, steady stopped trains and pulsing approach/departure
   stations through a feed poll. From 0.2.17, POWER toggles the lights off/on
   within that boot's approved live session. Holding it toggles only once.

Live output is capped at 16/255 brightness (about 6%), with the existing 2000 mA
LED-load estimate budget. These are estimates, not measured current protection.
The logical brightness setting can reduce brightness further; route selections
apply. No enable setting is saved: every boot starts OFF. Enable requires a
healthy startup, fresh or usable partial data, a released POWER button and no
pending recovery. It is unavailable on XIAO and non-network production builds.

The service uses an 8192-byte-stack FreeRTOS task on core 1 at priority 2,
serviced every 20 ms. It copies committed TrainFrame/status/settings snapshots
under a mutex and renders the route colours at the physical cap. The output
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
POWER resume permission: re-enable from the web page. Only a POWER pause can be
resumed with POWER; approval is never stored across a reboot.

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
