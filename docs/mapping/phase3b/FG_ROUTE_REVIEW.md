# F/G Route Semantic Review

**Status: human-review stage only.** No firmware or master semantic worksheet
was modified. Physical identities are retained from the master worksheet;
electrical chain order is not used as semantic evidence.

## Candidate counts

The review ledger contains **28 candidates**:

- **ALREADY_VERIFIED:** 1 — LED112, F at W 8 St-NY Aquarium.
- **Newly PROPOSED F stations:** 15.
- **Newly PROPOSED G stations:** 4.
- **Proposed F segments:** 0.
- **Proposed G segments:** 0.
- **AMBIGUOUS:** 6.
- **UNRESOLVED:** 0.
- **NOT_FG:** 2 — verified Q counterparts LED175 and LED7.

No LED was proposed as a segment: no unassigned candidate was shown clearly
between two identifiable same-route station anchors as a train position.

## Proposed shared F/G station pairs

Keep the two physical LEDs separate at every shared station:

- **Church Av:** LED40 = F and LED229 = G (confidence 0.92). The artwork and
  lane continuity show LED40 continuing south into the F-only Culver corridor,
  while the paired lane ends at Church Av.
- **Fort Hamilton Pkwy:** LED39 = F and LED228 = G (confidence 0.88). The
  paired coordinates continue the same parallel-lane orientation north from
  Church Av.
- **7 Av:** LED38 = F and LED227 = G (confidence 0.87). Their lane offset
  continues the repeated F/G pairing; these are distinct from the B/Q 7 Av
  station LEDs.
- **15 St-Prospect Park:** LED100 = F and LED206 = G (confidence 0.84).
  Continuity from the proposed Church, Fort Hamilton Pkwy, and 7 Av pairs
  supports the split. This is the F/G station **15 St-Prospect Park**, not the
  B/Q station **Prospect Park**. Do not merge the two station identities.

The pairs are review-stage proposals only, not VERIFIED assignments.

## Key determinations

- **LED100 / LED206:** the geometry and continuity support proposing LED100 =
  F and LED206 = G at 15 St-Prospect Park. They are not the B/Q Prospect
  Park station LEDs.
- **Church Av:** yes, the separate LED40/LED229 pair provides a reliable
  review-stage orientation anchor: LED40 continues south on F, while LED229's
  G lane terminates at Church Av.
- **Coney Island:** yes, LED165 can be proposed as F from its repeated
  F-to-Q lane offset relative to verified W 8 St and Coney Island Q anchors.
  LED166 and LED167 remain ambiguous and unassigned.

## Proposed F-only southern sequence

Geographic order from the G terminal and through the F-only continuation:

1. LED40 — F, Church Av (shared-station F side; confidence 0.92).
2. LED41 — F, Ditmas Av (confidence 0.84).
3. LED42 — F, 18 Av (confidence 0.84).
4. LED43 — F, Avenue I (confidence 0.84).
5. LED44 — F, Bay Pkwy (confidence 0.84).
6. LED45 — F, Avenue N (confidence 0.84).
7. LED46 — F, Avenue P (confidence 0.84).
8. LED47 — F, Kings Hwy (confidence 0.84).
9. LED48 — F, Avenue U (confidence 0.84).
10. LED49 — F, Avenue X (confidence 0.84).
11. LED111 — F, Neptune Av (confidence 0.84).
12. LED112 — F, W 8 St-NY Aquarium (`ALREADY_VERIFIED`).
13. LED165 — F, Coney Island-Stillwell Av (confidence 0.94).

The proposed station positions follow the manufactured F-lane artwork and
physical continuity. LED112 remains unchanged. At Coney Island, the signed
LED offset supports LED165 as the F counterpart to Q LED7: LED165 − LED7 is
approximately (-85,+80), matching the F − Q offset LED112 − LED175 of
(-85,+85) at W 8 St. Do not assign LED166 or LED167 to another route based on
this finding.

No F or G segment is proposed.

## Ambiguous and excluded candidates

- **LED168 / LED169** and **LED172 / LED173:** their positions are near
  northern Brooklyn route convergence, but the artwork and coordinates used in
  this pass do not establish exact F/G identity, station name, or whether they
  are stations or train-position segments. All remain `AMBIGUOUS`.
- **LED166 / LED167:** separate terminal markers with unresolved route
  identities. LED165 is the F proposal; no D/N or other identity is inferred.
- **LED175:** verified Q at W 8 St-NY Aquarium, not an F/G candidate.
- **LED7:** verified Q at Coney Island-Stillwell Av, not an F/G candidate.

The CSV retains each candidate's immutable designator, channel, channel index,
and global physical order. U23 remains the discrete Avi-location indicator
outside the 306 addressable LEDs. LED291 remains an ordinary addressable map
LED.
