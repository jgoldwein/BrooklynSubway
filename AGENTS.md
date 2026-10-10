# AGENTS.md — Brooklyn Active Subway Map Firmware

Read `PROJECT_CONTEXT.md` before changing firmware.

## Non-negotiable hardware rules

- The manufactured hardware authority is `Brooklyn_Active_Subway_Map_V1.1.eprj2`; firmware work must not change that board file.
- Production MCU is ESP32-S3-WROOM-1U-N16R8 (16 MB flash, 8 MB OPI PSRAM).
- Use pin assignments from `include/board_pins.h`; do not substitute pins without explicit hardware evidence and user approval.
- GPIO1 `LED_ENABLE` is active HIGH through Q1. Default LOW at startup and during failure/recovery paths.
- Eight LED channels use GPIO 4,5,6,7,10,11,12,13 with counts 37,38,37,37,42,37,38,40.
- GPIO14 drives the separate discrete Avi-location indicator U23 on `USER_LED`; active HIGH. It is not an addressable LED.
- Native USB is GPIO19/20. Preserve USB CDC during bring-up.
- Rotary: GPIO15 A, GPIO16 B, GPIO17 push. Front buttons: TEST 47, ROUTE 38, DIM 39, POWER 40, MODE 41, BRIGHT 42. All switch inputs are active LOW.
- 10-second rotary-push hold is the manufactured Wi-Fi recovery contract.
- Setup AP contract: `BrooklynMap-Avi` at `192.168.4.1`.
- The XIAO ESP32-S3 environment is a bench/toolchain target only. Never infer production wiring from it.

## Engineering workflow

- Make small, reviewable changes.
- Keep logical subway route/station/segment identities separate from PCB LED designators and chain indexes.
- Prefer explicit constants, typed enums/structs, and deterministic mappings over magic numbers.
- Add a serial diagnostic for every new hardware subsystem during bring-up.
- Never enable all LEDs at high brightness by default.
- Do not add a new dependency unless its purpose and version are documented in `platformio.ini` or `README.md`.
- For hardware-affecting changes, state the affected GPIO/net/designator before editing code.
- If hardware information is uncertain, stop and ask rather than guessing.
