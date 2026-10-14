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

## Phase 3B semantic completion

**Phase 3B semantic identification is complete: 302 / 302 map LEDs have
semantic assignments.** The authoritative worksheet contains **276 STATION**
rows and **26 SEGMENT** rows, with **0 unresolved map LEDs**. Four additional
addressable LEDs are status indicators, not map LEDs; U23 is a separate
discrete indicator and is outside the addressable-chain semantic count. The
append-only station registry contains **188 entities**.

The final 13 assignments were human-confirmed from the CPL-derived final V1.1
artwork. Existing station entities were reused where appropriate; four
genuinely missing station entities were appended as IDs 185-188. Segment LEDs
use endpoint station IDs and do not create station entities. No GTFS IDs were
invented.

The complete results and integrity checks are in the
[Phase 3B completion and audit report](analysis/PHASE3B_COMPLETION_AUDIT.md).
The authoritative worksheet, station registry, full current global review,
and projected-LED cache are the current semantic sources. The active
[remaining-LED candidate matrix](analysis/remaining_led_candidate_matrix.csv)
and [human-review queue](manual_review/HUMAN_REVIEW_QUEUE.md) have zero
unresolved map LEDs. Earlier candidate analysis and review crops are retained
as explicitly resolved historical evidence in `manual_review/`.

LED299 is HUMAN_CONFIRMED as the L segment Broadway Junction -> Atlantic Av
(registry endpoint IDs 83 -> 50). This supersedes its earlier human-confirmed
L / Bushwick Av-Aberdeen St station interpretation; LED89 remains the station
assignment.

The accepted projection and artwork registration were not refit or altered.
Historical overrides remain authoritative: LED96 is G / Lorimer St rather
than the historical Metropolitan Av; LED308/309 retain the human-confirmed
D/B bridge-route reversal; LED291 is the human-confirmed F segment rather than
historical D; LED304 remains Botanic Garden -> Park Pl; LED285 is B / Grand
St -> DeKalb Av; LED286 is D / Grand St -> Atlantic Av-Barclays Ctr; and LED287
is A / Hoyt-Schermerhorn Sts -> Nostrand Av.
