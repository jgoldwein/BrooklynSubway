# Historical Phase 3B station-centric legacy recovery

> **Historical analysis only; not an active review or current census.** Phase
> 3B is complete: the authoritative worksheet and synchronized CSV contain
> 302 / 302 resolved map LEDs (276 STATION, 26 SEGMENT, 0 unresolved). See
> [PHASE3B_COMPLETION_AUDIT.md](PHASE3B_COMPLETION_AUDIT.md) and the current
> [global semantic review](../global_semantic_review.md). Counts and proposal
> statuses below describe the earlier legacy-recovery snapshot only.

This archived review-only pass used the accepted physical/raster registration
and projected LED coordinates unchanged. Its historical proposal analysis
did not promote semantics; subsequent human-confirmed mappings are documented
in the authoritative worksheet and completion audit.

## Historical totals before final confirmations

- ALREADY_VERIFIED: **51**
- PROPOSED_HIGH: **37**
- PROPOSED_MEDIUM: **3**
- AMBIGUOUS: **23**
- UNRESOLVED: **188**
- POTENTIAL_CONTRADICTION: **0**
- Usable proposals among the original 251 unverified LEDs: **40**.
- Current unresolved review rows: **188**. The current
  unresolved rows do not yet have a supported element type, so an exact count
  of unresolved *station* LEDs cannot be established without guessing station
  versus segment geometry.

## A. Old single-route stations

- Historical single-route station/service records: **86**.
- Distinct station/service entities represented: **79**.
- Current LEDs supported at HIGH level by current artwork plus single-route
  history: **22**.
- Current LEDs supported at MEDIUM level by single-route history: **0**.
- Matched current ambiguous LEDs: **0**.
- Matched current unresolved LEDs with a station identity available: **0**.

Legacy designators are retained as evidence only. The recovered identity is
the station/service entity; the current physical LED is selected by the V1.1
artwork and projection. Existing single-route-supported MEDIUM proposals are
raised to HIGH only when the station name and service agree and no verified
assignment is displaced.

## B. Old shared stations

- Shared historical station entities represented: **53**.
- Station clusters with a current review-ledger station match: **14**.
- Route-specific current LEDs already VERIFIED or proposed at those clusters: **30**.
- Current route-specific rows at matched shared clusters by status: {'ALREADY_VERIFIED': 28, 'PROPOSED_HIGH': 1, 'PROPOSED_MEDIUM': 1}.

The old shared physical LED is not restored as a V1.1 identity. Where the
current review already has a station/service assignment, the catalog supplies
station-level corroboration; clusters without a defensible V1.1 route-lane
assignment remain ambiguous or unresolved.

## C. CP19 design-intent continuity

- CP19 rows reviewed: **21**.
- Relationship counts: **{'UNCERTAIN': 6, 'SAME_STATION_DIFFERENT_ROUTE': 11, 'CONFIRMED_CONTINUITY': 3, 'SUPERSEDED': 1}**.
- Same designator + same station/service: **3**.
- Same designator with current semantic conflict: **0**.
- CP19 designators superseded by another current LED at the station/service: **1**.
- Conflicts: **0**.
- Same station but different current service/LED: **11**.
- Still uncertain: **6**.
- `CONFIRMED_CONTINUITY` means current row has same designator, station, and
  route. `SAME_STATION_DIFFERENT_ROUTE` and `SUPERSEDED` do not assert that
  CP19's old physical placement survived. `CONFLICT` and `UNCERTAIN` are not
  promoted.

## D. Historical station catalog and visual atlas

- Historical catalog records normalized into station entities: **202**.
- Station-name entities with current review-ledger matches: **63**.
- Atlas crops generated from the original 4096×3578 EasyEDA image: **74**.
- Shared-transfer legacy-pointer locator crops: **15**.
- Atlas directory: `station_recovery_atlas/`.
- Crops show current nearby projected LED centers. Green denotes existing
  ALREADY_VERIFIED items; orange denotes unverified reviewed candidates. Labels
  include current designator, service (when assigned), and review status. CP19
  and legacy-shared locator crops are explicitly investigative; the old LED
  designator is not presumed to retain its historical station meaning.

## Outputs and safeguards

- `legacy_station_service_catalog.csv` records station entities, service
  families, available GTFS parents, historical designators, source coordinates,
  mapping modes, and source files.
- `cp19_continuity_review.csv` classifies all CP19 rows against the current
  review ledger without treating CP19 placements as current truth.
- `current_semantic_review.csv` was updated only with station/service legacy
  evidence and the supported single-route confidence/status adjustment.
- No segment pass was attempted. No new VERIFIED assignment was made.
- The original raster, accepted affine, projected centers, master worksheet,
  station registry, firmware, and manufacturing files were not modified.
