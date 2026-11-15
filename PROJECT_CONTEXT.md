# Brooklyn Active Subway Map — Project Context

## Manufactured hardware baseline

The hardware authority for firmware work is the exact EasyEDA Pro file sent for manufacture:

- **File:** `Brooklyn_Active_Subway_Map_V1.1.eprj2`
- **SHA-256:** `3ecb90c15d79f027a949c047f9c0a6e2f544706750498f9c7cb94209d61b4bc9`
- **PCB title:** `Brooklyn MTA Active Map PCB`
- **MCU:** U17, `ESP32-S3-WROOM-1U-N16R8`
- **Flash / PSRAM:** 16 MB flash / 8 MB OPI PSRAM
- Treat the manufacturing file as **read-only**. Never change PCB geometry, nets, footprints, routing, or artwork as part of firmware work unless Joel explicitly requests a hardware revision.

This context was extracted read-only from the manufactured `.eprj2` database, including the final PCB snapshot and component pad-to-net assignments.

## Core MCU pin map (U17)

| Firmware function | ESP32-S3 GPIO | U17 module pad | PCB net | Notes |
|---|---:|---:|---|---|
| LED output enable | 1 | 39 | `LED_ENABLE` | Drives Q1 (2N7002), which pulls active-low `LED_OE_N` low. **GPIO1 HIGH enables** U14/U15 outputs; LOW disables. Hardware pulldown keeps LEDs disabled during reset. |
| LED channel 1 data | 4 | 4 | `MCU_CH1` | U14 SN74AHCT125 -> R17 -> `DATA_CH1_IN` |
| LED channel 2 data | 5 | 5 | `MCU_CH2` | U14 -> R18 -> `DATA_CH2_IN` |
| LED channel 3 data | 6 | 6 | `MCU_CH3` | U14 -> R19 -> `DATA_CH3_IN` |
| LED channel 4 data | 7 | 7 | `MCU_CH4` | U14 -> R20 -> `DATA_CH4_IN` |
| Rotary A | 15 | 8 | `ROTARY_A` | EC11 SW13; external pull-up; active-low transitions |
| Rotary B | 16 | 9 | `ROTARY_B` | EC11 SW13; external pull-up |
| Rotary push | 17 | 10 | `ROTARY_PUSH` | EC11 SW13; external pull-up; press connects to GND |
| Power-source status | 18 | 11 | `STAT` | From U21 TPS2115APWR status output |
| I2C SDA | 8 | 12 | `SDA` | U22 OLED header pin 4 |
| Native USB D- | 19 | 13 | `USB_D-` | To USB1 through R38 / ESD network |
| Native USB D+ | 20 | 14 | `USB_D+` | To USB1 through R37 / ESD network |
| I2C SCL | 9 | 17 | `SCL` | U22 OLED header pin 3 |
| LED channel 5 data | 10 | 18 | `MCU_CH5` | U15 SN74AHCT125 -> R21 -> `DATA_CH5_IN` |
| LED channel 6 data | 11 | 19 | `MCU_CH6` | U15 -> R22 -> `DATA_CH6_IN` |
| LED channel 7 data | 12 | 20 | `MCU_CH7` | U15 -> R23 -> `DATA_CH7_IN` |
| LED channel 8 data | 13 | 21 | `MCU_CH8` | U15 -> R24 -> `DATA_CH8_IN` |
| Avi-location indicator LED (U23) | 14 | 22 | `USER_LED` | Separate discrete LED through R60 1 kΩ to GND; active HIGH; not addressable |
| Buzzer | 21 | 23 | unnamed | Directly to BUZZER3 pad 2; BUZZER3 pad 1 = GND |
| TEST button | 47 | 24 | `GPIO47` | SW5 to GND, external 10 kΩ pull-up; active LOW |
| ROUTE button | 38 | 31 | `GPIO38` | SW6; active LOW |
| DIM button | 39 | 32 | `GPIO39` | SW7; active LOW |
| POWER button | 40 | 33 | `GPIO40` | SW8; active LOW |
| MODE button | 41 | 34 | `GPIO41` | SW9; active LOW |
| BRIGHT button | 42 | 35 | `GPIO42` | SW10; active LOW |
| UART TX | 43 / U0TXD | 36 | `TX` | U24 header pin 2 |
| UART RX | 44 / U0RXD | 37 | `RX` | U24 header pin 3 |
| BOOT | 0 | 27 | unnamed | SW14 to GND; external pull-up and capacitor |
| EN / RESET | EN | 3 | unnamed | SW11 to GND; external pull-up and capacitor |

Unused / intentionally unconnected module GPIO pads in the manufactured PCB include GPIO3, GPIO46, GPIO48, GPIO45, GPIO35, GPIO36, GPIO37 and GPIO2.

## Confirmed passive buzzer

User confirmed BUZZER3 is LCSC C20181991, Same Sky/CUI CPT-9019A-SMT-TR,
on 2026-11-12. It is an externally driven piezo transducer; manufacturer
specifies 3 Vp-p rated, 1–25 Vp-p operating, 4 kHz rated frequency, and 1 mA
specified at 3 Vp-p / 4 kHz / 50% duty. GPIO21 connects directly to pad 2;
pad 1 is GND. The brief 3.3 V startup pattern uses 50% PWM from 0.2.10,
following the user's request for a louder multi-tone chime. Actual acoustic
level/current are not measured. No board modification.

