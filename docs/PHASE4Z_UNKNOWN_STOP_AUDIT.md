# Phase 4Z — UnknownStop evidence audit

## Scope and decision boundary

This is a host-only, first-pass explanation of the Phase 4Y live UnknownStop observations. It makes no firmware, semantic, route-alias, GTFS mapping, hardware, or release changes. Classifications describe evidence and are not mapping-change recommendations. The H/H19 realtime-only operational-stop contract remains separate and unchanged. The Phase 4Y capture separately recorded 100 `OperationalStop` observations at H19N; this stop remains deliberately unmapped and has no LED.

Input aggregates: `PHASE4Y_UNKNOWN_STOP_COUNTS.csv` (152 route/platform pairs, 429 observations).
Static source: MTA NYCT GTFS `20260826-X-long-term-supplement-trip-ids`.
Capture replay input: `replay.json`.
Derived detail: [`PHASE4Z_UNKNOWN_STOP_AUDIT.csv`](./PHASE4Z_UNKNOWN_STOP_AUDIT.csv); it contains only aggregate stop/service/map evidence, not raw feed payloads or trip IDs.

The geographic scope check uses the project's authoritative `data/gtfs_stop_map.csv` and Phase 3B semantic worksheet. A route-specific map stop is considered nearby only within 150 m, matching the existing Phase 4E audit threshold. Nearest candidates are evidence only; no station or route assignment is inferred from proximity.

## Findings

- **423 observations** (149 pairs) are static-served route positions outside route-specific map coverage.
- **2 observations** are at the mapped route-2 Clark St parent 231, with previous stop 230 outside the project stop map; this is a map-boundary segment, not a missing Clark St station LED.
- **4 observations** occur at 2 pairs where the observed route is absent from static GTFS service: D/A21S (static A/B/C); D/A22S (static A/B/C). The replay records A21S with next stop A22S and later records A24S as a next stop while at A22S; this is consistent with a live path different from static route service. The cause (for example, a diversion or feed inconsistency) remains unresolved.
- **40 observations** across 35 pairs report `UnknownDestination` from the embedded topology lookup. The topology table intentionally contains on-map destinations only; these observations are consistent with off-map destinations.
- All 152 observed platform IDs have static GTFS stop records. 425 observations match static route service. No route/stop mapping correction is justified by this sample.

The replay export groups 389 UnknownStop observations without an `UnknownDestination` previous-stop failure and 40 with that failure. The no-failure group is not sufficient to infer whether a train was stopped or which adapter endpoint failed; the replay schema does not retain `current_status`. The Clark St pair is identifiable from its mapped current parent and reported previous stop 230.

The raw capture and replay JSON remain local under Git-ignored `captures/`; they were not copied into the audit outputs.

## Static GTFS provenance

- Feed version: `20260826-X-long-term-supplement-trip-ids`.
- Archive SHA-256: `2C9E69B82AC596A2FE660B56656509570CFBA3C2E4C50BE75EAF624EEE1BC6F8`.
- `stops.txt` normalized SHA-256: `423036143D340315590054C138B76692010886527EA0B3693DABE74FC1B55162`.
- `trips.txt` normalized SHA-256: `34E06E96CF2A1E9D9F6E88707AA031B9F442FD851954B16E4D41431AD1F48D9D`.
- `stop_times.txt` normalized SHA-256: `DB63C86F7C5CF3BFAD6D0FB53F16E66B15A8A0F41A2EDDDEEB6E45EB2FC049A0`.
- All three normalized member hashes match the source hashes recorded in `data/gtfs_topology.csv`, confirming the static feed matches the compiled topology's source snapshot.

## Aggregate distribution

| Route | Pairs | Observations | Topology UnknownDestination |
|---|---:|---:|---:|
| 2 | 24 | 49 | 14 |
| 3 | 9 | 21 | 6 |
| 4 | 25 | 49 | 15 |
| 5 | 5 | 10 | 3 |
| A | 16 | 58 | 1 |
| D | 14 | 46 | 0 |
| F | 23 | 79 | 0 |
| J | 5 | 11 | 0 |
| L | 1 | 10 | 0 |
| M | 2 | 7 | 0 |
| N | 15 | 50 | 0 |
| Q | 13 | 39 | 1 |

By platform suffix: N = 224 observations; S = 205 observations.

Most frequent parent stops (all directions combined):

