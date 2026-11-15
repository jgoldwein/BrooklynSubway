# Private COM6 build 0.2.67 - MODE route focus, green NETWORK, Device MAC

MODE (GPIO41) toggles temporary route focus. Initial line2; turn GPIO15/16 knob
through18 logical routes, with wrap and existing click feedback. OLED instantly
identifies the selected line, mapped train/light counts, Turn knob: pick line,
MODE: all saved lines. MODE LED307/data13 is amber during focus. MODE again exits
and resumes the prior normal OLED page/browse state; idle timers pause in focus.
Last focus choice is retained in RAM only, reset by reboot.

Saved route mask is never written by focus. One-bit focus overrides even an
all-hidden saved mask; web settings changed during focus become the normal mask
on exit, not an old entry-time copy. Live renderer applies this temporary mask
under its existing mutex. Status indicators, data acquisition/retention, route
mapping, train colors, brightness/current caps and freshness safety unchanged.
POWER/quiet modes retain focus selection while honoring display power/dim rules.
TEST/easter eggs retain their functions/overlay priority and return to focus.
ROUTE exits focus then does its normal Lines/Auto/Pause action. Short rotary
press is ignored in focus; ten-second recovery clears focus and opens setup.
MODE boot-held/release debounce and diagnostic-exit button reset use existing
TestButton policy; generic controls relinquish duplicate MODE sampling.

Read-only status JSON adds routeFocusActive, routeFocusRoute and effectiveRouteMask;
routeVisibilityMask remains saved settings. Web overview labels active focus;
monitor visibility uses effective selection. Largest escaped home24407/24576B,
so future large UI additions should use separate pages/assets rather than grow
Home further. OLED focus work stays in its worker, all callers publish snapshots.

NETWORK LED251/channel8 index1/data13 now green when connected/ready (same
brightness cap); red connecting/unavailable, amber time sync, cyan setup retained.
Blue had been a firmware convention, not a universal standard. Device OLED final
row now shows Wi-Fi station MAC,17uppercase colon-separated characters plus MAC
label fits21chars/126px. Snapshot uses WiFi.macAddress(byte buffer), no String
allocation for MAC. GPIO/power/timing assignments unchanged, no dependencies/NVS.

Validation: all34 host suites pass across full run and targeted navigation rerun
(the new test fixture initially retained an earlier saturation flag; reset it and
reran successfully). All four builds pass. All18 route filters checked against
actual rendered pixels/status exclusion, saved-zero override/live saved update,
wrap, quiet restore/recovery/setup, OLED focus/setup/power/test/return priorities,
MAC width, networkgreen and saved/effective JSON covered. StaticRAM199004B (+64),
flash1316169B, frozen app1316528B. Prior branding/font/weather retained.

Frozen067 flashed ONLY verifiedCOM6serial28:84:85:34:1F:74 at115200, all hashes
verified. Healthconfirm/OLED3C/liveframes/feeds8of8 and weatherHTTP200 observed.
Operator MODE/knob events observed; API focus2/mask1/4visible -> exit/savedmask262143/
68visible -> focus5/mask8/0visible, with savedmask262143 unchanged throughout.
Serial2->Z wrap and further S/R line choices observed. A line with zero visible
lights can have no currently represented trains; no fabricated lights are added.
See captures/focus067-live.json and startup-067-extra-COM6.txt. Device MAC physical
appearance and NETWORK color still operator visual checks. COM5 untouched050.
No public release/tag/push. Alarm/per-feed monitoring remain later work.
