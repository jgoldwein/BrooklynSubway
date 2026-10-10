# F/G Route Semantic Review

**Status: human-reviewed assignments promoted.** The 19 F/G station
assignments listed below are `VERIFIED` in the Phase 3B master worksheet.
LED112 remains the previously verified F station. No segment assignments were
invented.

## Candidate counts

The review ledger contains **28 candidates**:

- **VERIFIED:** 19 newly promoted F/G stations.
- **ALREADY_VERIFIED:** 1 — LED112, F at W 8 St-NY Aquarium.
- **AMBIGUOUS:** 6 — LED168, LED169, LED172, LED173, LED166, and LED167.
- **UNRESOLVED:** 0.
- **NOT_FG:** 2 — LED175 and LED7, both verified Q counterparts.
- **New F segments:** 0.
- **New G segments:** 0.

## Verified shared F/G station pairs

Each route-specific LED remains a distinct physical element. The two route
rows at the same physical station share one stable station registry identity:

| Station | F LED | G LED | Station ID / key | Evidence |
|---|---|---|---|---|
| Church Av | LED40 | LED229 | 19 / `culver_line_church_av` | LED40 continues south into the F-only Culver corridor; LED229 is on the G lane that terminates at Church Av. |
| Fort Hamilton Pkwy | LED39 | LED228 | 20 / `culver_line_fort_hamilton_pkwy` | Parallel physical pair continues the same lane orientation north of Church Av. |
| 7 Av | LED38 | LED227 | 21 / `culver_line_7_av` | Repeated F/G lane offset and continuity north from Fort Hamilton Pkwy. |
| 15 St-Prospect Park | LED100 | LED206 | 22 / `culver_line_15_st_prospect_park` | Paired F/G geometry and continuity; this is distinct from B/Q Prospect Park. |

The Brighton B/Q station IDs remain separate for same-name locations: Church
Av uses ID 5 (`brighton_line_church_av`), and 7 Av uses ID 3
(`brighton_line_7_av`). Their physical LEDs are not the F/G pair LEDs.

## Verified assignments

All 19 new rows have confidence `1.00`; station IDs/keys link to
`station_registry.csv`.

### F route

| LED | Station | Station ID / key | Channel:index / global |
|---|---|---|---|
| LED40 | Church Av | 19 / `culver_line_church_av` | 1:2 / 2 |
| LED39 | Fort Hamilton Pkwy | 20 / `culver_line_fort_hamilton_pkwy` | 1:5 / 5 |
| LED38 | 7 Av | 21 / `culver_line_7_av` | 1:7 / 7 |
| LED100 | 15 St-Prospect Park | 22 / `culver_line_15_st_prospect_park` | 1:8 / 8 |
| LED41 | Ditmas Av | 23 / `culver_line_ditmas_av` | 1:1 / 1 |
| LED42 | 18 Av | 24 / `culver_line_18_av` | 1:0 / 0 |
| LED43 | Avenue I | 25 / `culver_line_avenue_i` | 2:12 / 49 |
| LED44 | Bay Pkwy | 26 / `culver_line_bay_pkwy` | 2:13 / 50 |
| LED45 | Avenue N | 27 / `culver_line_avenue_n` | 2:19 / 56 |
| LED46 | Avenue P | 28 / `culver_line_avenue_p` | 2:20 / 57 |
| LED47 | Kings Hwy | 29 / `culver_line_kings_hwy` | 2:21 / 58 |
| LED48 | Avenue U | 30 / `culver_line_avenue_u` | 2:22 / 59 |
| LED49 | Avenue X | 31 / `culver_line_avenue_x` | 2:23 / 60 |
| LED111 | Neptune Av | 32 / `culver_line_neptune_av` | 2:24 / 61 |
| LED165 | Coney Island-Stillwell Av | 34 / `coney_island_stillwell_av` | 2:36 / 73 |

### G route

| LED | Station | Station ID / key | Channel:index / global |
|---|---|---|---|
| LED229 | Church Av | 19 / `culver_line_church_av` | 1:3 / 3 |
| LED228 | Fort Hamilton Pkwy | 20 / `culver_line_fort_hamilton_pkwy` | 1:4 / 4 |
| LED227 | 7 Av | 21 / `culver_line_7_av` | 1:6 / 6 |
| LED206 | 15 St-Prospect Park | 22 / `culver_line_15_st_prospect_park` | 1:9 / 9 |

### Existing verified F station

- **LED112 — F, W 8 St-NY Aquarium**, station ID 33
  (`west_8_st_ny_aquarium`), CH2 index 27, global order 64. This row remains
  unchanged.

## Southern F route sequence

Geographic order from Church Av through the F-only Culver corridor:

1. LED40 — Church Av, ID 19, confidence 1.00.
2. LED41 — Ditmas Av, ID 23, confidence 1.00.
3. LED42 — 18 Av, ID 24, confidence 1.00.
4. LED43 — Avenue I, ID 25, confidence 1.00.
5. LED44 — Bay Pkwy, ID 26, confidence 1.00.
6. LED45 — Avenue N, ID 27, confidence 1.00.
7. LED46 — Avenue P, ID 28, confidence 1.00.
8. LED47 — Kings Hwy, ID 29, confidence 1.00.
9. LED48 — Avenue U, ID 30, confidence 1.00.
10. LED49 — Avenue X, ID 31, confidence 1.00.
11. LED111 — Neptune Av, ID 32, confidence 1.00.
12. LED112 — W 8 St-NY Aquarium, ID 33, already verified.
13. LED165 — Coney Island-Stillwell Av, ID 34, confidence 1.00.

At Coney Island, LED165 `(6555.0, -7370.0)` minus verified Q LED7
`(6640.0, -7450.0)` is `(-85, +80)`. At W 8 St, verified F LED112
`(7195.0, -6770.0)` minus verified Q LED175 `(7280.0, -6855.0)` is
`(-85, +85)`. The repeated lane offset, combined with the manufactured F-lane
continuity into the terminal, supports LED165 as F. LED7 remains Q. This does
not establish the route identities of LED166 or LED167; both remain
unassigned.

No F or G segment is proposed or verified by this review.

## Ambiguous and non-F/G candidates

- LED168, LED169, LED172, and LED173 remain `AMBIGUOUS`: the reviewed artwork
  and coordinates do not establish exact route, station identity, or
  station-versus-segment role.
- LED166 and LED167 remain `AMBIGUOUS` and unassigned terminal LEDs.
- LED175 remains `NOT_FG`: verified Q at W 8 St-NY Aquarium.
- LED7 remains `NOT_FG` in this review: verified Q at Coney Island-Stillwell
  Av.

## Station identity registry

`station_registry.csv` is the authoritative mapping-data identity source.
Same physical stations use one station ID across route-specific LEDs; distinct
physical stations with duplicate display names use distinct IDs and keys.
15 St-Prospect Park on the F/G corridor is not B/Q Prospect Park. GTFS parent
stop IDs remain blank because no authoritative values were available.

U23 remains the discrete Avi-location indicator outside the 306 addressable
LEDs. LED291 remains an ordinary addressable map LED.
