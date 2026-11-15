# Private COM6 build 0.2.55 - greeting, clock frame and quiet hours

## OLED and navigation

Dedication now reads Happy Bar Mitsvah! with Avi centered beneath it. Date,
Goldweins message and all eight scrolling names are retained. Main clock has a
thin 128x64 outline. A nonblocking five-second startup dedication is enabled by
default and can be disabled; turning skips it. Setup and POWER take priority.
The full dedication remains selectable. MODE button remains reserved.

## Settings

Signed-in Screen > Quiet hours and startup greeting opens /screen-options.
Quiet hours start disabled. Defaults: 22:00-07:00 New York time, OLED cap 1/255,
map LEDs Off. User can choose map Off, Dim with a 1-16 ceiling, or normal.
The schedule follows New York DST; while time is unknown, enabled quiet hours
stay quiet until synchronization. Saved normal brightness is never overwritten.
OLED contrast is temporarily capped, with normal value restored afterward.

Quiet hours mute startup, feed-ready, rotary, brightness and diagnostic sounds.
Explicit visual LED diagnostics remain available. Map Off clears/unarms live
output (GPIO1 LOW) and turns U23 off. Dim caps live map/status LEDs and U23.
Normal leaves their normal brightness. POWER choices remain independent; ending
quiet hours cannot revive a manually powered-off or fault-disabled session.
Fresh-data/health checks still gate restart. Quiet pause has a distinct indication
ending reason. Ordinary live LED restore intent survives successful diagnostics.

Preferences use one validated 64-bit NVS value, bkmap_cfg/screen_opts, so one save
covers startup flag, schedule, OLED cap and map mode/ceiling. Invalid loads use
defaults; failed saves retain current settings. No new dependency. 64-bit runtime
copies use a short critical section; GPIO/I2C calls stay outside it. U23 GPIO14,
buzzer GPIO21, OLED GPIO8/9, LED_ENABLE GPIO1 and all existing channel pins are
unchanged. Quiet policy is serviced by the live task even during a long manual
diagnostic. U23 startup flashes are omitted for scheduled Off/Dim.

The new settings page is separately rendered, avoiding expansion of the bounded
home form. GET and POST are explicitly in authorization policy, CSRF required on
save, sign-in returns to that settings page, strict fields/time/brightness bounds
are validated. Map brightness is a ceiling and cannot increase normal brightness.

## Verification and install

All 31 host suites and four target builds pass. Coverage includes window bounds,
overnight/day schedules, DST transitions, unknown time, preferences persistence/
failed save, 64-bit high map bits, startup timeout/skip/POWER priority, border and
text bounds, all sound suppression, discrete dim cap, LED off/unarm/dim/restore,
manual POWER/fault preservation and freshness gating. Browser QA passes at
320/390/768/1100 including native settings submission and active/default states.
Largest escaped home page: 24336/24576 bytes. Static production RAM: 198316 B.

Frozen dist/0.2.55/factory flashed only to serial-verified COM6
28:84:85:34:1F:74 at 115200 baud. Full hashes verified and hard reset completed.
Serial confirms startup dedication, OLED first frame, firmware health confirmation,
8/8 feeds and live output. Physical appearance/quiet schedule operator check
remains. Master COM5 remains 0.2.50, read-only checked Fresh 8/8 afterward.
No stable public release/tag created. User is temporarily disconnecting COM6 to
work on the OLED case; both USB and barrel power should be disconnected for
handling wiring. Do not operate COM6 again until it is reconnected/identified.
