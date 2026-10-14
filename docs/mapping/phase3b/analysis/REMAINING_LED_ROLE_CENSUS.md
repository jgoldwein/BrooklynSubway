# Phase 3B remaining-LED role census

> **Historical census only; no active candidates remain.** Phase 3B is
> complete: 302 / 302 map LEDs are resolved (276 STATION, 26 SEGMENT, 0
> unresolved). See
> [PHASE3B_COMPLETION_AUDIT.md](PHASE3B_COMPLETION_AUDIT.md). The old 213-row
> census is preserved in
> [role_census_pre_final_confirmation.csv](../manual_review/role_census_pre_final_confirmation.csv);
> the active census CSV has a header and zero candidate rows.

The following method and totals describe the prior read-only census, not the
current worksheet or an active review queue. The accepted V1.1 registration
was used as-is; no image registration was revisited or refit.

## Baseline and method

- Map LEDs: **302**; verified: **89**; unverified: **213**.
- The existing `led_visual_classification.csv` marks the remaining rows `UNCLASSIFIED` and cautions that its generated raster does not isolate printed station-marker geometry. Accordingly, this census records only clear artwork-level candidate evidence and keeps unresolved station/segment distinctions as `AMBIGUOUS_ROLE` rather than inferring from designator numbering or history.
- Candidate route reflects visible artwork only; historical records are evidence-only and do not establish current identity by LED designator.
- `visual_confidence` is qualitative: 0.60–0.79 = medium candidate/location confidence; 0.20 = role unresolved. A candidate is not a semantic promotion.

## Remaining-role totals

- STATION_CANDIDATE: **2**
- SEGMENT_CANDIDATE: **0**
- TRANSFER_CLUSTER_CANDIDATE: **13**
- OTHER_MAP_INDICATOR: **0**
- AMBIGUOUS_ROLE: **198**

- Confidently identifiable single route: **3** (A: 2, M: 1); a further **12** are identifiable only as MULTI/TRANSFER groups.
- Confidently identifiable station name: **13**.
- Confidently identifiable segment endpoints: **0**.
- Rows requiring manual visual review: **212** (ambiguous role, unresolved Aqueduct naming, or unresolved service split in transfer cluster).

## Station candidates

| Designator | Route | Candidate station | Confidence | Verified route anchors | Historical support / note |
|---|---|---|---:|---|---|
| LED267 | A | Unresolved: Aqueduct pair | 0.65 | LED268 (Howard Beach-JFK Airport) | No direct historical station/service match; adjacent labels do not resolve package-to-name pairing. |
| LED266 | A | Unresolved: Aqueduct pair | 0.65 | LED268 (Howard Beach-JFK Airport) | No direct historical station/service match; adjacent labels do not resolve package-to-name pairing. |

No remaining LED met the visual threshold for a confidently bounded segment with both neighboring station names. LED113 remains the sole VERIFIED segment and is outside the 213-row census.

## Transfer clusters

| Cluster | LEDs in census / nearby physical LEDs | Apparent service lanes | Separation assessment |
|---|---|---|---|
| Coney Island-Stillwell Av | LED166, LED167; nearby LED7 (Q), LED165 (F) | D/F/N/Q terminal complex | Complex is clear; LED7 and LED165 are route-specific, but individual D/N attribution for LED166/167 remains unresolved. |
| Myrtle-Wyckoff Avs | LED170; nearby LED85, LED86, LED87 (L) | L/M | LED86 is separately VERIFIED on L; LED170 appears on the M-side lane. Keep service-specific LEDs separate. |
| Broad Channel | LED269, LED270 | A / Rockaway Park Shuttle (S) | Paired packages are visible; package-to-service split is not clear enough to assign individually. |
| Rockaway Beach stops | LED277–LED284 | A / Rockaway Park Shuttle (S) | Stop names are readable and paired packages align to stops; individual A-versus-S package split remains unresolved. |

## Aqueduct crop review

- **LED266:** V1.1 regional crop places the package at the upper Aqueduct A-line stop marker adjacent to the “Aqueduct Racetrack” label; its nearest verified A-route anchor is LED268, Howard Beach-JFK Airport.
- **LED267:** The crop places the adjacent package at the next Aqueduct A-line stop marker near the “Aqueduct North Conduit Av” label.
- Both are `STATION_CANDIDATE`, route A, with names intentionally left blank: the two adjacent labels/markers overlap the projected packages in the current crop, so the exact pair mapping is not called unambiguous. Their prior review rows remain PROPOSED_MEDIUM; no promotion was made.

## Route-by-route census

Verified counts are from the authoritative worksheet. Remaining candidate counts include only route assignments that are visually clear in this pass; `UNKNOWN` and `MULTI/TRANSFER` are not redistributed to individual routes.

| Route | Verified station LEDs | Verified segment LEDs | Remaining station candidates | Remaining segment candidates | Transfer-cluster candidates | Ambiguous-role LEDs |
|---|---:|---:|---:|---:|---:|---:|
| A | 14 | 0 | 2 | 0 | 0 | 0 |
| B | 9 | 0 | 0 | 0 | 0 | 0 |
| C | 0 | 0 | 0 | 0 | 0 | 0 |
| D | 0 | 0 | 0 | 0 | 0 | 0 |
| F | 16 | 0 | 0 | 0 | 0 | 0 |
| G | 11 | 0 | 0 | 0 | 0 | 0 |
| J | 0 | 0 | 0 | 0 | 0 | 0 |
| L | 11 | 0 | 0 | 0 | 0 | 0 |
| M | 0 | 0 | 0 | 0 | 1 | 0 |
| N | 0 | 0 | 0 | 0 | 0 | 0 |
| Q | 20 | 1 | 0 | 0 | 0 | 0 |
| R | 6 | 0 | 0 | 0 | 0 | 0 |
| S | 1 | 0 | 0 | 0 | 0 | 0 |
| Z | 0 | 0 | 0 | 0 | 0 | 0 |
| 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| 5 | 0 | 0 | 0 | 0 | 0 | 0 |
| MULTI/TRANSFER (not a single route) | — | — | 0 | 0 | 12 | 0 |
| UNKNOWN route | — | — | 0 | 0 | 0 | 198 |

The route table intentionally exposes limited route certainty: most unverified packages cannot safely be assigned to a single route from the generated overlay alone. The lane colors in the prior visual-classification output are palette families, not an authoritative service legend.

## Source files

- `phase3b_semantic_mapping_worksheet.csv` — authoritative identities and verified/unverified status (read-only).
- `station_registry.csv` — registry cross-check (read-only).
- `projected_led_centers.csv` — accepted projected positions (read-only).
- `current_semantic_review.csv`, `led_visual_classification.csv`, `route_sequences.csv`, `ANALYSIS_REPORT.md`, and `station_recovery_atlas/` — existing analysis/evidence (read-only).
- Historical mapping CSVs under `local_reference/historical_mapping/` — historical support column only; designators do not imply current identities.
