# Private COM6 build 0.2.61 - Avi's easter eggs

Hold TEST + ROUTE together for at least two seconds, then release BOTH buttons.
Each fresh chord launches the next effect; the sequence resets at reboot:

1. Celebration (6 s): Happy Bar Mitsvah / Avi Yao, original rising/falling triads,
   one dim complete route at a time in alternating colors.
2. Avi's Subway Express (7 s): playful cyan light chaser through the map's PCB
   order. This is an animation, not a geographic train position or coverage test.
3. Credits (12 s): Goldweins message and all eight scrolling names, in the
   established order: Joel, Marlene, Marc, Kim, Juliana, Levi, Robbie, Eric.
4. Avi Yao in Morse (7.5 s): AVI YAO = .- ...- .. / -.-- .- --- at 700 Hz,
   100 ms units, dot 1 / dash 3 / symbol gap 1 / letter gap 3 / word gap 7.
   OLED identifies the current letter and dot/dash; final one-second title hold.
5. Anniversary preview (8 s): Avi Yao / September 26, 2026 / A day to remember.

On September 26 in New York time, the normal dedication also appears
once per year in the current power session when the OLED can show Main.
An existing startup/dedication display counts as that greeting, avoiding an
immediate repeat. Turning skips it, POWER/setup take priority; it has no extra
sound or map animation. A reboot resets this in-memory yearly marker.
Date calculations are cached at one-second intervals, including DST conversion.

MODE stays reserved. Normal TEST and ROUTE releases remain intact; overlapping
chords suppress both actions through debounced release. Boot-held chords do not
launch, and diagnostic exit discards pending/held chord state. Rotary ten-second
Wi-Fi recovery is unchanged. POWER cancels within the polling interval plus
frame/tone operations; recovery also stops. Manual effects pause main-loop web
servicing and feed-result handoff for at most 12 s; the feed worker can continue.
No new tasks, heap allocation, dependencies, pin assignments or NVS keys.

GPIO1/data4,5,6,7,10,11,12,13 map animations capped at4/255 and existing2A budget,
with status LEDs excluded. Quiet Off suppresses map animation; quiet Dim caps it;
quiet Normal keeps the4 cap. Existing selected POWER mode controls whether map
animation is allowed; OLED worker preserves its power state and quiet contrast.
Buzzer GPIO21 timer3/channel7 reuses shared indicator locking and quiet suppression;
manual sounds do not inherit DIM-at-boot startup mute. Knob cleanup cannot silence
an owned effect tone. U23 feed indicator wiring/meaning unchanged.
Black before disabling LED output on completion/cancel/quiet-off; buzzer LOW and
OLED overlay cleared on every exit. Successful effects can restore only prior
live intent through existing freshness/health gates; cancellation does not resume.

Validation: all33 host suites pass; final targeted LED tests also verify the last
transmitted frame is black on POWER/recovery exit. All four builds pass. Exact
Morse timeline, all names visible, invalid kind/bounds, chord boot/short/rollover,
quiet/power/status exclusion, PWM ownership/failure, OLED setup/power/restore,
New York midnight/date bounds and no repeated same-session anniversary covered.
Production static RAM198476B (+152 from060), flash1310089B (~20% app slot).

Frozen061 flashed ONLY serial-verified COM6 28:84:85:34:1F:74 at115200, all hashes
verified. OLED initialization observed; operator unplug/power-cycle interrupted the first
startup capture after the verified upload. On reconnection the same COM6 serial
was verified, increasing uptime/live frames and complete8/8 feed cycles observed
(captures/startup-061-extra-COM6.txt). No firmware-crash conclusion is supported
by that operator intervention. Physical effects still need operator
confirmation. COM5 remains050; no stable release/tag/push. Deferred operator
POWER and quiet-hours checks remain pending. Train representation unchanged.
