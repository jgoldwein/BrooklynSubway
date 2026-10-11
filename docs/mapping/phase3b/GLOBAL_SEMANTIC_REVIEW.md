# Phase 3B Global Semantic Review

**Status: review-only; no semantics were promoted.** The CSV contains one row
for every addressable map LED, copies all current verified assignments as
`ALREADY_VERIFIED`, and records the strongest new proposals supported by the
reviewed manufactured artwork and exact PCB coordinates. The master worksheet,
station registry, firmware, and manufacturing sources were not modified.

## Result

- Total map LEDs: **302**
- `ALREADY_VERIFIED`: **51**
- `PROPOSED_HIGH`: **21**
- `PROPOSED_MEDIUM`: **11**
- `AMBIGUOUS`: **23**
- `UNRESOLVED`: **196**
- `NOT_APPLICABLE`: **0**
- New segment proposals: **0**
- New station proposals: **32**
- Additional named station-entity candidates retained as ambiguous: **5**
- New station keys needed if the proposals/candidates are accepted: **37**

Of the 251 currently unverified map LEDs, **32 (12.75%)** received a usable
station proposal. If all high-confidence proposals were accepted, 72 of 302
map LEDs would be semantically classified (**23.84%**). If both high- and
medium-confidence proposals were accepted, 83 of 302 would be classified
(**27.48%**).

This pass did **not** reduce the unresolved population as far as intended:
196 rows remain `UNRESOLVED`, and another 23 remain `AMBIGUOUS`. Medium
proposals also require human acceptance before promotion. These counts are
reported rather than being inflated with route or station guesses.

## Route summary

| Route / service | Already verified | New high stations | New medium stations | New segments | Ambiguous rows |
|---|---:|---:|---:|---:|---:|
| A | 0 | 14 | 2 | 0 | 2 paired with S at Broad Channel |
| B | 9 | 0 | 0 | 0 | Included in B/Q junction candidates |
| C | 0 | 0 | 0 | 0 | No individual C assignment established |
| D | 0 | 0 | 0 | 0 | No individual D assignment established |
| E | 0 | 0 | 0 | 0 | No individual E assignment established |
| F | 16 | 0 | 0 | 0 | Northern F/G candidates remain |
| G | 4 | 0 | 0 | 0 | Northern F/G candidates remain |
| J | 0 | 0 | 0 | 0 | J/Z service lanes not separated |
| L | 0 | 1 | 9 | 0 | — |
| M | 0 | 0 | 0 | 0 | No individual M assignment established |
| N | 0 | 0 | 0 | 0 | No individual N assignment established |
| Q | 22 | 0 | 0 | 0 | Included in B/Q junction candidates |
| R | 0 | 6 | 0 | 0 | — |
| W | 0 | 0 | 0 | 0 | No individual W assignment established |
| Z | 0 | 0 | 0 | 0 | J/Z service lanes not separated |
| 2 | 0 | 0 | 0 | 0 | No individual 2 assignment established |
| 3 | 0 | 0 | 0 | 0 | No individual 3 assignment established |
| 4 | 0 | 0 | 0 | 0 | No individual 4 assignment established |
| 5 | 0 | 0 | 0 | 0 | No individual 5 assignment established |
| Rockaway Park Shuttle (S) | 0 | 0 | 0 | 0 | 8 paired A/S terminal-branch rows |

The 51 existing verified rows remain fixed: B=9, Q=22, F=16, G=4. No new
route assignment was made for the other displayed services because the review
did not establish an individual LED-to-route identity confidently enough.
These zeros mean **no attributable proposal**, not that the service is absent
from the manufactured map. The corresponding physical rows remain
`AMBIGUOUS` or `UNRESOLVED`.

### New station proposals

- **A — high:** Grant Av; 80 St; 88 St; Rockaway Blvd; 104 St; 111 St;
  Ozone Park-Lefferts Blvd; Howard Beach-JFK Airport; Far Rockaway-Mott Av;
  Beach 25 St; Beach 36 St; Beach 44 St; Beach 60 St; Beach 67 St.
- **A — medium:** Aqueduct Racetrack; Aqueduct-North Conduit Av.
- **L — high:** Canarsie-Rockaway Pkwy.
- **L — medium:** DeKalb Av; Myrtle-Wyckoff Avs; Halsey St; Wilson Av;
  Bushwick Av-Aberdeen St; Atlantic Av; Sutter Av; Livonia Av; East 105 St.
- **R — high:** 45 St; 53 St; 59 St; 77 St; 86 St; Bay Ridge-95 St.