| Parent | Station | N | S | Total |
|---|---|---:|---:|---:|
| A15 | 125 St | 13 | 4 | 17 |
| R14 | 57 St-7 Av | 15 | 0 | 15 |
| A24 | 59 St-Columbus Circle | 9 | 4 | 13 |
| B04 | 21 St-Queensbridge | 5 | 6 | 11 |
| D21 | Broadway-Lafayette St | 9 | 2 | 11 |
| F02 | 169 St | 11 | 0 | 11 |
| L01 | 8 Av | 0 | 10 | 10 |
| 127 | Times Sq-42 St | 5 | 3 | 8 |
| D03 | Bedford Park Blvd | 0 | 7 | 7 |
| R19 | 23 St | 1 | 6 | 7 |
| 302 | 145 St | 4 | 2 | 6 |
| A05 | 190 St | 6 | 0 | 6 |
| A14 | 135 St | 2 | 4 | 6 |
| A27 | 42 St-Port Authority Bus Terminal | 0 | 6 | 6 |
| R08 | 39 Av-Dutch Kills | 0 | 6 | 6 |
| 215 | 174 St | 5 | 0 | 5 |
| 503 | Gun Hill Rd | 3 | 2 | 5 |
| A12 | 145 St | 3 | 2 | 5 |
| A38 | Fulton St | 0 | 5 | 5 |
| B08 | Lexington Av/63 St | 5 | 0 | 5 |
| D20 | W 4 St-Wash Sq | 0 | 5 | 5 |
| J17 | 75 St-Elderts Ln | 5 | 0 | 5 |
| M01 | Middle Village-Metropolitan Av | 5 | 0 | 5 |
| R15 | 49 St | 5 | 0 | 5 |
| R16 | Times Sq-42 St | 1 | 4 | 5 |

## Highest-frequency route/platform pairs

| Route | Platform | Static station | Observations | Classification | Nearest route LED | Distance (m) |
|---|---|---|---:|---|---|---:|
| F | F02N | 169 St | 11 | OUTSIDE_ROUTE_MAP_SCOPE | York St | 16329 |
| L | L01S | 8 Av | 10 | OUTSIDE_ROUTE_MAP_SCOPE | Bedford Av | 4596 |
| N | R14N | 57 St-7 Av | 10 | OUTSIDE_ROUTE_MAP_SCOPE | Atlantic Av-Barclays Ctr | 8934 |
| A | A24N | 59 St-Columbus Circle | 9 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 7712 |
| D | A15N | 125 St | 8 | OUTSIDE_ROUTE_MAP_SCOPE | Atlantic Av-Barclays Ctr | 14249 |
| D | D03S | Bedford Park Blvd | 7 | OUTSIDE_ROUTE_MAP_SCOPE | Atlantic Av-Barclays Ctr | 22332 |
| A | A05N | 190 St | 6 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 18398 |
| A | A27S | 42 St-Port Authority Bus Terminal | 6 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 6454 |
| F | B04S | 21 St-Queensbridge | 6 | OUTSIDE_ROUTE_MAP_SCOPE | Delancey St–Essex St | 5487 |
| N | R08S | 39 Av-Dutch Kills | 6 | OUTSIDE_ROUTE_MAP_SCOPE | Atlantic Av-Barclays Ctr | 8477 |
| 2 | 215N | 174 St | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Clark St | 17916 |
| A | A15N | 125 St | 5 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 12852 |
| A | A38S | Fulton St | 5 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 1886 |
| D | D21N | Broadway-Lafayette St | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Atlantic Av-Barclays Ctr | 4818 |
| F | B04N | 21 St-Queensbridge | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Delancey St–Essex St | 5487 |
| F | D20S | W 4 St-Wash Sq | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Delancey St–Essex St | 1851 |
| J | J17N | 75 St-Elderts Ln | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Cypress Hills | 482 |
| M | M01N | Middle Village-Metropolitan Av | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Forest Av | 1377 |
| N | R15N | 49 St | 5 | OUTSIDE_ROUTE_MAP_SCOPE | Atlantic Av-Barclays Ctr | 8420 |
| Q | B08N | Lexington Av/63 St | 5 | OUTSIDE_ROUTE_MAP_SCOPE | DeKalb Av | 8343 |
| Q | R14N | 57 St-7 Av | 5 | OUTSIDE_ROUTE_MAP_SCOPE | DeKalb Av | 8241 |
| 3 | 302N | 145 St | 4 | OUTSIDE_ROUTE_MAP_SCOPE | Clark St | 14501 |
| 4 | 628N | 68 St-Hunter College | 4 | OUTSIDE_ROUTE_MAP_SCOPE | Borough Hall | 8626 |
| A | A14S | 135 St | 4 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 13683 |
| A | A15S | 125 St | 4 | OUTSIDE_ROUTE_MAP_SCOPE | High St | 12852 |

## Interpretation and follow-up

The 149 route-served, unmapped pairs have no route-specific map LED within 150 m. They are outside the Brooklyn map's displayed service scope, rather than evidence that 149 station assignments are missing. The single mapped Clark St pair reflects a segment crossing the map edge because Wall St parent 230 is not mapped. The two D-route pairs should remain an operational-feed investigation item; the capture does not authorize route aliases or GTFS changes.

Next recommended work is a dedicated review of the D/A21S and D/A22S live/static route discrepancy and, separately, a later capture that includes real on-map segment observations. Preserve the raw capture under ignored `captures/`; do not add it or live replay payloads to Git.

Reproduce the focused tests with `python tools\run_phase4z_tests.py`. Regenerate the local audit from the same ignored inputs with:

```powershell
python tools\phase4z\unknown_stop_audit.py --gtfs captures\phase4z_static\gtfs_subway.zip --replay captures\20261019T062411Z-464a7da9\replay.json --csv docs\PHASE4Z_UNKNOWN_STOP_AUDIT.csv --report docs\PHASE4Z_UNKNOWN_STOP_AUDIT.md
```

No stop mappings, route aliases, topology, station semantics, segment semantics, route colors, LED assignments, firmware, or hardware were changed.
