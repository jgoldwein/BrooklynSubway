# Q-Route / Brighton Corridor Semantic Review

**Status: human-reviewed semantic promotion.** The verified assignments below
are recorded in `phase3b_semantic_mapping_worksheet.csv`. Firmware and
authoritative manufacturing sources are unchanged. The three Coney Island
terminal LEDs without established route identities remain unassigned.

## Sources and limits

The candidate review uses only the manufactured V1.1 mapping materials in this
directory:

- `regions/region_2_2.png` and related regional artwork for route, station, and
  physical-lane appearance.
- `pcb_led_designator_overlay.png` for physical LED designators.
- `led_positions_v11.csv` for exact board coordinates.
- `phase3b_semantic_mapping_worksheet.csv` for channel, channel index, global
  physical order, and physical role.
- `u23_avi_location/` for the verified separate-indicator correction.

The human-reviewed manufactured geometry conclusions identify route-specific
parallel lanes at shared stations, the Q-lane segments, and the continuous Q
geometry south of Brighton Beach. The north-corridor review also verified the
7 Av, Atlantic Av-Barclays Ctr, and DeKalb Av B/Q station pairs. Promoted rows
have confidence `1.00` and status `VERIFIED`; remaining corridor candidates
are ambiguous. Electrical chain order is used only to preserve physical
identity, never to infer semantics.

## Candidate counts

The CSV contains **40 addressable candidate LEDs** for this corridor:

- **VERIFIED:** 32
- **PROPOSED:** 0
- **AMBIGUOUS:** 8
- **UNRESOLVED:** 0

Verified Q station LEDs (20):

- LED184 Prospect Park; LED50 7 Av; LED163 Atlantic Av-Barclays Ctr;
  LED190 DeKalb Av; LED51 Parkside Av; LED53 Church Av; LED54 Beverley Rd;
  LED55 Cortelyou Rd; LED56 Newkirk Plaza; LED57 Avenue H; LED58 Avenue J;
  LED59 Avenue M; LED60 Kings Hwy; LED61 Avenue U; LED289 Neck Rd; LED114
  Sheepshead Bay; LED115 Brighton Beach; LED116 Ocean Pkwy; LED175 W 8
  St-NY Aquarium; LED7 Coney Island-Stillwell Av.

Verified Q segment LEDs (2):

- LED52: Parkside Av → Church Av.
- LED113: Neck Rd → Sheepshead Bay.

Verified B counterparts (9): LED185 Prospect Park; LED230 7 Av; LED160
Atlantic Av-Barclays Ctr; LED189 DeKalb Av; LED231 Church Av; LED232 Newkirk
Plaza; LED233 Kings Hwy; LED207 Sheepshead Bay; and LED208 Brighton Beach.

Verified F counterpart (1): LED112 at W 8 St-NY Aquarium.

## Shared-station pairs and groups

Route-specific LEDs remain separate physical records:

- **Prospect Park (B/Q):** LED184 = Q and LED185 = B. This resolves the shared
  pair immediately upstream of Q-only Parkside Av LED51 using the repeated
  lower/left Q-lane and upper/right B-lane geometry.
- **7 Av (B/Q):** LED50 = Q and LED230 = B. Their signed PCB offset matches
  the repeated B/Q lane orientation and continuity from Prospect Park.
- **Church Av:** LED53 = Q; LED231 = B.
- **Newkirk Plaza:** LED56 = Q; LED232 = B.
- **Kings Hwy:** LED60 = Q; LED233 = B.
- **Sheepshead Bay:** LED114 = Q; LED207 = B.
- **Brighton Beach:** LED115 = Q; LED208 = B. Keep both; B terminates here
  while Q continues.
- **W 8 St-NY Aquarium:** LED175 = Q; LED112 = F.
- **Coney Island-Stillwell Av:** LED7 = Q. Keep LED167, LED166, and LED165
  separate but do not assign their routes yet.
- **Atlantic Av-Barclays Ctr:** LED160 = B and LED163 = Q are verified station
  LEDs; their signed offset is approximately (+85,+75), consistent with the
  established B-minus-Q orientation from Prospect Park and 7 Av. LED159/LED158
  are an ambiguous likely B/Q pair (0.70), with station-versus-segment identity
  unresolved. LED136/LED157 are an ambiguous likely B/Q pair (0.65), with role
  in the converging corridor unresolved.
- **DeKalb Av:** LED189 = B and LED190 = Q are verified station LEDs; their
  signed offset is approximately (+85,+80), consistent with the established
  B-minus-Q orientation and continuous northward corridor.

**Still ambiguous / unassigned:** terminal route identities LED167, LED166,
and LED165; also LED290, whose possible Q-side corridor/segment position
between Prospect Park and 7 Av is not established well enough to assign.
Do not guess terminal D/N/F identities. LED7 remains verified as Q at Coney
Island-Stillwell Av.

There are no candidates currently marked `UNRESOLVED`.

## Human confirmation still required

The following exact route identities or element semantics remain ambiguous:

1. **Prospect Park–7 Av corridor:** determine whether LED290 is a Q-side
   corridor/segment LED and establish its route/endpoints if so.
2. **Atlantic Av-Barclays Ctr:** determine whether LED159/LED158 and
   LED136/LED157 are route-specific B/Q positions and whether they are
   stations or segments.
3. **Coney Island terminal:** identify the routes for LED167, LED166, and
   LED165. LED7 is verified as Q; do not infer assignments for the other three.

“Prospect Park” (B/Q) and “15 St–Prospect Park” (F/G) are distinct stations
and must never be conflated in the semantic model.
LED100 and LED206 are associated geographically with 15 St–Prospect Park on
the F/G corridor, are not Q-route candidates, and are omitted from this review.
Do not assign F versus G here.

The verified station and segment semantics are recorded in the master
worksheet with their supporting manufactured-artwork and human-review notes.

## U23 exclusion and addressable accounting

U23 is the separate discrete Avi-location LED on GPIO14 (`USER_LED`, active
HIGH), located approximately midway between addressable LEDs LED188 and
LED291. It is **not** in the addressable candidate list and is not part of an
LED chain. LED291 remains an ordinary addressable map LED; it is not U23 and is
not excluded from future map semantics.

The accounting remains **306 addressable LEDs total**, comprising **4 CH8
status LEDs** and **302 addressable subway-map LEDs**, plus U23 as a separate
discrete indicator.
