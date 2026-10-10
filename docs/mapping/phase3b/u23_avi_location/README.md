# Phase 3A.1 correction — Avi location LED is U23

This correction is based on the exact manufactured hardware baseline `Brooklyn_Active_Subway_Map_V1.1.eprj2` and the manufactured schematic.

## Verified identity

Avi's location indicator is **U23**, not one of the addressable `LEDxxx` devices.

- Designator: `U23`
- Schematic net: `USER_LED`
- MCU control: `GPIO14`
- Drive polarity: **HIGH = on**
- Series resistor: `R60`, 1 kΩ
- Schematic LED part marking: `SZY0402Y`
- Return: GND
- Type: separate discrete yellow indicator LED
- Member of 306 addressable LEDs: **No**

The manufactured PCB rendering places U23 immediately between/adjacent to the nearby addressable LEDs **LED291** and **LED188**, which is why the original addressable-LED-only designator overlay did not label it.

## Correct LED accounting

The earlier Phase 3A accounting remains correct:

- **306 addressable RGB LEDs total**
- **4** of those are front-panel CH8 status LEDs
- **302** addressable map LEDs remain available for subway station/segment semantics
- **U23 is additional** and is outside the 306-addressable-LED topology

Therefore:

- Do **not** reclassify LED291 as Avi's indicator.
- Do **not** reduce the logical addressable subway-map count from 302 to 301.
- Treat U23 as a separate special-purpose indicator controlled directly by GPIO14.

## Included files

- `pcb_led_designator_overlay_with_U23.png` — original addressable-LED overlay plus a U23 callout.
- `avi_location_U23_zoom.png` — enlarged local view around U23 / LED291 / LED188.
- `schematic_U23_USER_LED.png` — manufactured schematic excerpt showing USER_LED → R60 → U23 → GND.
- `special_indicators_v11.csv` — special-indicator inventory including U23 and the four CH8 status LEDs.
- `PHASE3A1_U23_AVI_LOCATION_CODEX_PROMPT.md` — instructions for updating the firmware documentation/semantic naming without changing physical behavior.

No EasyEDA/manufacturing source was modified.