All proposed stations have blank `station_id` values and corridor-qualified
`station_key` values. The five additional ambiguous entity keys are Broad
Channel, Beach 90 St, Beach 98 St, Beach 105 St, and Rockaway Park-Beach 116
St. Paired review rows for a shared candidate station use the same key.

## Ambiguities and unresolved geographic clusters

### Explicitly ambiguous rows

- **Northern Brooklyn / Culver transition:** LED168, LED169, LED172, LED173.
  These lie near the F/G corridor and its northern connections; route and
  station-versus-segment role are not established.
- **Prospect Park–7 Av:** LED290. It is a possible Q-side corridor element,
  but route, element type, and endpoints remain uncertain.
- **Atlantic Av-Barclays Ctr / DeKalb Av convergence:** LED159, LED158,
  LED136, LED157, LED161, LED191. The likely neighboring B/Q lanes do not
  establish each marker's station/segment/interchange role.
- **Coney Island-Stillwell Av:** LED166 and LED167 remain separate,
  unassigned terminal markers.
- **Rockaway branches:** LED269/LED270 at Broad Channel and LED277–LED284
  at Beach 90, Beach 98, Beach 105, and Rockaway Park-Beach 116 St. Station
  candidates are named, but individual A versus S lane membership is not
  established.

### Remaining unresolved clusters

The 196 unresolved rows are concentrated in the other northern and central
Brooklyn corridors, the Broadway Junction/Brownsville/Canarsie network,
Downtown Brooklyn and its route merges, the southern 4th Avenue/West End/Sea
Beach approaches, and connecting Queens/Rockaway corridors. The CSV preserves
each LED's exact designator, channel, channel index, physical order, and
coordinates for continued review. No segment was proposed because the reviewed
evidence did not establish both a train-position role and two defensible
same-route station endpoints for any new marker.

## Station identity and shared entities

The review uses the existing registry's numeric `station_id` and stable
`station_key` for every existing verified station. It does not use
`display_name` as identity. Four existing display names intentionally map to
different physical station entities:

- **7 Av:** ID 3 `brighton_line_7_av`; ID 21 `culver_line_7_av`.
- **Church Av:** ID 5 `brighton_line_church_av`; ID 19
  `culver_line_church_av`.
- **Kings Hwy:** ID 7 `brighton_line_kings_hwy`; ID 29
  `culver_line_kings_hwy`.
- **Avenue U:** ID 15 `brighton_line_avenue_u`; ID 30
  `culver_line_avenue_u`.

Existing verified multi-route station entities retain shared IDs:

- DeKalb Av (ID 1): Q LED190, B LED189.
- Atlantic Av-Barclays Ctr (ID 2): Q LED163, B LED160.
- 7 Av (Brighton, ID 3): B LED230, Q LED50.
- Prospect Park (ID 4): B LED185, Q LED184.
- Church Av (Brighton, ID 5): Q LED53, B LED231.
- Newkirk Plaza (ID 6): Q LED56, B LED232.
- Kings Hwy (Brighton, ID 7): B LED233, Q LED60.
- Sheepshead Bay (ID 8): B LED207, Q LED114.
- Brighton Beach (ID 17): B LED208, Q LED115.
- Church Av (Culver, ID 19): F LED40, G LED229.
- Fort Hamilton Pkwy (ID 20): G LED228, F LED39.
- 7 Av (Culver, ID 21): G LED227, F LED38.
- 15 St-Prospect Park (ID 22): F LED100, G LED206.
- W 8 St-NY Aquarium (ID 33): Q LED175, F LED112.
- Coney Island-Stillwell Av (ID 34): F LED165, Q LED7.

Verified segment endpoints still use registry identity fields:

- LED52, Q: Parkside Av (ID 11,
  `brighton_line_parkside_av`) → Church Av (ID 5,
  `brighton_line_church_av`).
- LED113, Q: Neck Rd (ID 16, `brighton_line_neck_rd`) → Sheepshead Bay
  (ID 8, `brighton_line_sheepshead_bay`).

## Validation

- Exactly **302** map rows are present; all **51** existing verified rows are
  represented as `ALREADY_VERIFIED`.
- All 302 designators, channels, channel indices, global physical orders, and
  PCB X/Y coordinates match the physical chain and position sources.
- No duplicate designator, channel/index pair, or global physical order was
  found.
- Every verified semantic field, including station and segment endpoint
  identities, matches the master worksheet.
- Both verified segment endpoints resolve to the station registry.
- The 302-row ledger excludes all four CH8 status LEDs. U23 remains separate
  from the addressable inventory; LED291 remains an ordinary map LED and is
  included in the ledger.
- No GTFS parent-stop IDs were added or guessed.
