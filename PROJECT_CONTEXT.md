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

Do not hard-code an OLED controller or resolution until the actual display module is confirmed.

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
