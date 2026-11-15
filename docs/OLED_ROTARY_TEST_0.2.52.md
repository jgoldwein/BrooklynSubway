# OLED rotary navigation - private COM6 build 0.2.52

COM5 remains the master on 0.2.50 for stability monitoring. This build is local
and not published as a stable release.

## Controls

- Turn the rotary knob to browse Main, Activity, Connection, Device and Lines.
- On Lines, press the knob to enter line browsing; turn to choose a subway line.
  Press again to return to screen browsing. The footer says Turn:screen or
  Turn:line, and the header says Auto or Paused.
- ROUTE is a shortcut into line browsing. On Lines, ROUTE toggles automatic
  browsing, advancing one line every five seconds. Turning pauses Auto.
- MODE remains an alternate next-screen button; it leaves line browsing.
- Hold the knob for 10 seconds to enter the existing Wi-Fi setup flow. A long
  hold is not interpreted as a short selection when released.
- POWER and brightness behavior are unchanged. Navigation never turns on an
  OLED that POWER has turned off. Hardware-test/recovery priority is retained.

Line browsing affects only the OLED; it does not filter or recolor the map.
The selected line is remembered while switching screens and resets at reboot.
Main is always the initial screen. The MODE light follows the screen, with
amber for Lines; its existing output/brightness/current guards still apply.

## Screen content

Connection includes the SSID, Wi-Fi signal, feed success count and clear waiting,
missing-feed or old-data text. Long SSIDs scroll one character per second, with
a two-second pause at each end. Short names do not scroll. Unsupported OLED
characters are replaced with question marks; passwords are never displayed.

Lines shows mapped trains for that line and separately the number of illuminated
map LEDs. Three trains sharing one LED are three trains and one light. Counts
come from the committed display frame and last successful physical frame, not
the in-progress download. Hidden lines still have train counts but zero visible
lights. A saturated occupancy count is labeled >=. The freshness label describes
the map update, not independent proof of that line's current train positions.

On the web page, Screen > Screen browsing chooses the idle return to Main:
Stay on this screen, 30 seconds, 1 minute (default), 2 minutes or 5 minutes.
The authenticated/CSRF-protected setting persists as bkmap_cfg/oled_return;
failed saves preserve the current value. Auto browsing does not reset the idle
clock. POWER-off, setup, diagnostic work and physically held controls suspend
idle timing. A changed timeout starts a new idle interval.

## Hardware and validation

Existing pins only: rotary A GPIO15 / B GPIO16 / push GPIO17, ROUTE GPIO38,
MODE GPIO41; OLED U22 SDA GPIO8 / SCL GPIO9; MODE LED307 on channel 8 index 3,
data GPIO13. Live task samples every 5 ms; quadrature uses valid four-transition
Gray-code cycles and cancels bounce/invalid jumps. Physical button actions use
30 ms debounced release with held-at-boot/diagnostic suppression. Short rotary
selection is under one second. The manufactured 10-second recovery stays intact.

All 31 host suites and four firmware targets pass. Tests cover rotary reversal,
bounce and invalid transitions; screen/line context, line wrap and retention;
Auto/Pause/manual pause; timeout/Stay and millis rollover; long press duration;
SSID scroll endpoint pauses and safe text; shared/saturated counts; stopped map
outputs; setting persistence/failure/validation and mutation authorization.
Browser QA passes at 320/390/768/1100 pixels including native screen-return form
submission. Largest escaped full status page is 24196 / 24576 bytes; buffer was
not enlarged. Production static RAM is 198228 bytes (+328 from 0.2.51).
No new dependencies, pin changes, train-rendering/palette/polling changes or TEST
sequence changes. The queued full-line diagnostic ordering improvement remains.

Frozen dist/0.2.52/factory was flashed to serial-verified COM6
28:84:85:34:1F:74; esptool verified hashes. Startup confirms OLED at 0x3C,
a successful 8/8 feed cycle, live output and health confirmation. COM5 was
read-only checked afterward: still 0.2.50, Fresh 8/8, no TLS/transport errors.
Operator still needs to confirm physical rotary direction/detent feel and OLED
readability; do not call that visually confirmed from serial writes alone.
