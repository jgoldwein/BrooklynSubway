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
geometry south of Brighton Beach. Promoted rows have confidence `1.00` and
status `VERIFIED`. Electrical chain order is used only to preserve physical
identity, never to infer semantics.

## Candidate counts

The CSV contains **29 addressable candidate LEDs** for this corridor:

- **VERIFIED:** 26
- **AMBIGUOUS:** 3
- **UNRESOLVED:** 0

Verified Q station LEDs (17):

- LED184 Prospect Park; LED51 Parkside Av; LED53 Church Av; LED54 Beverley Rd;
  LED55 Cortelyou Rd; LED56 Newkirk Plaza; LED57 Avenue H; LED58 Avenue J;
  LED59 Avenue M; LED60 Kings Hwy; LED61 Avenue U; LED289 Neck Rd; LED114
  Sheepshead Bay; LED115 Brighton Beach; LED116 Ocean Pkwy; LED175 W 8
  St-NY Aquarium; LED7 Coney Island-Stillwell Av.

Verified Q segment LEDs (2):

- LED52: Parkside Av → Church Av.
- LED113: Neck Rd → Sheepshead Bay.

Verified B counterparts (6): LED185 Prospect Park; LED231 Church Av; LED232
Newkirk Plaza; LED233 Kings Hwy; LED207 Sheepshead Bay; and LED208 Brighton
Beach.

Verified F counterpart (1): LED112 at W 8 St-NY Aquarium.

## Shared-station pairs and groups

Route-specific LEDs remain separate physical records:

- **Prospect Park (B/Q):** LED184 = Q and LED185 = B. This resolves the shared
  pair immediately upstream of Q-only Parkside Av LED51 using the repeated
  lower/left Q-lane and upper/right B-lane geometry.
- **Church Av:** LED53 = Q; LED231 = B.
- **Newkirk Plaza:** LED56 = Q; LED232 = B.
- **Kings Hwy:** LED60 = Q; LED233 = B.
- **Sheepshead Bay:** LED114 = Q; LED207 = B.
- **Brighton Beach:** LED115 = Q; LED208 = B. Keep both; B terminates here
  while Q continues.
- **W 8 St-NY Aquarium:** LED175 = Q; LED112 = F.
- **Coney Island-Stillwell Av:** LED7 = Q. Keep LED167, LED166, and LED165
  separate but do not assign their routes yet.

**Still ambiguous / unassigned:** terminal route identities LED167, LED166,
and LED165. Do not guess their D/N/F identities. LED7 remains verified as Q at
Coney Island-Stillwell Av.

There are no candidates currently marked `UNRESOLVED`.

## Human confirmation still required

The following exact route identities remain ambiguous:

1. **Coney Island terminal:** identify the routes for LED167, LED166, and
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
