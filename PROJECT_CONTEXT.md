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