Source: [manufacturer datasheet](https://www.mouser.com/datasheet/2/1628/cpt_9019a_smt_tr-3510475.pdf)

## LED hardware

- **306** addressable RGB LEDs are populated on the manufactured board.
- U23 (`SZY0402Y`) is the separate discrete Avi-location indicator on the
  `USER_LED` net, driven by GPIO14 through R60 1 kΩ to GND. GPIO14 HIGH turns
  U23 on. U23 is not part of the 306 addressable LEDs or any LED channel.
- Device: `XL-1615RGBC-2812B-S` / LCSC C41413180, 800 kbit/s, WS2812B-compatible protocol.
- Data is split across **8 independent channels** through two SN74AHCT125DR level shifters (U14/U15).
- Channel counts are **37, 38, 37, 37, 42, 37, 38, 40** (total 306).
- U14/U15 output-enable pins share `LED_OE_N`. Q1 makes firmware `LED_ENABLE` active-high: keep GPIO1 LOW until firmware is ready to transmit valid LED data.
- Each channel enters through a 330 Ω series resistor (R17–R24).

### Channel starts

| Channel | GPIO | LED count | First LED | End net |
|---|---:|---:|---|---|
| 1 | 4 | 37 | LED42 | `DATA_CH1_END` |
| 2 | 5 | 38 | LED208 | `DATA_CH2_END` |
| 3 | 6 | 37 | LED226 | `DATA_CH3_END` |
| 4 | 7 | 37 | LED220 | `DATA_CH4_END` |
| 5 | 10 | 42 | LED190 | `DATA_CH5_END` |
| 6 | 11 | 37 | LED194 | `DATA_CH6_END` |
| 7 | 12 | 38 | LED198 | `DATA_CH7_END` |
| 8 | 13 | 40 | LED252 | `DATA_CH8_END` |

The exact physical data-chain sequence is in `data/led_chain_map.csv`.

### Channel 8 status LEDs

The first four LEDs in channel 8 are the four front-panel status indicators, verified by matching PCB coordinates to silkscreen labels:

| Channel 8 index | Designator | Front label |
|---:|---|---|
| 0 | LED252 | SYSTEM |
| 1 | LED251 | NETWORK |
| 2 | LED306 | MTA FEED |
| 3 | LED307 | MODE |

Channel 8 indices 4–39 are map LEDs.

The addressable-LED accounting is 306 total, including four CH8 status LEDs
and 302 subway-map LEDs. U23 is a separate discrete indicator and is excluded
from this addressable topology. LED291 remains an ordinary addressable map LED
(CH5 index 18).

## Front controls and UI contract

Manufactured front-panel labels establish these control meanings:

- SW5 / GPIO47: **TEST**
- SW6 / GPIO38: **ROUTE**
- SW7 / GPIO39: **DIM**
- SW10 / GPIO42: **BRIGHT**
- SW9 / GPIO41: **MODE**
- SW8 / GPIO40: **POWER**
- Rotary: **TURN — NAVIGATE / PRESS — SELECT**
- Wi-Fi recovery instruction on the manufactured board: **press knob 10 sec.** Firmware should implement a 10-second rotary-push hold as Wi-Fi recovery.

## Wi-Fi / setup UI contract

The manufactured artwork identifies:

- Setup SSID: `BrooklynMap-Avi`
- Setup address: `192.168.4.1`
- QR/front text should be treated as a firmware/UI contract unless Joel explicitly changes it in a future board revision.

## OLED / I2C

U22 is a four-pin OLED header:

1. GND
2. 3V3
3. SCL = GPIO9
4. SDA = GPIO8

User confirmed on 2026-11-13: 2.42-inch SSD1309 128x64, four-pin IIC/I2C module.
User subsequently supplied a working ESPHome configuration for the same display:
SSD1306 128x64 at 0x3C, updating every second. From 0.2.23 firmware uses that
proven initialization profile on GPIO8/9. From 0.2.24, probe user-requested
0x38 first, with 0x3C fallback; Wire exclusively owns pin initialization.
User initially reported the OLED working with 0x38 selected; this did not
establish its active address because fallback was enabled. From 0.2.25, use the saved administrator
OLED address (default 0x38); reboot to apply edits, with no address fallback.
From 0.2.29, the saved address is preferred, with fallback probing of
0x38/0x3C/0x3D when needed. Web status reports the active address and I2C error.
No separate reset pin is wired.

Live verification on 2026-11-13: the board on 0.2.29 reports saved/startup address
0x38, active address 0x3C, and successful frame writes (I2C code 0). The original
signed 0.2.24 application was also blank until both USB and barrel power were
removed together for 10 seconds. After the same complete power cycle, the user
visually confirmed the OLED working on 0.2.29 and confirmed all beeps correct.
A CPU-only reset does not remove OLED power. Keep this verified baseline;
the excess steady-blue MTA interpretation remains a separate unresolved issue.

Independent address verification on 2026-11-13 used a temporary Wire-only
scanner on GPIO8 SDA / GPIO9 SCL at 100 kHz, probing 7-bit addresses 0x08-0x77
without OLED initialization or saved settings. Repeated scans with the OLED
connected acknowledged only 0x3C (write-address byte 0x78); 0x38 and 0x3D did
not acknowledge. After the user removed power and unplugged the OLED, repeated
control scans found no devices. Both tests had idle SDA/SCL HIGH and no other
bus errors. This independently establishes the module's actual address as
0x3C, regardless of the saved preferred address 0x38. Logs are in ignored
captures/i2c-oled-connected.txt and captures/i2c-oled-disconnected.txt.
The verified original 0.2.29 application was restored after the scan, retaining
NVS settings; the LAN status confirmed 0.2.29 and Fresh data with OLED unplugged.


## Station confidence revision prepared after 0.2.29

The 0.2.30 source uses steady dim amber for uncertain STOPPED_AT reports,
without the withdrawn 0.2.27 pulsing policy. Blue requires a matching arrival
within the last 30 seconds, vehicle age <=90 seconds and feed/prediction age
<=60 seconds. Shared-pixel priority is recent matching stop, approach, uncertain.
Matching recent reports are still feed evidence, not independent dwell proof.
Four fresh NQRW captures 30 seconds apart confirmed stop changes while status
remained STOPPED_AT; replay still yields 16-20 blue and 1-3 amber map LEDs per
snapshot. Do not promise that all blue lights should disappear. OLED and beeps
are unchanged. See docs/RELEASE_0.2.30.md for scope and validation.

## Configurable blue hold (0.2.31)

The user requested a 20-second maximum blue indication per trip/station,
configurable in the administrator UI. Station blue hold accepts 1-300 seconds,
defaults to 20, persists in bkmap_cfg/blue_hold_sec and applies next poll.
Visit history keys the trip hash pair plus the exact four-character logical
stop ID, independently of physical LED identity. Refreshed feed timestamps do
not restart it. Missing polls retain visits; entries unseen for 30 minutes may
be reclaimed. Capacity exhaustion is amber. Verified movement clears the old
visit. A blueSecondsRemaining frame field allows live expiration between polls.
Shared LEDs use the longest remaining credible stop contribution. Reboot starts
new visit history; genuine long dwells also become amber. OLED/beeps unchanged.
See docs/RELEASE_0.2.31.md.

The same revision adds independent OLED brightness (contrast) 0-255, default
207 matching U8g2's SSD1306 initialization, saved in bkmap_cfg/oled_contrast.
The administrator slider applies without reboot via an atomic request consumed
by the existing OLED worker. No web-handler I2C access or display reinitialization.
Zero is minimum contrast, not power off. Preserve address fallback and GPIO8/9.

0.2.32 fixes missing authorization-policy registration for /config/oled-dimmer
and /config/blue-hold. In 0.2.31 their forms/handlers existed but saving failed
with "Authorization policy mismatch" before session/CSRF validation. Register
both as protected POST mutation endpoints; retain all existing checks. The LAN
page test now checks every generated POST form against the actual auth policy.

From 0.2.33 the user explicitly requested color-only arrival/departure indications
because constant blinking was irritating. Remove all traffic on/off phase gating:
approach orange-to-cyan, departure blue-to-magenta with continuous fade to off by
30 seconds, missing/stalled predictions steady orange. Preserve blue hold and
steady dim amber, OLED settings, status/diagnostic sequences and power controls.
The compatibility blinkingLeds statistic is zero. Do not reintroduce traffic
blinking or add a blink configuration without a new user request.

0.2.34 adds saved show/hide plus brightness ceiling for unknown-timing orange
and uncertain/expired-hold amber. Default shown/no extra dimming (255 ceiling),
range 0-255; 1 is faint, zero dark. Always limit by normal map brightness and
physical caps. NVS unknown_timing packs enabled in bit 8 and brightness in the
low byte; both are saved atomically as one UInt. Applies through live snapshots
without reboot/resetting visits. Known motion and counts are unchanged.

User UI preference: simple enough for a 13-year-old to understand and navigate.
Use plain labels and short explanations, with key status first and advanced
diagnostics behind more details. A broader visual/navigation redesign was
discussed as a separate future change, not implemented in this controls release.

0.2.35 fixes home-page rendering failure: full signed-in production controls and
live route/station diagnostics exceeded the old 16 KiB static gPage buffer.
Use web::kStatusPageCapacity (24 KiB) as the shared firmware/test capacity.
Regression includes all 18 routes and eight station samples (16,956 bytes), plus
long escaped fields (17,797 bytes). The separate /update page remains available
for installing this fix while the old home page fails. No display-rule changes.

0.2.36 adds Last reported location as a third uncertain-train mode. Use faint
neutral white capped at 2/255 and existing brightness limits, fading for 1-300
seconds (default 60). Repeated timestamps at an unchanged uncertain location do
not reset the fade. Missing reports/position fields may retain rendering-only
hints; stale input cannot refresh them. New useful locations replace old ones,
conflicts/removals clear hints, and no extra train/occupancy counts are created.
Bounded location history is 256 trips, independent of blue-visit timers, and
is not persisted. New configuration defaults to Last reported; preserve saved
legacy Hide/Orange choices. The UInt unknown_timing now packs mode in bits 8-9
(0 hide, 1 orange, 2 last-reported), brightness in bits 0-7, and fade seconds
above bit 9; a zero fade field decodes as legacy default 60. Validate mode and
duration before saving/loading. White fades at very low brightness have steps.
Keep traffic blink-free and preserve OLED/power behavior.

0.2.37 redesigns the embedded main/sign-in UI for the user's 13-year-old clarity
goal. Navigation: Overview, Train lights, Screen, Wi-Fi, Advanced (explicitly
not More). Responsive cards, colored line badges, local assets, large controls,
live slider readouts, collapsed diagnostics, and session-only area memory after
normal saves. CSS/navigation are served read-only from flash at /ui.css and
/ui.js; bump the versioned URLs when changing assets. No external dependencies.
Preserve existing forms/actions, CSRF/auth, text-only live refresh, settings and
MTA/OLED/hardware behavior. Keep the 24 KiB shared page buffer; assets are not
embedded into it. See docs/WEB_UI.md and docs/RELEASE_0.2.37.md for maintenance
and browser-validation coverage.

0.2.38 adds saved four-state RGB colors. User defaults: approaching Yellow,
stopped Red, leaving Green, uncertain Blue. Palette applies to live station and
moving segment indications, including uncertainty/last-reported fading markers.
No traffic blinking or ETA color gradients. Fade durations, state classification,
brightness/current limits and counts are unchanged. NVS train_colors is one
12-byte TrainColors blob; validate size before loading and strict #RRGGBB form
values before saving. Route/status/diagnostic palettes remain separate. The web
legend reflects the selected palette; stopped-duration wording is color-neutral.
Assets version is 038. Build static RAM is 188,412 bytes, 80 bytes above 0.2.37.

Accepted follow-up work after the color release:
- Compare optional estimated motion against recorded feeds before enabling it
  on hardware. Use observed stop changes, bounded learned travel times and ETAs;
  reconcile fresh observations, suppress contradictory/stale/ambiguous estimates,
  and distinguish estimates from confirmed reports. Do not silently enable guesses.
- In a subsequent release, add OLED mapped-train count between Brooklyn Subway
  and the existing clock. Use the committed map's resolved-train count so fading
  location hints are not counted as extra trains. Preserve OLED GPIO8/9, address
  fallback, dimmer, worker isolation and time/DST behavior.
- The earlier geographic end-to-end ordering fix for the separate full-line
  diagnostic remains queued after MTA representation work.

Subway Now research (2026-11-14): its own sunny-ng/subwaynow-server README
documents 15-second production processing with async workers and rolling average
trip runtimes for ETAs (https://github.com/sunny-ng/subwaynow-server). Our current
poll scheduler waits 15 seconds AFTER eight sequential downloads and publishes
at cycle completion; this adds latency. Compare actual feed timestamps and cycle
durations before claiming which app gets newer observations. No polling or
estimation changes were included in 0.2.38.

## USB / programming

- Native ESP32-S3 USB is wired to USB1: GPIO19 D-, GPIO20 D+.
- USB1 is the manufactured 16-pin USB-C receptacle.
- BOOT = GPIO0 (SW14) and RESET/EN = SW11 provide recovery programming access.
- Maintain USB CDC support during bring-up so serial diagnostics remain available.

## Development targets

### Production target

`brooklyn-s3`

- ESP32-S3-WROOM-1U-N16R8
- 16 MB flash
- 8 MB OPI PSRAM
- Native USB CDC enabled

### Bench target

`xiao-s3`

- Seeed XIAO ESP32-S3 used only as a travel/toolchain test board
- 8 MB flash / 8 MB PSRAM
- It is **not** electrically equivalent to the manufactured Brooklyn PCB.
- Never infer Brooklyn PCB pin behavior from the XIAO.

## Firmware architecture

Keep hardware mapping separate from logical subway data. Planned modules:

1. `hardware` — safe GPIO startup, controls, buzzer, user LED, I2C, power status.
2. `led_engine` — 8 physical channels, brightness/current limiting, status LEDs, logical-to-physical mapping.
3. `controls` — encoder and six front buttons, debounce, long-press handling.
4. `network` — Wi-Fi station mode, setup AP, recovery, web configuration, OTA.
5. `mta_feed` — real-time MTA feed retrieval/parsing.
6. `subway_model` — logical routes/stations/segments independent of PCB LED designators.
7. `ui` — OLED and front-panel behavior.
8. `settings` — persistent configuration.

Do not combine MTA route semantics with physical LED numbering. The logical model must map to the physical LED map through a dedicated mapping layer.

## Bring-up safety rules

1. On production hardware, set GPIO1 (`LED_ENABLE`) LOW immediately and leave the level-shifter outputs disabled until all LED data pins are initialized LOW and the LED driver is ready.
2. Do not run full-white/high-brightness tests across all 306 LEDs during initial bring-up.
3. Verify 5 V and 3.3 V rails before enabling LEDs.
4. Test one LED channel at low brightness first, then all eight.
5. Keep native USB CDC operational until OTA and recovery paths are proven.
6. Do not erase or repurpose GPIO0/EN recovery behavior.
7. Treat all front buttons and rotary push as active LOW.
8. Preserve the exact manufactured PCB as the hardware authority.

## Pending 0.2.38 POWER controls

GPIO40 POWER short-release cycles both on, LED-off/OLED-on, LED-on/OLED-off, both off. Default both on; selections are volatile. GPIO1 startup/fault guards remain. U23 GPIO14 follows LED selection through a serialized gate; its once-per-boot beep is retained. OLED U22 GPIO8/9 uses power-save commands in its own worker (up to one-second response), without changing address or contrast. Three-second debounced hold requests reboot once; main loop performs it at a safe boundary after any active feed/update operation. Held-at-boot input is ignored until release. No PCB changes.

## 0.2.39 administrator sign-in navigation

All expired/missing administrator-session responses now redirect with HTTP 303 to the existing /login form instead of showing a plain-text sign-in-required error. This includes protected actions, diagnostics, update-page session-token expiry and denied package uploads. The package uploader recognizes its redirected login response and navigates the browser to /login. Login still obtains a fresh login CSRF token; authorization and mutation CSRF checks remain unchanged. Failed actions are not replayed after sign-in; the user returns to the map and retries. The selected settings area remains remembered by the existing UI. No hardware changes.

## 0.2.40 BRIGHT/DIM controls

SW10 GPIO42 and SW7 GPIO39 are sampled by the 20 ms live task on debounced release. They adjust only the sole selected display (POWER mode 1 OLED, mode 2 LEDs), ignoring both-on/all-off, boot-held buttons, opposing chords, mode changes and diagnostic/recovery input. Eight queued presses maximum; mode changes and diagnostics clear the queue. Main loop drains before/after synchronous network work, persists via existing configuration setters, and updates physical rendering. LED range 1�16 step 1; OLED contrast 0�255 step 16. U23 dim duty is unchanged. BUZZER3 GPIO21 emits a 45 ms 800�3200 Hz level cue on existing PWM channel 7, serialized with feed-ready audio. OLED U22 GPIO8/9 temporarily shows level for three one-second worker frames and may wake in LED-only mode, restoring power-save afterward; POWER changes clear feedback. Settings remain adjustable independently on the web. No new dependency or PCB modification.

## 0.2.41 web titles and dedication

Web pages use Brooklyn Subway as the browser title and primary heading, with Specially Designed for Avi Yao directly below as a subtitle. Applies to dashboard, sign-in, provisioning, update/restart, diagnostics and notice pages. Action-specific headings remain below the brand. The device name moves into Advanced / Device information, still HTML-escaped. Wi-Fi setup SSID/hostname contracts are unchanged.

The same 0.2.41 update also adds the previously queued OLED middle line Trains: N using the committed operational frame resolvedTrains count (same as web Mapped trains, excluding fading last-reported hints). Baselines 16/34/55 fit title/count/time on 128x64. Shows Trains: -- while no current usable frame is available, and Trains: 0 for a valid empty frame. Atomic handoff to the existing I2C worker; brightness feedback temporarily replaces the normal three lines, then restores them. Serial logs announce count changes.

## 0.2.42 sign-in return destination

Denied protected requests carry a fixed destination token in /login?next=... and in the login form. Successful sign-in returns to the corresponding GET page or settings tab, including firmware updates, diagnostics and Wi-Fi setup. Only explicit local destination tokens are accepted; URLs, actions and duplicate/embedded-null parameters are rejected. Original mutations and uploads are not replayed; retry after sign-in. Login CSRF and administrator checks remain intact, and inline legacy two-field sign-in forms still work. No global return target shared between clients.

## 0.2.43 skipped-stop journeys and route audit

Fresh trip predictions can reconcile STOPPED_AT across one unique forward static path of at most 12 edges, not just adjacent stops. Adjacent inference retains 90 s vehicle-age bound; skipped-stop inference allows the existing 300 s maximum with prediction/feed <=60 s old and ETA 1�600 s ahead. Raw timestamps stay unchanged. RtTrain carries an internal original departure endpoint and prediction-evidence timestamp; provider uses this endpoint rather than an unvisited local predecessor. First stop SKIPPED/NO_DATA is decoded and rejected; conflicting predictions remain rejected. Bounded elapsed/ETA progress can select existing segment pixels; otherwise next station uses approaching color. Motion freshness uses the prediction, departure trails retain raw age, and last-reported history follows a fresh inferred approach. No blink/brightness/pin changes.

Read-only tools: tools/compare_subwaynow.py and tools/route_snapshot_audit.cpp. Three eight-feed/web snapshots show one additional F and two additional R departures per snapshot, with unchanged mapped train counts 85/80/77 and no decoder/overflow/renderer failures. All 18 route categories reviewed; 15 had mapped samples, 14 had comparable website positions, S only identity evidence, 5/B/Z no mapped samples. Original F trip 053700_F..S now maps 7 Av -> Church Av as approaching. See docs/TRAIN_DATA_COMPARISON_2026-11-14.md for boundaries/reproduction. This is bounded prediction-based motion, not GPS or a guarantee of equality with Subway Now. Learned historical runtimes and unrestricted reroute interpolation are not implemented.

## 0.2.44 train monitoring and visible LED counts

Two additional complete all-18-route comparisons are recorded at 2026-11-14 11:15:09 and 11:28:34 New York time; an intervening numbered-feed timeout was repeated rather than counted as success. 5/B/Z have no mapped samples; S lacks reference position estimates in the second check. Details: docs/TRAIN_MONITORING_RECHECK_2026-11-14.md.

BlueReport now includes fixed per-route mapped/moving/estimated/uncertain counters plus source-header bounds, stale/unmapped/conflict/decode/capacity counts. Counts are deduplicated at pipeline finish; EstimatedDeparture uses a spare evidence flag. Live controller counts nonzero map colors after current limiting (status LEDs excluded), with zero reported when disabled. OLED U22/GPIO8/9 displays Trains:N Lit:M using an atomic visible-count update from the live task. Current fade/color/settings are preserved; observed preference is Last reported, 20 s fade, cap 2/255 (user ceiling 5), explaining sparse visible lights despite many mapped reports.

Read-only /monitor and /api/monitor expose the latest poll separately from the held display frame and actual live lit count, all 18 routes, received/partial/missing feed states, ages, fade policy, errors and a JSON download. Source group lookup uses kFeeds names; S requires ACE plus G for H/FS. Network reports all eight expected feeds even if budget skipped attempts. Home links from Overview and Advanced; extra status JSON fields visibleMapLeds/darkOccupiedMapLeds. No auth, mapping, brightness or output policy changes. New counters add bounded memory; existing page/JSON buffers are reused. Static report data and live output are separate measurements.

## 0.2.45 stopped-color qualification timer

Fixed early stopped-color countdown: StopVisit.firstSeen is now 0 until the first RecentMatchingArrival report, while lastSeen still tracks uncertain observations. The timer begins on first qualification, remains anchored across repeated/noisy reports, and resets only for a new stop or the existing visit-recovery/expiry rules. No added RAM. A regression reproduces first uncertain STOPPED_AT, qualification 19 seconds later, and a full 20-second window; repeated reports do not restart it and a later station gets a new window. Existing 30-minute history/capacity and clock rollback guards remain. The visible duration can still be shorter because of download time, held old frames or a later changed report; no artificial minimum dwell or weakened stop evidence. User currently runs Steady color (internal mode orange), blue uncertain at 5/255. At diagnosis the board reported DegradedHold, data age 36 seconds, and a 20-second stopped-color limit.


## 0.2.45 post-update observation (2026-11-14 evening, New York)

Read-only five-minute observation saved locally in captures/monitor045-live.jsonl: 12 distinct completed polls, all 8/8 feeds, no decode/capacity drops, no new transport/TLS failures or reset, stable internal heap about 110 KB and PSRAM about 7.84 MB. OLED frame writes report I2C code 0. Qualifying-stop diagnostic count varied 9-18; this is poll evidence, not a measurement of the current rendered red count. Recorded reset reason was panic before observation; uptime increased continuously, so this does not identify its cause or prove long-term stability.

Remaining opportunities: observed completed-poll spacing 23-33 s despite a configured 15 s post-download wait; frame reference to publication lag 8-18 s still consumes a 20 s stopped-color window. Separate first-visible hold timing from poll epoch without refreshing original source/evidence ages or renewing old visits. Partial candidates can also spend their hold window before being displayed. Synchronous full-cycle downloads blocked 5 of 12 status requests beyond a 12 s client timeout; following monitor reads succeeded. Consider a bounded network worker with synchronized snapshots, preserving mutation/OTA serialization, rather than reentrant web servicing during downloads. Add actual rendered per-state counts and durable crash evidence; current API cannot verify physical red dwell. Per-feed commits with individual freshness boundaries are a larger follow-up to avoid whole-map holds when one feed fails. No firmware/settings changes were made during this observation.


## 0.2.46 responsive feed worker and stopped publication clock

One 16 KiB-stack, priority-1 task on core 0 owns sequential HTTPS/decoder work, the PSRAM feed buffer and the collecting pipeline. PollHandoff uses release/acquire ownership transfer. Loop reads only cached completed counters/report while collecting; finishCycle, operational-frame commit, reporting and all web/config mutations remain loop-owned. A result is consumed once before another cycle can start. TLS root verification, per-feed deadline, overall budget, post-cycle wait and whole-map partial grace are unchanged. Worker creation failure retries after the normal interval without synchronous fallback. Worker startup/stack low-water diagnostics are serial-only.

finishCycle accepts publication time and whether the display controller will commit the candidate. StopVisit arms only for committed, fresh qualifying stops. TrainFrame.stoppedEpochUnix is independent of animationEpochUnix; movement ETA/age and source timestamps are not rebased. Each stop retains an absolute deadline from original vehicle <=90 s, prediction/feed <=60 s freshness; publication clamps red to that deadline. Repeated visits and clock rollback do not renew timers. Held partial candidates can be inspected but never arm a new visit. Current color/brightness/hold preferences are preserved.

Both status APIs expose fetchingFeeds; monitor and Overview explain downloading while showing the last completed result and live ages. OTA checks/installs and network diagnostics wait for worker release; authenticated raw package uploads return 503/Retry-After while fetching, with authentication checked first. Wi-Fi setup and DNS application are deferred between cycles. Existing immediate reboot remains available. Production static RAM 193468 B, +4392 B, plus the dynamically allocated worker stack/task. All 29 host suites and four builds pass; runtime TLS/web heap and ESP32 responsiveness need post-install observation. No hardware changes; no fixes claimed for the earlier unexplained panic. Independent feed commits and rendered-color counters remain follow-ups.


## 0.2.46 installed check (2026-11-15 morning, New York)

Read-only post-install sampling saved in captures/monitor046-live.jsonl, plus full-page tests in captures/pages046-live.json. About six minutes, 61 paired status/monitor samples, no HTTP errors; typical API latency below 0.2 s, worst observed about 4.2 s (versus repeated >12 s status timeouts on 0.2.45). Full Overview and monitor GETs succeeded while fetching. Uptime increased continuously, reset reason software, OTA confirmed, no OLED I2C errors, no decoder/capacity failures. Idle internal heap stayed near 86.7-87.3 KB; active TLS samples about 38-50 KB, lifetime minimum 21912 B, idle PSRAM returned to 7844347 B. No sustained idle-memory decline observed; this is a short check, not a long-term stability claim.

One distinct partial poll processed 6/8 feeds, with two generic secure-connect failures (last SI, 33 ms, zero body bytes; L missing in per-route status). It entered DegradedHold and recovered to 8/8 on the next cycle. Failures did not recur during the remaining observation. Cause is unconfirmed: generic connect/TLS counters do not distinguish network, certificate or allocation failure. Follow-up opportunity: capture TLS error code plus largest internal free block and worker stack watermark in diagnostics before assuming a network cause. Actual visible red duration still needs the user's visual confirmation; poll stop counts are not live rendered-color measurements. No firmware/settings changes during the check.


## 0.2.47 live indication observation

Observation-only RenderTrace supplies semantic kind and end reason beside unchanged RGB. A diagnostic per-LED bit identifies whether the existing stopped remaining time was capped by source freshness. Live controller counts nonzero map pixels after current limiting only after successful transmit, tracks continuous stopped-light starts with wrap-safe uptime subtraction, and retains 64 ending records in a bounded RAM ring. Records describe LED-level transitions, not individual-train dwell/departure. POWER, recovery, zero output, failed writes and hardware-test pauses are separate from station timing. No policy, GPIO, palette, brightness, polling or stopped-window change.

Read-only /api/indications returns 12 recent endings by default or paginated endings after a numeric sequence cursor, plus up to 12 active stops/full total. Sequence cursors expose overwritten history; collectors must detect reboot/uptime decrease and restart at zero. Existing status APIs include four small counts without copying history into every StatusSnapshot. Runtime mutex protects only copying, never HTTP serialization. Monitor panel refreshes every 2 s; existing full monitor refresh remains 5 s. History resets on restart and never writes NVS. tools/monitor_indications.py collects bounded JSONL, paginates and reports gaps/restarts. Source-expiry and configured-time expiry are distinct observed reasons; a changed indication is not automatically called a departure.

Validation: 30 host suites pass with affected diagnostic-double include rechecked, four builds pass, browser UI passes at four widths. Duration tests cover 280 ms and 20 s; ring/collector tests cover overwrite, cursor pagination, rollover and reboot. RenderTrace preserves RGB across 120 configurations. Production static RAM 196596 B (+3128 B). The 0.2.46 transient generic TLS failures remain unexplained; monitor runtime headroom after install. User asked why red ends while train remains stopped: current 20 s per-visit maximum can cause this despite continuing reports; extending red with fresh consistent evidence is only a proposed future policy, not part of 0.2.47.


## 0.2.47 first live history capture (2026-11-15 morning, New York)

After verifying installed 0.2.47, tools/monitor_indications.py collected 300 seconds into captures/indication047-installed/history.jsonl: 271 samples, 180 stopped-light endings, zero history gaps, restarts or collector errors. All 180 endings were StopWindowExpired (configured 20-second limit), lasting 19.164-20.103 s, median 19.841 s. No sub-second stopped indication occurred in this sample. Max age of successful-output sample was 53 ms. State counts ranged approaching 1-16, stopped 0-19, leaving 0-5, uncertain 30-58. These are LED-level indications, not independent proof of train departure or dwell.

Concurrent health sampling observed usual completed-poll spacing about 22 s (range 18-23 s), so the hard 20 s per-visit stop window typically expires before the next report can confirm or contradict the stop. The next recommended policy is to keep the stopped color while fresh consistent stop evidence supports it, switch to movement on verified departure, and become uncertain on stale/contradictory evidence, rather than force red off solely at total visit age 20 s. No policy/settings were changed during this observation; the earlier explicit 20 s preference should be revisited before changing it.

Health: no reset or OLED error observed, minimum internal heap 22060 B. One distinct 2/8 partial poll was held and then recovered to 8/8. Generic secure-connect failures reached 7 (one earlier plus six in that partial cycle), with no HTTP/transport timeout errors. Cause remains unconfirmed; TLS error code/largest contiguous internal block would help separate network from allocation pressure. Existing video was filmed on 0.2.46; no additional video is needed to establish the 20-second-cutoff behavior.


## 0.2.48 evidence-driven stopped indications

User explicitly approved replacing the total per-visit 20 s cutoff after the 180-event monitoring result. StopVisit now stores the last published support's absolute freshUntil, reusing the same struct/RAM. Initial red still requires matching arrival <=30 s old plus vehicle <=90 s, prediction/feed <=60 s and non-future timestamps. A same-trip/same-stop published confirmation may continue with an older arrival only before its existing support deadline and only while current matching, non-future vehicle/prediction/feed evidence is fresh. Raw timestamps always bound the new deadline; repeated old inner timestamps cannot extend it. Contradiction/missing prediction/future arrival/conflict/non-station outcomes invalidate support. New trip or station cannot borrow support; clock rollback fails closed. Verified trip-update departure remains unchanged and wins immediately.

No fixed visit-duration cap remains. The frame's stopped remaining time now exclusively represents source expiry, with source-limited diagnostic bit set. Publication delay still consumes original source lifetime; held partial candidates never establish/renew published continuation. Empty polls retain only the old bounded support (no renewal), and continuation cannot bootstrap across an expired gap from an old arrival. Strong new recent-arrival evidence can establish support again. Source freshness thresholds and whole-map partial grace are unchanged.

The stop-time form is replaced by a simple explanation; deprecated /config/blue-hold authenticates/CSRF-checks then returns 410 without mutation. Existing NVS blue_hold remains for rollback, and legacy API blueHoldSeconds is explicitly inactive via stoppedTimeLimitEnabled=false. APIs label stoppedPolicy=fresh-evidence. No pin/palette/OLED/poll/power/TLS changes or extra RAM (production static 196596 B). Tests include ~110 s continuous supported stop, frozen timestamps, contradictions, missing predictions, changed identities, unpublished candidates, expired gaps, delayed publication, rollback, duplicate conflicts and departure precedence. All 30 host suites pass with the status-buffer tests using production 6144 B capacity; four builds and four-width browser checks pass. Post-install collect at least five minutes using tools/monitor_indications.py and compare reason/duration distribution with 0.2.47.


## 0.2.48 live verification (2026-11-15, New York)

Verified installed 0.2.48 with stoppedPolicy=fresh-evidence and stoppedTimeLimitEnabled=false. A 300 s collector run in captures/indication048-installed/history.jsonl recorded 181 samples, 139 completed stopped indications, zero gaps/restarts/errors. No ending used StopWindowExpired. Durations were 22.298-198.107 s (median 48.283 s), versus the 0.2.47 sample's 180/180 fixed-limit endings around 20 s. Reasons: 104 no indication at that position in the new displayed frame, 17 became uncertain, 12 leaving replacements, 4 approaching replacements, 2 original-source freshness expiry. Do not call all 104 no-indication endings confirmed physical departures: this is a map-pixel transition, not per-trip dwell proof. Longer source-supported stops and stale-source fallback were both exercised.

Concurrent health samples remained Fresh with 8/8 feeds, no resets, OLED errors, decoder/capacity errors, TLS/HTTP/transport failures. Idle internal heap returned from about 83.8 KB to about 83.8 KB after temporary drops during heavier HTTP/TLS work; lifetime minimum 22028 B. No sustained idle decline observed in this short run. No settings or firmware changes during observation. No additional video needed for this policy verification; next longer-run concern remains occasional generic TLS failures seen on earlier builds, not reproduced in this sample.


## 0.2.49 connection/restart diagnostics and fresh-board AP fix

User requested connection diagnostics plus an overnight check, and independently programmed a second production PCB. COM5 serial 28:84:85:36:D0:7C remains original board 10.1.1.56; COM6 serial 28:84:85:34:1F:74 is the new board. Prepared frozen factory bundles under dist/0.2.48/factory and dist/0.2.49/factory; they verify hashes and never build the working tree. User flashed COM6 themselves. 0.2.48 booted/chimed but no AP: begin() queued recovery while state remained Disabled, then update() returned before handling it. 0.2.49 sets NoCredentials before queueing startup recovery. User confirmed BrooklynMap-Avi now appears on COM6. Never use the factory flasher on COM5; original uses signed web updates. New board needs its own LAN IP; fixed mDNS hostname can collide.

AttemptDiagnostics is a default optional FeedConnection observation (no protocol changes). HttpsFeedConnection resets the pinned framework's protected _lastError diagnostic before open; this prevents stale TLS errors when DNS fails before a handshake. Captures HTTP code and negative TLS code before explicit close, plus internal heap/largest block before attempt and before close; the library may have cleaned up internally already, so this is not an allocation-peak measurement. Worker owns an eight-item failure batch; release/acquire handoff copies every item into a loop-owned eight-record journal. Existing last-failure fields remain compatible. Runtime API exposes null when TLS detail is absent, not success. Serial prints compact codes/memory detail. Stack watermarks multiply the port StackType_t size; this ESP32-S3 port uses uint8_t stack elements.

Stable board ID is ESP.getEfuseMac formatted as 16 hex digits; original expected 00007cd036858428 (base MAC 28:84:85:36:D0:7C, SDK copies bytes into little-endian uint64). Random boot IDs identify restarts. RTC_NOINIT retained state (36 B) stores a checksum, ID, warm-session boot/unexpected counts and main-loop uptime/free/min-heap checkpoint, normally every 5 s. No NVS writes. Power-on/corruption resets scope; previous checkpoint can be older during blocking maintenance. This is not a panic backtrace. API /api/runtime-diagnostics and status board/boot IDs support long-running logs. Monitor has a connection/restart section; Advanced links to it. Production static RAM 197812 B (+1216 B). All 30 host suites/four builds/browser widths pass; local fake-HTTP soak smoke passed. Actual warm-reset retention remains to validate without inducing a crash.

Overnight collector tools/monitor_stability.py is read-only, bounded to <=24 h, pins a board ID, can wait for a scheduled timezone-aware start/version, emits capture.jsonl and rolling summary.json, distinguishes baseline history from new failures, detects boot IDs/history gaps/coverage gaps, samples idle heap and stack minima, and lists persistent indications for review (not automatic faults; ongoing examples are capped at 12 by API). Plan an eight-hour run tonight 22:00-06:00 New York, requiring >=0.2.49 and original device ID. Computer must stay awake/on LAN. User also explicitly requested AP/captive-portal UI redesign matching other pages; do it as subsequent 0.2.50 so the existing local 0.2.49 COM6 install remains version-consistent.


## 0.2.50 setup portal and OLED connection instructions

User requested the new captive-portal UI plus AP connection instructions on OLED and explicitly authorized testing 0.2.50 via COM6. Redesigned local-assets page matches Brooklyn Subway/Avi Yao UI, with map-password, Wi-Fi and per-board LAN-IP steps; Advanced reset, visible-password controls and manual SSID entry. Background /setup/networks JSON exposes all 24 networks without reloading forms. Native POST fields remain pick/pass; scans cannot interrupt connecting/saved states. Complete HTML/JSON must fit existing 6144-byte buffer or return an error; initial HTML has three network cards, JSON supplies the full list. No new dependencies.

OLED U22 SDA GPIO8/SCL GPIO9 keeps its proven driver and address behavior. Loop publishes atomic setup-mode flag while NetworkService is Provisioning; OLED worker replaces clock with Wi-Fi setup / Join BrooklynMap-Avi / Open in browser: / 192.168.4.1, returning to normal title/train count/time after AP ends. POWER and temporary brightness feedback retain precedence. Added setup/normal transition serial diagnostics, no I2C access outside worker. Host cases verify text bounds and return from setup. All 30 suites/four builds pass; browser checks at 320/390/768/1100 cover overflow, 24 escaped networks, form-scoped password reveal, typed fields surviving rescan, real native form submit, Connecting and Saved result-only pages. Production static RAM remains 197812 B.

Verified COM6 serial 28:84:85:34:1F:74 before freezing dist/0.2.50/factory and flashing; esptool verified hashes and reset board, heartbeats observed. COM5 untouched. Need operator visual confirmation of OLED and AP UI; USB capture started after initial boot messages. Original overnight observer remains scheduled for 22:00-06:00, PID38652, expected board 00007cd036858428 at 10.1.1.56, requires firmware >=0.2.49; this work does not imply original board upgraded or overnight capture completed.


## Master prototype designation (2026-11-15)

User explicitly designated COM5 as the master prototype. Reference device: production PCB serial 28:84:85:36:D0:7C, stable runtime ID 00007cd036858428, LAN 10.1.1.56. COM6 serial 28:84:85:34:1F:74 is the secondary/test board. Identify by serial/runtime ID rather than trusting a COM number or shared mDNS hostname alone. Master is now observed running 0.2.50 with Fresh 8/8 feeds and OLED writes OK at active 0x3C; user confirmed new setup UI/OLED instructions work. Master updates use the signed web updater; no factory reset or USB factory flash is implied by this designation. Preserve its settings as the reference baseline. User has a 5V/1A supply on one board; asked which supply currently powers COM5 so monitoring results can be interpreted correctly.

Read-only two-minute post-upgrade observation launched into captures/master-COM5-050. Overnight process 38652 remains alive and armed for 2026-11-15 22:00 through 2026-11-16 06:00 New York, pinned to the master runtime ID; its minimum firmware requirement is now satisfied. Overnight results are not yet available. Next review concerns are resets, HTTP/TLS failure codes, idle-heap recovery, OLED health and red-indication transitions; no train-rendering or brightness changes made during observation.

User confirmed COM5 master prototype uses the 5 V, 3 A supply during monitoring. The separately mentioned 5 V, 1 A supply is not powering the master for this run.


## Private 0.2.51 OLED/MODE test on secondary board

User approved OLED screens/MODE navigation development on COM6 while COM5 master remains unchanged for overnight stability. New oled_screens.h/.cpp define Main/Activity/Connection/Device pages and bounded five-line text. Activity counts are visible map indications, not distinct trains. Main is unchanged title/trains/lit/clock. Connection shows Wi-Fi RSSI, last cycle successes out of all eight, display health and wrap-safe last usable age. Device shows version, individual LAN IP and uptime. Main loop publishes a small snapshot once per second under a short copy-only critical section; OLED worker retains exclusive I2C ownership. Selection is atomic and volatile (Main after reboot). No new NVS keys/dependencies.

MODE GPIO41 is sampled by the 20 ms live worker using existing debounced-release/boot-held guard. Disabled during hardware diagnostics/recovery; setup instructions cannot be dismissed. Brightness feedback and POWER screen-off retain priority. Existing controls-loop MODE sampling is skipped when live worker owns it. MODE LED307, channel 8 index 3 (data GPIO13), follows screen off/cyan/green/purple under existing brightness/current caps. Existing logical white provisioning indication is retained, and recovery still shuts physical LED output down. Screen change marks the status overlay dirty without altering map pixels. StatusIndicators has an explicit default-compatible constructor for the project's C++11 target.

All 30 suites and four builds pass; OLED runtime covers pages, bounds, absent data, age rollover, setup/POWER priority; status/live tests cover mode colors/map preservation/recovery shutdown. Static production RAM 197900 B (+88). No train-rendering, polling, palette, ROUTE/rotary action or TEST sequence changes. Frozen dist/0.2.51/factory bundle flashed only to serial-verified COM6 (28:84:85:34:1F:74); hashes verified. Serial confirms OLED at 0x3C, first frame, 8/8 data, live output and firmware health confirmation. User needs to press MODE four times to verify physical appearance. Build remains private (no GitHub release/tag), so COM5 remains 0.2.50. Read-only master check confirmed 0.2.50 Fresh 8/8 after COM6 upload. See docs/OLED_MODE_TEST_0.2.51.md.


## Private 0.2.52 rotary screens / line browsing

User approved SSID, a subway-line screen and refinements on COM6, then clarified the intended control: turn to browse screens; on Lines, press and then turn to browse individual lines; press again to return to screen browsing. This replaces the earlier proposed short-push-next-screen behavior. MODE remains an alternate next-screen shortcut, ROUTE opens line browsing and toggles Auto/Paused when on Lines. Turning pauses Auto. Auto advances every 5 s, wraps all 18 canonical lines, and does not reset inactivity. Selection is remembered across screens but resets at boot. No map filtering is performed. Navigation is a pure typed state machine, guarded for rollover/held controls/setup/POWER/diagnostics; the 10-second rotary recovery stays intact and a >=1 s hold is never a short selection.

Existing rotary GPIO15/16/17 and ROUTE38 now sampled by live task every 5 ms; debounced release and Gray-code full-cycle rotation cancel bounce and invalid jumps. Controls loop skips duplicate live-owned input. Screen count is five, Lines uses amber MODE LED307 ch8 index3/data13; main title includes 1/5. Connection shows bounded SSID with slow ping-pong scrolling (one character/second and 2 s endpoint pauses), clear connecting/waiting/old/missing-feed text. Unsupported OLED characters use question marks. Committed map-frame occupancies supply per-line mapped trains; successful physical output supplies per-line visible LED counts, including off/hidden behavior and shared-train distinction. Saturation is labeled >=. Never read the worker's mutable pipeline to populate OLED lines.

Screen web section adds persistent idle return choices 0/30/60/120/300 seconds (Stay, 30 s, default 1 min, 2 min, 5 min). New bkmap_cfg/oled_return callbacks validate loads and preserve values on failed save. /config/oled-return is explicitly registered, mutation/CSRF protected, maps sign-in return to Screen and returns /#screen on success. Setup and POWER/diagnostic/held controls suspend timers; auto line advancement does not count as user activity. All prior brightness/power/clock/setup priorities remain.

All 31 host suites/four builds and four-width browser QA pass. Shared/saturated line counts, encoder direction/bounce, long/boot-held press, five-screen context, line memory/wrap, Auto/Pause, idle/Stay/rollover, scroll pauses, auth/settings persistence/failure and bounded screen text covered. Largest escaped full web status 24196/24576 bytes; no buffer increase. Static production RAM 198228 B (+328 over private 0.2.51). No dependency, pin assignment, rendering/palette/polling/TEST sequence change. Queued entire-line diagnostic ordering fix still outstanding.

Uploaded frozen dist/0.2.52/factory only to verified COM6 serial 28:84:85:34:1F:74. Hashes verified; startup OLED ACK/first frame at 0x3C, 8/8 feeds, live output and OTA health confirmation observed. COM5 remains read-only master, confirmed 0.2.50 Fresh 8/8 with no TLS/transport errors afterward. 0.2.52 is private, no GitHub stable release/tag, preserving the master's overnight test. Operator physical rotary direction/detent/visual screen checks remain. See docs/OLED_ROTARY_TEST_0.2.52.md.


## Private 0.2.53 knob feedback and response; MODE reserved

User reported needing two notches for a screen/line change and requested a beep per turn. Four-transition decoder and one-second OLED redraw were both contributors worth addressing. Changed to two valid Gray-code transitions per event, supporting either resting phase with bounce reversal cancellation/invalid-jump reset. OLED now checks revisions every 20 ms and redraws after navigation/data/power/feedback changes, but idle redraw stays 1 Hz; physical response also includes the existing 100 kHz framebuffer transfer. Brightness feedback changed from three frames to three real seconds so faster navigation does not shorten it. No I2C outside the OLED worker and no unsafe notifications to a potentially deleted worker task.

Existing BUZZER3 GPIO21 / LEDC channel 7 timer3 gives a nominal 15 ms 2000 Hz click plus 15 ms gap per accepted rotary event. Eight-item bounded pending count, try-lock service and no delay calls prevent click duration from blocking sampling. Priority brightness/feed-ready cues cancel active clicks under the shared indicator lock; diagnostics clear click state before their cue. Diagnostic/recovery cancel pending sound. U23 allowed-state updates are published atomically and use a try-lock/retry so brightness/feed-ready audio cannot make live input spin on their lock. Main-loop indicator updates consult the latest allowed value. Separate U23 timer/drive unchanged.

User explicitly reserved MODE for another future use. Removed its alternate navigation binding and live-owned sampling; generic controls serial logging remains. MODE LED307 still tracks OLED screen. Rotary turn screens; press on Lines then turn lines; press back; ROUTE shortcut/Auto/Pause; 10-second rotary setup preserved. Do not reassign MODE without a subsequent requested function.

All 31 suites/four builds pass, including half-step events in both phases, early screen/line redraw, idle 1 Hz, 3 s brightness, async click/gap/queue/rollover/cancel/PWM-failure/priority and all prior startup/feed/recovery tests. Static production RAM 198244 B (+16 from 0.2.52). No new dependency, pin assignment, rendering/palette/polling/web change. Verified serial COM6 28:84:85:34:1F:74, flashed frozen 0.2.53 with verified hashes; startup OLED 0x3C, 8/8 feeds, live output and an operator rotary screen event captured. Physical notch-to-step/beep feel awaits operator confirmation. COM5 read-only check remains 0.2.50 Fresh 8/8; keep 0.2.53 private for master's overnight stability test. See docs/OLED_ROTARY_TEST_0.2.53.md.


## Private 0.2.54 dedication/uptime; COM6 upload recovery pending

User confirmed much improved rotary response on 0.2.53. Requested explicit uptime plus a dedication screen: Happy Bar Mitzvah! / Avi Yao / From the Goldweins / September 26, 2026, scrolling Joel, Marlene, Marc, Kim, Juliana, Levi, Robbie, Eric. Added Dedication as sixth screen after Lines (one backward turn from Main), pink MODE indicator with existing caps; MODE button remains reserved. All counters six, generic screen count for navigation. Main remains boot default; existing idle return applies and all names are visible within one minute. Bounded generic scroll helper handles the full names while preserving separate 32-byte SSID bound.

Device now says Uptime with days/hours/minutes/seconds, sourced from 64-bit esp_timer_get_time on main loop snapshots; millis only computes bounded snapshot age, so uptime survives its ~49-day wrap. No new dependency, NVS key, pin assignment, audio, map policy/polling or web change. All 31 suites/four builds pass, including exact text/date, all eight complete names visible, six-screen wrap/context, real font bounds and 64-bit uptime/wrap/large values. Production static RAM report remains 198244 B. See docs/OLED_DEDICATION_TEST_0.2.54.md.

IMPORTANT: frozen dist/0.2.54/factory prepared, but initial COM6 flash at 460800 lost USB at 33% through app. Bootloader/partition/OTA selector hashes had verified; app is partial/unverified. 115200 retry also got no serial data. COM6 still enumerates serial 28:84:85:34:1F:74. Asked user to hold BOOT, tap/release RESET, release BOOT and say ready. Wait for that physical action, re-identify COM port/serial, then retry full frozen bundle at 115200 and capture startup. Do not report 0.2.54 installed until verified. COM5 master remains untouched 0.2.50 Fresh 8/8, confirmed after failure. Keep build private for master overnight baseline. Suggested future additions (not implemented): quiet hours for OLED/beeps and optional short startup dedication.


## 0.2.54 COM6 upload recovered (supersedes pending recovery note)

User said try again after the manual BOOT/RESET prompt. Re-identified COM6 serial 28:84:85:34:1F:74; full frozen 0.2.54 upload at 115200 completed with ALL hashes verified and hard reset. The partial app has been completely rewritten; 0.2.54 is installed. Serial observed time synchronization, OTA image health confirmation and live LED output resumed (positive successful frames). Physical dedication scrolling/uptime appearance still awaits operator confirmation. See captures/flash-054-recovery.txt, startup-054-recovered-COM6.txt and startup-054-feedcheck-COM6.txt. COM5 untouched and still 0.2.50; one read-only sample during recovery showed DegradedHold 7/8, so do not claim uninterrupted 8/8 operation. Additional status/runtime snapshot saved to captures/master-during-054-recovery.json for stability review. Private build, no public release. Suggested quiet hours and optional brief startup dedication remain proposals only.


## Private 0.2.55: centered Avi, clock frame, startup greeting and quiet options

User requested Happy Bar Mitsvah! / centered Avi (remove Yao on dedication only), then a box frame around Main and continuation of suggested optional startup dedication/quiet hours. Added Main outline, retained date/Goldweins/all eight names, and nonblocking default-ON five-second startup dedication with skip on rotary turn. Setup/POWER override; MODE remains reserved. Screen > Quiet hours and startup greeting links to protected /screen-options; strict POST /config/screen-options has CSRF and fixed sign-in return routing.

User first chose quiet map Off, then explicitly requested configurable Off / Dim to a level / Do nothing. New validated 64-bit bkmap_cfg/screen_opts stores flags/schedule/OLED cap/map ceiling in one write. Defaults quiet OFF, start 22:00 / end 07:00 NY/DST, OLED cap1, map Off. Dim accepts ceiling1..16, normal encoded255. Invalid loads/defaults and failed-save preservation tested, including high bits. Runtime preference copy uses short critical section (no 64-bit atomic library dependency). Time unknown + enabled quiet fails quiet until sync. Saved normal brightness never changed.

Quiet mutes all existing beep sources. Live task refreshes policy even during diagnostics. Map Off unarms GPIO1, clears indication lights and U23; Dim caps addressable map/status and U23, Normal retains normal levels. Planned quiet restore respects POWER, faults and fresh-data enable gates; explicit manual LED diagnostics remain available and restore their prior intent correctly afterward. Added QuietHours indication reason without renumbering old reasons. U23 initial flashes suppressed for quiet Off/Dim. Existing GPIOs only: OLED8/9, buzzer21, U2314, LED_ENABLE1, LED307/ch8idx3/data13.

All 31 suites/four builds and browser four-width tests pass, covering windows/DST/unknown-clock, persistence/failure/64-bit map fields, startup/skip/POWER, frame/text, muted cues, U23 dim, live Off/Dim/Normal restoration, manual-off/fault/freshness guards and signed-in native form. Largest escaped home page24336/24576, unchanged capacity. Production static RAM198316 B (+72 from0.2.54). No new dependency, feed/palette/train-policy change. Frozen055 flashed only to verified COM6 serial28:84:85:34:1F:74 at115200; hashes verified, startup dedication/first OLED frame/health confirmation/8-of-8 feeds/live output observed. COM5 remains050 (read-only Fresh8/8), public latest remains050. Physical new display/quiet choices still await operator confirmation. See docs/OLED_QUIET_HOURS_TEST_0.2.55.md.

User asked to unplug COM6 briefly to upgrade the OLED display case. Upload is complete; advised disconnect BOTH USB and barrel power before handling OLED/wiring and leave COM5 master powered. Do not assume COM6 connected or start another flash until user reconnects and device identity is checked.


## Private 0.2.56: startup dedication until Wi-Fi and centered static lines

User requested holding the startup greeting until Wi-Fi connects, confirmed COM6 ready after case work, and asked to center all dedication items except the last scrolling line. First four rows now center by actual font width; final names row stays left aligned. Current Happy Bar Mitsvah! / Avi wording, Goldweins/date/names unchanged. Main frame, responsive rotary/audio, quiet options and MODE reserved remain.

Added Keep it until Wi-Fi connects checkbox, default on, alongside Show dedication. Greeting has five-second minimum, then waits for actual WL_CONNECTED + nonzero local IP snapshot. Does not wait for NTP/MTA. Normal idle return cannot end waiting startup; manual turn/setup/POWER override it. Later disconnection never re-enters startup greeting. Startup-wait bool bit40 and schema marker bit41 extend validated 64-bit screen_opts; legacy055 quiet preferences preserved and default to waiting. An explicitly saved brief mode has schema marker with wait false, so reload keeps brief. Migration does not write NVS automatically. Strict POST includes checkbox duplicate/value/null validation and all prior auth/CSRF/return routing.

All 31 suites/four builds/browser four widths pass; early/late Wi-Fi, minimum5s, >idle wait, one-shot, manual/setup/POWER, centered render coordinates, legacy migration/brief persistence, native checked form covered. RAM198316 B unchanged; home largest24336/24576. Frozen056 flashed only to verified COM6 serial28:84:85:34:1F:74 at115200; hashes verified. OLED ACK/first frame3C, greeting wait and Main transition plus health confirmation observed. Initial G-feed timeout recorded HTTP-11/TLS0/bytes0/9387ms, so do not claim 8/8 startup. COM5 remains050, read-only Fresh8/8 after upload. Private build, no public release. Physical centered-page confirmation still pending. See docs/OLED_STARTUP_WIFI_TEST_0.2.56.md.


## Private 0.2.57: complete-line RGB diagnostic on COM6

Continued queued request to test each subway line in its entirety. Prior route routine grouped logical lines but walked PCB global wiring order, causing a spatially fragmented appearance. Default route test now lights all LEDs of one logical line simultaneously red/green/blue at4/255, clears/disarms, then next line. No invented geographic order or mapping changes. Existing individual RGB walk retained separately as routes-walk; all-channel numbered-beep test unchanged. Physical TEST fast125ms/color+75ms gap (~10s incl output); web normal750ms/color+250ms gap (~50s). All302 semantic LEDs/18 routes, four status LEDs excluded. Whole frames checked against2A software budget; POWER/recovery polled every20ms during waits plus transfer time. GPIO1 LOW/black cleanup retained.

OLED worker-owned overlay identifies line/color/count, clears brightness feedback on test start and clears overlay on every test exit via guard. Setup/POWER keep priority, no I2C from diagnostic caller. Existing pins LED_ENABLE1/data4,5,6,7,10,11,12,13/OLED8,9 only; no dependency, NVS, train policy, palette, polling or MODE assignment change. MODE reserved. All31 suites/four builds pass including complete-route frames, all302 once per color, ordering, speeds, status exclusion, cancellation/cleanup, old walks and OLED label/setup/POWER behavior. Static RAM198324B (+8).

Frozen057 verified/flashed ONLY COM6 serial28:84:85:34:1F:74 at115200, all hashes verified. OLED3C ACK/first frame, startup greeting/Main, image confirmation, feeds8/8 and live frames observed. No test triggered remotely; operator physical observation pending. COM5 untouched050; read-only sample DegradedHold6/8 with last SI connect/TLS failure saved captures/master-after-057.json, overnight observerPID38652 still waiting/running. No public release/tag/push. See docs/WHOLE_LINE_TEST_0.2.57.md. Preserve unrelated simulator/replay/RESUME edits.
