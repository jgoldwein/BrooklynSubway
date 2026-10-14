# Historical Phase 3B promotions and pre-confirmation review

> **Resolved historical evidence only.** This document predates the final
> review of the last 13 map LEDs. Every candidate described in its
> pre-confirmation analysis has since been human-confirmed. Current status is
> 302 / 302 map LEDs assigned, 277 STATION, 25 SEGMENT, and zero unresolved;
> see [PHASE3B_COMPLETION_AUDIT.md](../analysis/PHASE3B_COMPLETION_AUDIT.md).
> Do not treat historical candidate labels below as active assignments or a
> current review queue.

## Human-confirmed promotions

All 44 supplied assignments were promoted as authoritative. The worksheet's
physical inventory fields were preserved. Validation against
`led_positions_v11.csv`, `analysis/cpl_validation.csv`, the accepted CPL
projection, and final V1.1 artwork found:

- 44/44 designators match their V1.1 placement X/Y/angle exactly.
- 44/44 CPL placements are within the 0.05 mm tolerance; maximum radial
  residual is 0.000559595 mm.
- 44/44 channel, channel-index, and global-order identities match the
  accepted projection. The published affine reproduces their accepted pixel
  centers within 0.005319 px.
- No physical-identity or artwork-registration contradiction was found.
- Every promoted ledger row is `HUMAN_CONFIRMED` / `CONFIRMED`; every source
  note begins `Human identified from CPL-derived final V1.1 artwork.`

### Stations (31 LEDs)

| Corridor | Human-confirmed assignments |
|---|---|
| Fourth Avenue R | LED17 = Union St; LED18 = 4 Av-9 St; LED19 = Prospect Av |
| West End D | LED30 = 62 St; LED31 = 71 St; LED32 = 79 St; LED33 = 18 Av; LED34 = 20 Av; LED35 = Bay Pkwy; LED36 = 25 Av; LED37 = Bay 50 St |
| Crosstown G | LED95 = Nassau Av; LED96 = Lorimer St |
| Culver F/G | LED97 = F / Bergen St; LED247 = G / Bergen St; LED98 = F / Carroll St; LED248 = G / Carroll St; LED99 = F / Smith-9 Sts; LED249 = G / Smith-9 Sts |
| Sea Beach N | LED105 = 18 Av; LED106 = 20 Av; LED107 = Bay Pkwy; LED108 = Kings Hwy; LED109 = Avenue U; LED110 = 86 St |
| Broadway J/M/Z | LED234 = Z / Marcy Av; LED62 = J / Marcy Av; LED235 = M / Marcy Av; LED236 = M / Hewes St; LED63 = J / Hewes St |
| Fulton C | LED120 = Lafayette Av |

Shared physical station identities are reused for 4 Av-9 St (registry ID86),
Marcy Av (ID118), Bergen St (ID171), Carroll St (ID172), Smith-9 Sts (ID173),
and Hewes St (ID180). Same-name stations on different corridors remain
distinct.

### Segments (13 LEDs)

| LED | Confirmed segment | Endpoint IDs |
|---|---|---|
| LED305 | S / Broad Channel -> Beach 90 St | 154 -> 155 |
| LED308 | D / Grand St -> Atlantic Av-Barclays Ctr | 182 -> 2 |
| LED309 | B / Grand St -> DeKalb Av | 182 -> 1 |
| LED310 | Q / Canal St -> DeKalb Av | 183 -> 1 |
| LED311 | N / Canal St -> Atlantic Av-Barclays Ctr | 183 -> 2 |
| LED291 | F / York St -> East Broadway | 107 -> 184 |
| LED292 | N / Atlantic Av-Barclays Ctr -> 36 St | 2 -> 78 |
| LED295 | G / Lorimer St -> Broadway | 170 -> 35 |
| LED297 | M / Hewes St -> Lorimer St | 180 -> 109 |
| LED300 | 2 / Franklin Av-Medgar Evers College -> President St-Medgar Evers College | 89 -> 121 |
| LED301 | 3 / Franklin Av-Medgar Evers College -> Nostrand Av | 89 -> 146 |
| LED304 | S / Botanic Garden -> Park Pl | 90 -> 153 |
| LED303 | 5 / Atlantic Av-Barclays Ctr -> Franklin Av-Medgar Evers College | 2 -> 89 |

