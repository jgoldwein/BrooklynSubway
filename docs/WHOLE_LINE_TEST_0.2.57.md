# Private COM6 build 0.2.57 - complete subway-line RGB test

All subway lines now lights every mapped LED belonging to one logical route
at once: red, green, then blue at 4/255, then clears before the next route.
Order: 2, 3, 4, 5, A, B, C, D, F, G, J, L, M, N, Q, R, S, Z.
This avoids the fragmented appearance of a PCB-wiring-order walk without
inventing a geographic station order or changing any mapping.

- Physical TEST: fast whole-line test, about 10 seconds including output time.
- Web LED diagnostics > All subway lines: about 50 seconds including output time.
- Individual LED walk by line: retains the former one-LED RGB walk, about 4 minutes.
- All channels / individual channel tests retain their counts, numbered beeps,
  ordering and status-light coverage.

The OLED shows the line, RGB color and mapped-light count during a route test.
Setup and POWER display selection retain priority. Completion/abort clears the
label; prior navigation remains selected. Existing OLED address/driver unchanged.
POWER or Wi-Fi recovery stops the sequence. Whole-line waits check cancellation
every 20 ms in addition to output-transfer time. Black frames and GPIO1 LOW on
exit. Whole frames are checked against the existing 2 A software LED budget.
No diagnostic runs automatically at startup. Feeds/web service pause during tests.
Only the successful physical TEST path restores the prior live-output intent,
subject to the existing freshness/health policy; web test behavior is unchanged.

All 31 host suites and four production/bench builds pass. Tests check each of
302 map LEDs exactly once per color, complete single-route frames, 18-route
ordering, status exclusion, both speeds, POWER/recovery/startup guards, black
cleanup, OLED label clearing/setup/power priority and existing individual walks.
Production static RAM 198324 B (+8 B); firmware binary 1306752 B.

Frozen dist/0.2.57/factory flashed only to serial-verified COM6
28:84:85:34:1F:74 at 115200. All hashes verified and reset completed.
OLED ACK/first frame 0x3C, startup greeting-to-Main, image health confirmation,
8/8 feeds and live-output frames observed. Physical whole-line
appearance still needs operator confirmation; tests were not triggered remotely.
COM5 remains 0.2.50; read-only sample during upload preparation was DegradedHold
6/8, last failure SI connect/TLS, recorded in captures/master-after-057.json.
Existing overnight observer process retained. No public release/tag/push.

Try on COM6: press TEST once and watch that each entire line turns red, green,
blue before moving on. For slower observation choose All subway lines in the web
LED diagnostics page. Press POWER if you want to stop.
