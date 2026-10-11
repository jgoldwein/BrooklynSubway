# Phase 3B Station Identity Registry

This registry assigns one stable internal identity to each distinct physical
station entity represented by currently verified Phase 3B station mappings.
It contains **54 station entities** after the direct-continuity promotion.
`station_id` is a positive 16-bit-compatible integer and `station_key` is a
stable, unique human-readable key. Neither is derived from the display name,
route, LED designator, electrical chain position, nor any future external
transit-feed identifier.

## Identity rules

- IDs are unique to physical station entities and must never be reused.
- Route-specific LEDs at the same physical station share its `station_id` and
  `station_key`.
- Different physical stations with identical or similar display names receive
  different IDs and corridor-qualified keys.
- `display_name` is descriptive only; it is not a key.
- `gtfs_parent_stop_id` is populated only where an authoritative historical
  GTFS mapping supplied a parent-stop ID. Do not infer missing IDs.
- Segment endpoint identity is represented by registry IDs. The descriptive
  `from_station` and `to_station` labels remain for readability, but are not
  identity fields.
- Once published, station IDs and keys are append-only identities: do not
  renumber, recycle, or silently repurpose them.

## Distinct same-name station entities

| Display name | Station key | Physical corridor |
|---|---|---|
| Church Av | `brighton_line_church_av` | Brighton B/Q |
| Church Av | `culver_line_church_av` | Culver F/G |
| 7 Av | `brighton_line_7_av` | Brighton B/Q |
| 7 Av | `culver_line_7_av` | Culver F/G |
| Kings Hwy | `brighton_line_kings_hwy` | Brighton B/Q |
| Kings Hwy | `culver_line_kings_hwy` | Culver F |
| Avenue U | `brighton_line_avenue_u` | Brighton B/Q |
| Avenue U | `culver_line_avenue_u` | Culver F |
| Prospect Park | `brighton_line_prospect_park` | Brighton B/Q/S |
| 15 St-Prospect Park | `culver_line_15_st_prospect_park` | Culver F/G |
| DeKalb Av | `brighton_line_dekalb_av` | Brighton B/Q |
| DeKalb Av | `canarsie_line_dekalb_av` | Canarsie L |
| Atlantic Av-Barclays Ctr | `brighton_line_atlantic_av_barclays_ctr` | Brighton B/Q |
| Atlantic Av | `canarsie_line_atlantic_av` | Canarsie L |

In particular, B/Q/S Prospect Park and F/G 15 St-Prospect Park are distinct
stations despite their similar labels.

## Shared physical stations

The same registry entity is used for multiple route-specific LEDs at these
stations:

- Brighton B/Q: DeKalb Av, Atlantic Av-Barclays Ctr, 7 Av, Church Av,
  Newkirk Plaza, Kings Hwy, Sheepshead Bay, and Brighton Beach.
- Brighton B/Q/S: Prospect Park (B LED184, Q LED51, and S LED185).
- Culver F/G: Church Av, Fort Hamilton Pkwy, 7 Av, and 15 St-Prospect Park.
- Cross-corridor physical stations: W 8 St-NY Aquarium (F/Q) and Coney
  Island-Stillwell Av (F/Q).
- Crosstown G stations: Broadway, Flushing Av, Myrtle-Willoughby Avs,
  Bedford-Nostrand Avs, Classon Av, Clinton-Washington Avs, and Fulton St.
- Bay Ridge R stations: 45 St, 53 St, 77 St, and 86 St.
- Canarsie L stations: DeKalb Av, Halsey St, Wilson Av,
  Bushwick Av-Aberdeen St, Atlantic Av, Sutter Av, Livonia Av, New Lots Av,
  and East 105 St.

## Direct-continuity station entities

The following 20 entities were appended after review of current V1.1 artwork,
accepted physical registration, direct historical designator continuity, and
historical station/service mapping. Parent-stop IDs are copied from the
historical GTFS mapping.

| ID | Display name | Station key | Corridor/service | GTFS parent |
|---:|---|---|---|---|
| 35 | Broadway | `crosstown_line_broadway` | Crosstown G | G30 |
| 36 | Flushing Av | `crosstown_line_flushing_av` | Crosstown G | G31 |
| 37 | Myrtle-Willoughby Avs | `crosstown_line_myrtle_willoughby_avs` | Crosstown G | G32 |
| 38 | Bedford-Nostrand Avs | `crosstown_line_bedford_nostrand_avs` | Crosstown G | G33 |
| 39 | Classon Av | `crosstown_line_classon_av` | Crosstown G | G34 |
| 40 | Clinton-Washington Avs | `crosstown_line_clinton_washington_avs` | Crosstown G | G35 |
| 41 | Fulton St | `crosstown_line_fulton_st` | Crosstown G | G36 |
| 42 | 45 St | `bay_ridge_line_45_st` | Bay Ridge R | R39 |
| 43 | 53 St | `bay_ridge_line_53_st` | Bay Ridge R | R40 |
| 44 | 77 St | `bay_ridge_line_77_st` | Bay Ridge R | R43 |
| 45 | 86 St | `bay_ridge_line_86_st` | Bay Ridge R | R44 |
| 46 | DeKalb Av | `canarsie_line_dekalb_av` | Canarsie L | L16 |
| 47 | Halsey St | `canarsie_line_halsey_st` | Canarsie L | L19 |
| 48 | Wilson Av | `canarsie_line_wilson_av` | Canarsie L | L20 |
| 49 | Bushwick Av-Aberdeen St | `canarsie_line_bushwick_av_aberdeen_st` | Canarsie L | L21 |
| 50 | Atlantic Av | `canarsie_line_atlantic_av` | Canarsie L | L24 |
| 51 | Sutter Av | `canarsie_line_sutter_av` | Canarsie L | L25 |
| 52 | Livonia Av | `canarsie_line_livonia_av` | Canarsie L | L26 |
| 53 | New Lots Av | `canarsie_line_new_lots_av` | Canarsie L | L27 |
| 54 | East 105 St | `canarsie_line_east_105_st` | Canarsie L | L28 |

This is station identity sharing, not LED sharing: every route-specific map
element retains its own designator and physical reference.

## Master worksheet linkage

Every currently verified station row references `station_id` and
`station_key`. Verified segments retain their readable endpoint names and also
reference `from_station_id` / `to_station_id` (and corresponding endpoint
keys). Both endpoint IDs must resolve to entries in `station_registry.csv`.

The registry is mapping data only. Firmware is unchanged by this phase, and
the logical model's numeric `StationId` remains the intended consumer type.