Manhattan Bridge endpoint identities use the accepted final-map topology:
D Grand St -> Atlantic Av-Barclays Ctr; B Grand St -> DeKalb Av; Q Canal St
-> DeKalb Av; N Canal St -> Atlantic Av-Barclays Ctr. Grand St, Canal St,
and East Broadway were appended as endpoint entities only (IDs 182-184); no
fake station LED or GTFS identity was created.

Historical conflicts were retained as superseded evidence, not applied to the
current worksheet: LED96 remains G / Lorimer St rather than historical
Metropolitan Av (G29); LED308/309 are D/B, reversing the older bridge-audit
values; LED291 remains the F segment rather than the historical D assignment.
The G Lorimer registry/GTFS naming discrepancy is explicitly flagged for
review.

## Recalculated authoritative totals

| Measure | Before this batch | Current |
|---|---:|---:|
| VERIFIED map semantics | 245 | 289 |
| STATION rows | 238 | 269 |
| SEGMENT rows | 7 | 20 |
| Unresolved map LEDs | 57 | 13 |
| Station-registry entities | 158 | 184 |

The registry grew by 26 append-only IDs: 23 new station identities (159-181)
and three endpoint-only identities (182-184). Existing IDs were not renumbered.
No GTFS IDs were added.

## Fresh forensic analysis of the 13 remaining LEDs

The unresolved set was derived from the current worksheet and is exactly:
`LED187, LED188, LED201, LED202, LED255, LED256, LED257, LED258, LED259,
LED285, LED286, LED287, LED288`.

Each row was rechecked against the exact manufactured position, accepted
projection, current HUMAN_CONFIRMED/VERIFIED anchors, final-artwork lane and
station geometry, and route sequence. The review is inference-only: no
candidate below was promoted or assigned a new station-registry identity.
The reviewed legacy crosswalk and CP19 rows provided no direct current
assignment for these 13 designators.

