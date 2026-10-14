# Phase 3B route-anchored station matching

Review-only analysis. The authoritative worksheet, station registry, route ledgers, firmware, accepted image registration, and manufacturing files were not changed. The accepted 4096x3578 V1.1 artwork and projected LED centers were used as-is.

## Results

The union of the required historical source sets yields **261 unique station/service keys** across the 18 requested routes. Exact normalized route/name joins identify **61 already-verified current rows**; **200 expected historical station/service rows** remain unverified in this inventory.

| Match result | Count |
|---|---:|
| MATCH_HIGH | 2 |
| MATCH_MEDIUM | 0 |
| AMBIGUOUS | 198 |
| NO_MATCH | 0 |

**Aqueduct evidence:** in the enlarged accepted-artwork crop, LED266 is uniquely adjacent to the printed “Aqueduct Racetrack” marker on the A lane; LED267 is uniquely adjacent to “Aqueduct-North Conduit Av.” Their positions are distinct and in the displayed route sequence. Both are MATCH_HIGH for this read-only analysis; no worksheet/registry promotion was made.

The historical inventory does not exact-match **27** currently VERIFIED station rows by normalized station name plus route. They are not silently merged into another historical service row. This includes modern/current naming or service entries absent from the historical key set; it is an inventory limitation, not a challenge to current worksheet semantics.

## Route-local geometry calibration

The accepted PCB-to-raster registration was not revisited. Exploratory route-local robust similarity diagnostics from the prior analysis are retained as calibration context. Their transform coefficients are not present in the durable source artifacts, so they were not applied to claim new station matches here.

| Route/corridor | Anchors | Inliers | Median residual (px) | Maximum inlier residual (px) | Adjudication |
|---|---:|---:|---:|---:|---|
| B | 8 | 6 | 44 | 67 | No additional match accepted |
| F | 13 | 8 | 50 | 70 | No additional match accepted |
| L | 8 | 5 | 32 | 56 | No additional match accepted |
| Q | 17 | 14 | 42 | 99 | No additional match accepted |
| R | 5 | 5 | 12 | 38 | Candidate geometry only; lane/sequence evidence insufficient |
| G | — | — | — | — | Unstable fit; not used |
| A, C, D, J, M, N, S, Z, 2, 3, 4, 5 | — | — | — | — | Insufficient anchors; not fit |

Residuals are too large to support MATCH_HIGH without independent current-marker, lane, and sequence confirmation. No candidate was assigned from nearest-neighbor distance alone. Old shared-transfer designators and CP19 design intent were not treated as current physical identities.

## Remaining physical LEDs

The current worksheet has **213 unverified map LEDs**. Two are assigned to the Aqueduct station markers above. The remaining **211** have no defensible station or segment assignment from this pass.

| Leftover class | Count |
|---|---:|
| SEGMENT_HIGH | 0 |
| SEGMENT_MEDIUM | 0 |
| STILL_UNRESOLVED | 211 |

No segment was inferred merely because a package remains unmatched. Segment endpoints remain unassigned.

## Route summary

Historical service rows below are shown as `expected / current-exact / high / medium / ambiguous / no-match`. Physical leftovers are not allocated to route where service-lane assignment is not confirmed.

| Route | Verified station LEDs | Verified segment LEDs | MATCH_HIGH | MATCH_MEDIUM | AMBIGUOUS | NO_MATCH |
|---|---:|---:|---:|---:|---:|---:|
| A | 14 | 0 | 2 | 0 | 8 | 0 |
| B | 9 | 0 | 0 | 0 | 1 | 0 |
| C | 0 | 0 | 0 | 0 | 18 | 0 |
| D | 0 | 0 | 0 | 0 | 16 | 0 |
| F | 16 | 0 | 0 | 0 | 9 | 0 |
| G | 11 | 0 | 0 | 0 | 13 | 0 |
| J | 0 | 0 | 0 | 0 | 16 | 0 |
| L | 11 | 0 | 0 | 0 | 11 | 0 |
| M | 0 | 0 | 0 | 0 | 7 | 0 |
| N | 0 | 0 | 0 | 0 | 17 | 0 |
| Q | 20 | 1 | 0 | 0 | 4 | 0 |
| R | 6 | 0 | 0 | 0 | 11 | 0 |
| S | 1 | 0 | 0 | 0 | 6 | 0 |
| Z | 0 | 0 | 0 | 0 | 9 | 0 |
| 2 | 0 | 0 | 0 | 0 | 17 | 0 |
| 3 | 0 | 0 | 0 | 0 | 18 | 0 |
| 4 | 0 | 0 | 0 | 0 | 5 | 0 |
| 5 | 0 | 0 | 0 | 0 | 12 | 0 |

## Route atlas notes

`route_matching_atlas/` contains one locator image for each requested service. Green rings mark current VERIFIED station anchors; orange rings mark the two Aqueduct candidates. Each retains the accepted projected V1.1 artwork. For routes without a usable calibration, predicted points are intentionally omitted rather than fabricated. These are review locators, not a complete point-by-point atlas of unresolved stations.

The CSV includes one row per historical station/service key and one physical row per unverified LED. Blank predicted coordinates mean no route-local prediction was applied. No authoritative semantic status was changed.
