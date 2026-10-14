# Phase 3B Semantic Completion and Integrity Audit

## Result

**Phase 3B semantic identification is complete: 302 / 302 map LEDs are
assigned.**

- STATION rows: **276**
- SEGMENT rows: **26**
- Unresolved map LEDs: **0**
- Addressable chain: **306 LEDs** (302 map LEDs plus four status LEDs)
- Separate discrete U23 indicator: excluded from the chain and map semantic
  total
- Station registry: **188 entities**
- Registry IDs: **188 unique IDs, 1-188, with no gaps**

These counts were recalculated from
[`phase3b_semantic_mapping_worksheet.csv`](../phase3b_semantic_mapping_worksheet.csv),
not copied from the prior expected totals. All five final segments reference
existing station endpoint identities. The four registry additions were:

| ID | Entity | Station key |
|---:|---|---|
| 185 | Delancey St–Essex St | `manhattan_bridge_delancey_st` |
| 186 | Greenpoint Av | `crosstown_greenpoint_av` |
| 187 | Rockaway Av (3 line) | `irt_rockaway_av` |
| 188 | Junius St | `irt_junius_st` |

IDs 1-184 remain in place; IDs 185-188 are append-only additions. No GTFS
IDs were added. Shared route-specific LEDs use the same station entity where
they represent the same physical station.

## Final 13 human confirmations

The worksheet and manual review ledger record the final 13 as
`HUMAN_CONFIRMED` / `CONFIRMED`. All physical identity columns were retained.

| Designators | Confirmed semantics |
|---|---|
| LED188, LED201, LED202, LED255-LED259 | F / York St; 3 / Rockaway Av; 3 / Junius St; F/M/J/Z / Delancey St–Essex St; F / East Broadway |
| LED187 | G / SEGMENT / Greenpoint Av -> Nassau Av |
| LED285 | B / SEGMENT / Grand St -> DeKalb Av |
| LED286 | D / SEGMENT / Grand St -> Atlantic Av-Barclays Ctr |
| LED287 | A / SEGMENT / Hoyt-Schermerhorn Sts -> Nostrand Av |
| LED288 | C / SEGMENT / Lafayette Av -> Clinton-Washington Avs |

The shared Delancey St–Essex St F/M/J/Z station LEDs reuse ID185. No
station identity was created for a segment LED.

### Final correction: LED299

Joel's final-artwork confirmation supersedes LED299's prior human-confirmed
station interpretation. LED299 is now `L / SEGMENT / Broadway Junction ->
Atlantic Av`, using endpoint IDs **83 -> 50**. It has no station ID of its
own. The manual-review ledger records the correction as
`HUMAN_CONFIRMED` / `CONFIRMED` and retains the former station interpretation
in its supersession note; no station registry identity was added.

## Physical and CPL validation

- Worksheet map set: **302 unique designators**, all present in the V1.1
  placement table and accepted projected-center table.
- The 306 worksheet physical records match all 306 V1.1 placement records
  for designator, exact X/Y coordinates, and rotation angle.
- All 302 map records match accepted projection channel, channel index,
  global physical order, and PCB coordinates exactly.
- No duplicate map designators, channel/index pairs, or global physical-order
  values were found.
- Maximum radial CPL-to-placement residual across 306 addressable LEDs:
  **0.000631505 mm**, within the accepted **0.05 mm** tolerance.
- Maximum map-center deviation from accepted CPL-derived centers:
  **0.007087525 px**.
- The accepted affine/projection was not refit, and the physical chain order
  was not changed.

## Semantic integrity

- Every station row references a registry station.
- Every segment row references two valid, distinct endpoint stations; segment
  rows have no station ID of their own.
- Segment display names match the endpoint registry display names.
- All 26 segments have endpoint IDs in route order as recorded in the
  authoritative worksheet.
- Existing registry IDs were not renumbered or modified during the final
  promotion; new station IDs were appended.
- The four new registry rows have blank GTFS IDs; no GTFS IDs were invented.
- Route-specific LEDs at shared stations remain separate physical
  designators while reusing the common station identity.

No same-route duplicate station-semantic assignment remains. In particular,
LED89 is L / STATION / Bushwick Av-Aberdeen St (ID49), while LED299 is the
L segment Broadway Junction -> Atlantic Av (IDs 83 -> 50). The superseded
LED299 station interpretation is documented in the review ledger and below.

The repeated B and D segment endpoint pairs at LED285/309 and LED286/308 are
separate physical LEDs on the same displayed route paths, not station
identity collisions. Both physical designators and human-confirmed semantics
were preserved.

## Historical overrides retained

- LED96 = G / Lorimer St, not historical Metropolitan Av.
- LED308 = D and LED309 = B Manhattan Bridge segments; the old bridge audit
  had these reversed.
- LED291 = F / York St -> East Broadway, not historical D.
- LED304 = S / Botanic Garden -> Park Pl, despite its physical proximity to
  Franklin Av–Medgar Evers College.
- LED285 = B / Grand St -> DeKalb Av.
- LED286 = D / Grand St -> Atlantic Av-Barclays Ctr.
- LED287 = A / Hoyt-Schermerhorn Sts -> Nostrand Av.
- LED299 = L / SEGMENT / Broadway Junction -> Atlantic Av, superseding its
  prior L / Bushwick Av-Aberdeen St station interpretation.

Human-confirmed values prevail over superseded history. The LED96 GTFS naming
inconsistency remains flagged; no GTFS identity was inferred.

## Current review state and scope

The active candidate matrix is header-only, the current global and current
semantic review CSVs each contain 302 verified map rows, and the projected
LED cache contains all 306 addressable rows. Pre-confirmation analysis and
images are retained in `manual_review/` as resolved history. No unassigned
mapping or semantic-integrity exception remains. LED299's superseded prior
station interpretation is retained as historical review evidence.

This correction modified Phase 3B semantic mapping, review, and audit
documentation only. Firmware and manufacturing files were untouched. No
build, upload, commit, push, tag, or release was performed.
