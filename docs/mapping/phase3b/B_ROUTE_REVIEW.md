# B-Route / Brooklyn Corridor Semantic Review

**Status: human-reviewed semantic promotion.** The six prior B assignments
remain unchanged. The 7 Av, Atlantic Av-Barclays Ctr, and DeKalb Av B/Q station
pairs are `VERIFIED` in the master worksheet. Other corridor candidates remain
review-stage only; firmware is unchanged.

## Sources and method

This review uses only the Phase 3B manufactured V1.1 materials:

- `regions/region_2_2.png` and `regions/region_1_3.png` for corridor artwork,
  station labels, lane continuity, and route convergence.
- `pcb_led_designator_overlay.png` for designator locations.
- `led_positions_v11.csv` and the physical columns of
  `phase3b_semantic_mapping_worksheet.csv` for immutable channel/index/global
  references and PCB coordinates.
- `q_route_review.csv` for existing verified Q/B paired-lane relationships.

Electrical chain order and LED numbering were not used to infer semantics.
Physical identities are copied unchanged from the worksheet. Where the artwork
does not separate route lanes or distinguish station markers from train-in-
transit positions, the row remains ambiguous.

## Candidate counts

There are **19 B-corridor candidate records examined**:

- **ALREADY_VERIFIED:** 6 existing B anchors.
- **VERIFIED:** 6 additional B/Q station records (LED230, LED50, LED160,
  LED163, LED189, LED190).
- **PROPOSED stations:** 0.
- **PROPOSED segments:** 0.
- **AMBIGUOUS:** 7.
- **UNRESOLVED:** 0.

### Existing verified B anchors

These six are represented only to provide corridor continuity and are not
changed by this review:

- LED208 Brighton Beach
- LED207 Sheepshead Bay
- LED233 Kings Hwy
- LED232 Newkirk Plaza
- LED231 Church Av
- LED185 Prospect Park

### B corridor, Brighton Beach northward

In route progression order:

1. LED208 — Brighton Beach (`ALREADY_VERIFIED`)
2. LED207 — Sheepshead Bay (`ALREADY_VERIFIED`)
3. LED233 — Kings Hwy (`ALREADY_VERIFIED`)
4. LED232 — Newkirk Plaza (`ALREADY_VERIFIED`)
5. LED231 — Church Av (`ALREADY_VERIFIED`)
6. LED185 — Prospect Park (`ALREADY_VERIFIED`)
7. LED230 — 7 Av station (`VERIFIED`).
8. LED160 — Atlantic Av-Barclays Ctr station (`VERIFIED`).
9. LED159 — likely B-side corridor marker (`AMBIGUOUS`, confidence 0.70;
   station-versus-segment unresolved).
10. LED136 — likely B-side corridor marker (`AMBIGUOUS`, confidence 0.65;
    role unresolved).
11. LED189 — DeKalb Av station (`VERIFIED`).

LED290, at (4815.0, -2900.0), remains `AMBIGUOUS`: it lies in the
Prospect Park–7 Av corridor and appears closer to the extrapolated Q side, but
its route and segment identity are not sufficiently established. It is not a B
segment proposal.

The ordered corridor list does not imply a physical chain order. The CSV
retains each item's actual channel, channel index, and global physical order.

## Shared-station groups and ambiguity

- **Prospect Park:** LED185 is already verified B; LED184 is the separately
  verified Q counterpart. Keep the physical LEDs distinct.
- **Church Av:** LED231 is already verified B; LED53 is the Q counterpart.
- **Newkirk Plaza:** LED232 is already verified B; LED56 is the Q counterpart.
- **Kings Hwy:** LED233 is already verified B; LED60 is the Q counterpart.
- **Sheepshead Bay:** LED207 is already verified B; LED114 is the Q
  counterpart.
- **Brighton Beach:** LED208 is already verified B; LED115 is the Q
  counterpart. Brighton Beach is the B terminal on this map.
- **7 Av:** LED230 = B and LED50 = Q are verified route-specific station LEDs.
- **Atlantic Av-Barclays Ctr:** LED160 = B and LED163 = Q are verified station
  LEDs. Their signed offset is approximately (+85,+75), consistent with the
  established B-minus-Q orientation from Prospect Park and 7 Av.
  LED159/LED158 are an ambiguous likely B/Q pair (0.70), with
  station-versus-segment role unresolved. LED136/LED157 are an ambiguous
  likely B/Q pair (0.65), with their corridor role unresolved. LED161 is not a
  B/Q candidate under the current geometry review.
- **DeKalb Av:** LED189 = B and LED190 = Q are verified station LEDs. Their
  signed offset is approximately (+85,+80), consistent with the established
  B-minus-Q orientation and continuous northward corridor.
  LED85 and LED191 remain outside that verified pair; LED191 is not assigned
  B/Q semantics.

## Human confirmation required

1. Decide whether LED290 is a Q-side corridor/segment LED and identify its
   endpoints, if applicable.
2. Determine whether LED159/LED158 and LED136/LED157 are actual route-specific
   B/Q positions and whether they represent stations or segments.
3. Do not fold LED191 or LED85 into the verified DeKalb pair without evidence.
4. Preserve the distinct B/Q LED identities at every shared station.

No B assignment is proposed north of DeKalb Av in this review. Brighton Beach
remains the B terminal at the southern end; no B LEDs are assigned to Ocean
Pkwy, W 8 St-NY Aquarium, or Coney Island-Stillwell Av.

## Exclusions and accounting

U23 is the separate Avi-location indicator on GPIO14 and is outside the 306
addressable LED topology. LED291 remains an ordinary addressable map LED.
The addressable accounting remains 306 total: four CH8 status LEDs and 302
subway-map LEDs.
