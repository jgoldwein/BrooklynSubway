# Private COM6 build 0.2.69 - readiness, steady clock, degrees and Avi guides

Startup indicators use only SYSTEM LED252, NETWORK251, MTA306 (CH8 indices0/1/2,
GPIO13), with GPIO1 LOW until safe hardware/map/backend initialization. A150ms
red stage precedes local-check green. Preview cap4/255; MODE/station pixels black.
Saved config/quiet policy loads before preview. No-credential/setup and faults
keep outputs off. A known failure clears black then disablesGPIO1.

Controller continues a status-only frame while awaiting its first usable map:
SYSTEMgreen, NETWORKred->amber->green, MTAred until data, amber for partial/stale,
green forFresh. POWER/quiet/fault/recovery apply. First usable frame hands off to
normal live rendering under unchanged freshness gates. Startup-only flag in
status API/web overview prevents calling lit readiness LEDs simply Lights off.
Normal arm/transmit API permissions remain locked; full PCB live build opts in.

Readiness sound uses sharedGPIO21/LEDC7 timer3 with existing indicator lock.
SYSTEM1000Hz90ms, NETWORK1500Hz90ms, MTA2000Hz100ms, queued at green edges of actual
transmitted status pixels. No repeat on steady green; repeated recovery can cue
again. Runtime cue service has no delay,80ms inter-cue gap, normal knob feedback
retained. Quiet/DIM-at-boot and diagnostic/POWER/recovery cancellation apply.
Boot SYSTEM cue is serviced before legacy startup tune; U23 first-on tone delegates
to the green MTA cue so it does not double-beep. No deferred muted cue replay.

Dedicated clock now uses pinned U8g2 spleen12x24_mn: eight fixed12px cells atx3,
with a leading blank for one-digit hours, colon positions fixed, AM/PMx103. Time,
date and outside rows retain baseline24/42/58. Numeric width96px constant, suffix
endsx119 inside126pxframe. Colons never deliberately blink; seconds still change.
Hiding seconds remains a possible later option, not implemented without agreement.
Weather/clock show degree glyph0xB0+F via their fullLatin1 6x12/8x13fonts, both
verified in pinned font data; no UTF8 decoding assumption/newdependency.

Avi quick start380words, full guide1386words/glossary, links verified, README
operator entrypoint. Firmware/engineering material separate; preview version
clearly labeled. Guides cover startup, colors, controls, route focus, screens,
clock/quiet, Wi-Fi/settings, troubleshooting and secrets. Prepared locally for
consolidated GitHub release, NOT published remotely yet. Alarm/per-feed reporting
remain subsequent work. Operator POWER/quiet checks were deferred.

All34 suites/four builds pass including boot status role exclusion/cap/black-first,
actual green detection, startup map-dark/first-frame handoff, quiet/POWER/setup/
recovery/fault guards, queued cue edges/frequency/mute/PWM failure/no-U23 duplicate,
fixed-cell 1/12-hour render and degree display. Home worst24465/24576B: further
large UI additions need another page or assets. RAM199012B, flash1318177B, frozen
app1318536B. No new NVS keys/tasks/dependencies/pin assignments/board-file edits.

Frozen069 installed ONLY verifiedCOM6serial28:84:85:34:1F:74 at115200; allhashes
verified. Capture confirms initialred->SYSTEMgreen1000Hz, NETWORKgreen1500Hz,
MTAgreen2000Hz, healthconfirm, full8/8 cycle, U23 delegation, live frames and
weatherHTTP200. API firstsampleWaitingForData/startupIndicatorsActive=true/
visibleMapLeds0/live=false, laterFresh8/8/startup=false/live=true. Runtime minimum
stacks feed11664B/main6808B/live2420B; heapminimum29336B. COM5 untouched050, observer
PID38652 still running/waiting. 068 was build-only intermediate, not installed.
No public release/tag/push. Physical readability/audio appearance still operator
observations, not inferred from logs.
