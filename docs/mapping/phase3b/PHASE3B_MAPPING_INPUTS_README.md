# Phase 3B semantic-mapping inputs

These files were extracted **read-only** from the exact manufactured EasyEDA Pro project:

- `Brooklyn_Active_Subway_Map_V1.1.eprj2`
- SHA-256: `3ecb90c15d79f027a949c047f9c0a6e2f544706750498f9c7cb94209d61b4bc9`
- PCB document UUID: `74298b6af49b4919b9aa371879fcf505`

No manufacturing data was modified.

## Files

- `led_positions_v11.csv` — exact PCB X/Y/angle for all 306 LED designators.
- `phase3b_semantic_mapping_worksheet.csv` — physical chain order + exact coordinates + blank semantic columns for audited route/station/segment assignment. The four CH8 status LEDs are pre-marked as non-map status elements.
- `pcb_led_designator_overlay.png` — exact manufactured PCB rendering overlaid with LED designator numbers.
- `region_*.png` — six enlarged regional crops for semantic mapping review.

## Semantic assignment rule

Do not guess. For each map LED, populate semantic fields only when route/station/segment identity is supported by the manufactured artwork or another explicitly approved source. Ambiguous items remain unassigned.

Important columns in the worksheet:

- `channel`, `channel_index`, `global_physical_order`, `designator` — immutable physical identity.
- `x`, `y`, `angle` — exact PCB placement coordinates from V1.1.
- `route` — route identity (e.g. B, Q, 2, 5).
- `element_type` — `STATION`, `SEGMENT`, or `STATUS`.
- `station_name` — for station elements.
- `from_station`, `to_station` — for segment elements.
- `direction` — only when supported/needed.
- `semantic_status` — e.g. `VERIFIED`, `AMBIGUOUS`, `UNASSIGNED`.
- `confidence` — use explicit values rather than implicit assumptions.
- `source_note` — traceable source for each assignment.

Shared stations must preserve route-specific physical LEDs. Do not collapse multiple route LEDs into one logical station LED.
