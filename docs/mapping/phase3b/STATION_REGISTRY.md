# Phase 3B Station Identity Registry

This registry assigns one stable internal identity to each distinct physical
station entity represented by currently verified Phase 3B station mappings.
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
- `gtfs_parent_stop_id` is blank throughout this initial registry because no
  authoritative parent-stop ID was present in project data. Do not infer one.
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
| Prospect Park | `brighton_line_prospect_park` | Brighton B/Q |
| 15 St-Prospect Park | `culver_line_15_st_prospect_park` | Culver F/G |

In particular, B/Q Prospect Park and F/G 15 St-Prospect Park are distinct
stations despite their similar labels.

## Shared physical stations

The same registry entity is used for multiple route-specific LEDs at these
stations:

- Brighton B/Q: DeKalb Av, Atlantic Av-Barclays Ctr, 7 Av, Prospect Park,
  Church Av, Newkirk Plaza, Kings Hwy, Sheepshead Bay, and Brighton Beach.
- Culver F/G: Church Av, Fort Hamilton Pkwy, 7 Av, and 15 St-Prospect Park.
- Cross-corridor physical stations: W 8 St-NY Aquarium (F/Q) and Coney
  Island-Stillwell Av (F/Q).

This is station identity sharing, not LED sharing: every route-specific map
element retains its own designator and physical reference.

## Master worksheet linkage

Every currently verified station row references `station_id` and
`station_key`. Verified segments retain their readable endpoint names and also
reference `from_station_id` / `to_station_id` (and corresponding endpoint
keys). Both endpoint IDs must resolve to entries in `station_registry.csv`.

The registry is mapping data only. Firmware is unchanged by this phase, and
the logical model's numeric `StationId` remains the intended consumer type.