| LED | Classification | Proposed route / role / station or endpoints | Evidence, route constraints, and nearest confirmed anchors |
|---|---|---|---|
| LED187 | `NO_DEFENSIBLE_ASSIGNMENT` | No unique assignment; G station versus segment remains possible | At the north end of the visible G corridor, but no station marker or complete label binds to the package. Human-confirmed G / Nassau Av LED95 is nearby; assigning Nassau Av again would duplicate the confirmed G stop. LED96 / Lorimer St provides corridor context but does not establish the next station or segment endpoints. No route-order inversion can be assessed safely. |
| LED188 | `HIGH_CANDIDATE` | F / STATION / York St | Projected center coincides with the York St stop marker on the F lane. F / Jay St-MetroTech -> York St LED294 and human-confirmed F / York St -> East Broadway LED291 bracket the stop and support the route and sequence. This introduces neither a duplicate F stop nor an order inversion. Candidate only. |
| LED201 | `HIGH_CANDIDATE` | 3 / STATION / Rockaway Av | Center aligns with the printed 3-line Rockaway Av marker. Confirmed LED200 (3 / Saratoga Av) precedes it and LED202 (candidate Junius St) and LED203 (3 / Pennsylvania Av) follow in the printed sequence; nearby L anchors LED91/92 are transfer context. This is IRT Rockaway Av, distinct from C / Rockaway Av (registry ID143). Candidate only. |
| LED202 | `HIGH_CANDIDATE` | 3 / STATION / Junius St | Center aligns with the printed red 3-line Junius St marker at the 3/L transfer. Confirmed 3 / Saratoga Av LED200 precedes it and 3 / Pennsylvania Av LED203 follows it; L / Sutter Av LED91 and Livonia Av LED92 explain the transfer geometry. Candidate only. |
| LED255 | `AMBIGUOUS` | Possible F / Delancey St station or J/M/Z / Essex St station; segment not excluded | Part of a four-package stack across parallel Delancey/Essex route lanes. The crop shows both stop names, but does not bind this footprint to one service or prove station versus short segment. J/M/Z Marcy Av LEDs62/235/234 are route-sequence anchors. |
| LED256 | `AMBIGUOUS` | Possible F / Delancey St or J/M/Z / Essex St station; segment not excluded | Same stacked route bundle as LED255/257/258. A unique service-to-package assignment is not visible; assigning from chain/designator order would be unsupported. J/M/Z Marcy Av LEDs62/235/234 provide only sequence context. |
| LED257 | `AMBIGUOUS` | Possible F / Delancey St or J/M/Z / Essex St station; segment not excluded | Same stacked route bundle as LED255/256/258. Printed stops support a station-cluster interpretation, but a unique service lane and individual role remain unresolved. |
| LED258 | `AMBIGUOUS` | Possible F / Delancey St or J/M/Z / Essex St station; segment not excluded | Same stacked route bundle as LED255/256/257. No unique route/stop pairing is defensible from the crop; retain as a cluster review. |
| LED259 | `HIGH_CANDIDATE` | F / STATION / East Broadway | Center aligns with the printed East Broadway stop on the F lane. Human-confirmed F segment LED291 terminates at this existing East Broadway endpoint (registry ID184), supporting the stop rather than a segment-midpoint interpretation. Candidate only; LED259 has no station assignment. |
| LED285 | `AMBIGUOUS` | Possible A/C / STATION / High St or B/D/N/Q / bridge SEGMENT; route/endpoints unresolved | Lies in the High St / Manhattan Bridge approach where A/C station lanes and bridge lines converge. Confirmed A/C High St LEDs209/117 and human-confirmed bridge segments LED308-311 constrain the geometry, but do not identify this package's route or role. Some bridge routes already have confirmed segment LEDs, so an additional same-route segment cannot be assumed. |
| LED286 | `AMBIGUOUS` | Possible A/C / STATION / High St or B/D/N/Q / bridge SEGMENT; route/endpoints unresolved | Adjacent to LED285 in the same converging approach. It has the same station-versus-segment and route ambiguity; no unique endpoints can be selected without risking a duplicate bridge segment. |
| LED287 | `AMBIGUOUS` | Possible station on an Atlantic Av-Barclays Ctr/DeKalb service lane or a segment into that transfer complex | Center lies among converging transfer lanes. Human-confirmed 3 / Atlantic Av-Barclays Ctr LED136 and R / DeKalb Av LED191, plus verified B/Q Atlantic LEDs160/163 and C / Lafayette Av LED120, give nearby anchors but not a unique route-specific marker or consecutive endpoint pair. |
| LED288 | `HIGH_CANDIDATE` | C / SEGMENT / Lafayette Av -> Clinton-Washington Avs; endpoint IDs 181 -> 150 | Center lies on the blue C lane between human-confirmed C / Lafayette Av LED120 (ID181) and Clinton-Washington Avs LED121 (ID150), not on either printed station marker. The endpoints preserve route order and do not duplicate a station. Candidate only; no station identity is created for LED288. |

### Candidate counts

| Classification | Count | Designators |
|---|---:|---|
| `HIGH_CANDIDATE` | 5 | LED188, LED201, LED202, LED259, LED288 |
| `MEDIUM_CANDIDATE` | 0 | — |
| `AMBIGUOUS` | 7 | LED255, LED256, LED257, LED258, LED285, LED286, LED287 |
| `NO_DEFENSIBLE_ASSIGNMENT` | 1 | LED187 |

There are no contradictions with the 289 current VERIFIED semantic assignments.
Where a plausible alternative would duplicate an existing stop or confirmed
route segment, that alternative is documented and has not been selected.

## Review crops

These review images reuse existing accepted-artwork crops without resampling
the map panels. Older marker/status overlays are identified in the crop
footers where later human confirmations supersede them.

- Cluster A1, LED187: [cluster_A_LED187_nassau.png](../manual_review/remaining_clusters/cluster_A_LED187_nassau.png)
- Cluster A2, LED188: [cluster_A_LED188_york.png](../manual_review/remaining_clusters/cluster_A_LED188_york.png)
- Cluster B, LEDs255-259: [cluster_B_LED255_LED259_lower_east_side.png](../manual_review/remaining_clusters/cluster_B_LED255_LED259_lower_east_side.png)
- Cluster C, LEDs285-288: [cluster_C_LED285_LED288_bridge_downtown.png](../manual_review/remaining_clusters/cluster_C_LED285_LED288_bridge_downtown.png)
- Cluster D, LEDs201-202: [cluster_D_LED201_LED202_eastern_brooklyn.png](../manual_review/remaining_clusters/cluster_D_LED201_LED202_eastern_brooklyn.png)

No firmware or manufacturing files were modified. No build, upload, commit,
push, release, or tag action was performed.
