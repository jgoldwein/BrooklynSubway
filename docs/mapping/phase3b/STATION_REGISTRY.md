# Phase 3B Station Identity Registry

This registry assigns one stable internal identity to each distinct physical
station entity represented by currently verified Phase 3B station mappings.
It contains **188 station entities** after completion of Phase 3B semantic
identification.
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
| Fort Hamilton Pkwy | `culver_line_fort_hamilton_pkwy` | Culver F/G |
| Fort Hamilton Pkwy | `west_end_line_fort_hamilton_pkwy` | West End D |
| Fort Hamilton Pkwy | `sea_beach_line_fort_hamilton_pkwy` | Sea Beach N |

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
- Fourth Avenue D/N/R: 36 St.
- Fourth Avenue N/R: 59 St.
- Bay Ridge R: 25 St.
- West End D: 9 Av, Fort Hamilton Pkwy, 50 St, and 55 St.
- Sea Beach N: 8 Av, Fort Hamilton Pkwy, and New Utrecht Av.
- Canarsie L stations: DeKalb Av, Halsey St, Wilson Av,
  Bushwick Av-Aberdeen St, Atlantic Av, Sutter Av, Livonia Av, New Lots Av,
  and East 105 St.
- Broadway J/M/Z: Lorimer St, Flushing Av, and Myrtle Av.
- Broadway J/M/Z: Marcy Av.

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

## Remaining-high-proposal station entities

The following 18 entities were appended after review of current V1.1 artwork,
accepted projected centers, historical route/station evidence where available,
and route/branch topology. The Aqueduct Racetrack and Aqueduct-North Conduit Av
proposals remain unverified and are not included.

| ID | LED | Display name | Station key | Corridor/service | GTFS parent |
|---:|---|---|---|---|---|
| 55 | LED186 | Grant Av | `a_line_grant_av` | A | — |
| 56 | LED260 | 80 St | `a_line_80_st` | A | — |
| 57 | LED261 | 88 St | `a_line_88_st` | A | — |
| 58 | LED262 | Rockaway Blvd | `a_line_rockaway_blvd` | A | — |
| 59 | LED263 | 104 St | `a_line_104_st` | A | — |
| 60 | LED264 | 111 St | `a_line_111_st` | A | — |
| 61 | LED265 | Ozone Park-Lefferts Blvd | `a_line_ozone_park_lefferts_blvd` | A | — |
| 62 | LED268 | Howard Beach-JFK Airport | `a_line_howard_beach_jfk_airport` | A | — |
| 63 | LED271 | Far Rockaway-Mott Av | `a_line_far_rockaway_mott_av` | A | — |
| 64 | LED272 | Beach 25 St | `rockaway_beach_25_st` | A | — |
| 65 | LED273 | Beach 36 St | `rockaway_beach_36_st` | A | — |
| 66 | LED274 | Beach 44 St | `rockaway_beach_44_st` | A | — |
| 67 | LED275 | Beach 60 St | `rockaway_beach_60_st` | A | — |
| 68 | LED276 | Beach 67 St | `rockaway_beach_67_st` | A | — |
| 69 | LED8 | Canarsie-Rockaway Pkwy | `canarsie_line_canarsie_rockaway_pkwy` | Canarsie L | L29 |
| 70 | LED86 | Myrtle-Wyckoff Avs | `canarsie_line_myrtle_wyckoff_avs` | Canarsie L | L17 |
| 71 | LED6 | Bay Ridge-95 St | `bay_ridge_line_bay_ridge_95_st` | Bay Ridge R | R45 |
| 72 | LED23 | Bay Ridge Av | `bay_ridge_line_bay_ridge_av` | Bay Ridge R | R42 |

The 14 A-line entities are supported by the current V1.1 artwork, accepted
projected centers, and route/branch topology; historical designator continuity
is not claimed for them. The two Aqueduct candidates remain outside the
verified registry until their package-to-station assignments are resolved.

## Human-confirmed Bay Ridge and neighboring station entities

The following identities were appended after human review of the CPL-derived
positions on the final V1.1 artwork. Route-specific LEDs at the same physical
station entity share a registry ID; distinct stations with the same display
name remain separate. GTFS parent IDs are copied from the authoritative
historical source where available.

| ID | LED(s) | Display name | Station key | Corridor/service | GTFS parent |
|---:|---|---|---|---|---|
| 73 | LED20 | 25 St | `bay_ridge_line_25_st` | Bay Ridge R | R35 |
| 74 | LED26 | 9 Av | `west_end_line_9_av` | West End D | B12 |
| 75 | LED27 | Fort Hamilton Pkwy | `west_end_line_fort_hamilton_pkwy` | West End D | B13 |
| 76 | LED28 | 50 St | `west_end_line_50_st` | West End D | B14 |
| 77 | LED29 | 55 St | `west_end_line_55_st` | West End D | B15 |
| 78 | LED101, LED172, LED173 | 36 St | `fourth_avenue_line_36_st` | Fourth Avenue D/N/R | R36 |
| 79 | LED102, LED174 | 59 St | `fourth_avenue_line_59_st` | Fourth Avenue N/R | R41 |
| 80 | LED103 | 8 Av | `sea_beach_line_8_av` | Sea Beach N | N02 |
| 81 | LED104 | Fort Hamilton Pkwy | `sea_beach_line_fort_hamilton_pkwy` | Sea Beach N | N03 |
| 82 | LED171 | New Utrecht Av | `sea_beach_line_new_utrecht_av` | Sea Beach N | N04 |

LED21 (45 St, R) remains unchanged.

## Additional human-confirmed transfer and corridor entities

These identities were appended from human-confirmed readings of the CPL-derived
final V1.1 artwork. Related route-specific LEDs at one physical station or
transfer complex share an identity. No GTFS parent IDs were added where the
authoritative historical source did not already provide one.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 83 | LED148, LED149, LED150, LED151 | Broadway Junction | `broadway_junction` | A/C/J/Z/L | — |
| 84 | LED152, LED153, LED154 | Jay St-MetroTech | `jay_st_metrotech` | A/F/R | — |
| 85 | LED155, LED156 | Hoyt-Schermerhorn Sts | `hoyt_schermerhorn_sts` | A/G | — |
| 86 | LED168, LED169 | 4 Av-9 St | `fourth_avenue_4_av_9_st` | F/G | F23 |
| 87 | LED176 | Franklin Av-Fulton St | `franklin_av_fulton_st` | S | — |
| 88 | LED177, LED178, LED179 | Borough Hall | `borough_hall` | 2/4/5 | — |
| 89 | LED180, LED181, LED182 | Franklin Av-Medgar Evers College | `franklin_av_medgar_evers_college` | 2/4/5 | — |
| 90 | LED183 | Botanic Garden | `botanic_garden` | S | — |
| 91 | LED147 | Newkirk Av-Little Haiti | `newkirk_av_little_haiti` | 2/5 | — |
| 92 | LED266 | Aqueduct Racetrack | `a_line_aqueduct_racetrack` | A | — |
| 93 | LED267 | Aqueduct-North Conduit Av | `a_line_aqueduct_north_conduit_av` | A | — |
| 94 | LED128 | Liberty Av | `fulton_line_liberty_av` | C | A52 |
| 95 | LED129 | Van Siclen Av | `fulton_line_van_siclen_av` | C | A53 |
| 96 | LED130 | Shepherd Av | `fulton_line_shepherd_av` | C | A54 |
| 97 | LED131, LED212 | Euclid Av | `euclid_av` | A/C | A55 |
| 98 | LED205 | New Lots Av | `new_lots_av_3` | 3 | — |
| 99 | LED243 | Alabama Av | `broadway_alabama_av` | Z | J24 |
| 100 | LED244 | Van Siclen Av | `broadway_van_siclen_av` | J/Z | J23 |
| 101 | LED204 | Van Siclen Av | `irt_van_siclen_av` | 3 | — |

Existing entities were reused for the other confirmed LEDs: station 2 for
Atlantic Av-Barclays Ctr (LED157, LED158, LED159, LED161, LED162, LED164);
station 34 for Coney Island-Stillwell Av (LED166, LED167); station 70 for
Myrtle-Wyckoff Avs (LED170); and station 49 for Bushwick Av-Aberdeen St
(LED89). LED299's previous station interpretation at this entity was
superseded by Joel's final-artwork confirmation; LED299 is now an L segment.
GTFS parent IDs above are included only where the historical
station/service catalog supplies an ID applicable to that entity.

LED203 is verified as 3 / Pennsylvania Av after human re-review corrected the
earlier C-route claim. Its CPL-projected position is on the red 3 line at the
printed Pennsylvania Av label.

## Downtown Brooklyn manual-review entities

These six entities were appended after manual confirmation against the
CPL-derived final V1.1 artwork. Route-specific LEDs at the same station share
one registry identity. The two newly confirmed segment LEDs are not station
entities and are therefore not listed here.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 102 | LED10 | Court St | `bay_ridge_line_court_st` | R | R28 |
| 103 | LED117, LED209 | High St | `high_st` | A/C | A40 |
| 104 | LED132, LED213 | Clark St | `irt_clark_st` | 2/3 | — |
| 105 | LED134 | Hoyt St | `irt_hoyt_st` | 3 | — |
| 106 | LED135, LED215, LED217 | Nevins St | `irt_nevins_st` | 2/3/4/5 | — |
| 107 | Segment LED294 endpoint only | York St | `culver_line_york_st` | F | — |

LED118 reuses station 84 (Jay St-MetroTech), and LED133 reuses station 88
(Borough Hall). Segment LED293 references station 84 (Jay St-MetroTech) and
station 1 (DeKalb Av); segment LED294 references station 84 and station 107
(York St). No station registry IDs are assigned to segment LEDs themselves;
station 107 represents the actual York St endpoint, not LED294.

## J/Z/M, L, and numbered-line human confirmations

The following station identities were confirmed against the CPL-derived final
V1.1 artwork. Route-specific LEDs at Lorimer St, Flushing Av, and Myrtle Av
share the same physical station entity where the J, M, and Z alignments meet.
The Z segment LEDs reference their station endpoints; neither segment is a
station entity.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 108 | LED203 | Pennsylvania Av | `irt_pennsylvania_av` | IRT New Lots 3 | — |
| 109 | LED64, LED237 | Lorimer St | `broadway_lorimer_st` | Broadway J/M/Z | — |
| 110 | LED65, LED238 | Flushing Av | `broadway_flushing_av` | Broadway J/M/Z | — |
| 111 | LED66, LED239, LED240 | Myrtle Av | `broadway_myrtle_av` | Broadway J/M/Z | — |
| 112 | LED67 | Kosciuszko St | `broadway_kosciuszko_st` | Broadway J | — |
| 113 | LED83 | Morgan Av | `canarsie_morgan_av` | Canarsie L | — |
| 114 | LED84 | Jefferson St | `canarsie_jefferson_st` | Canarsie L | — |
| 115 | LED192 | Central Av | `myrtle_line_central_av` | Myrtle Ave M | — |
| 116 | LED193 | Knickerbocker Av | `myrtle_line_knickerbocker_av` | Myrtle Ave M | — |
| 117 | Segment LED296 endpoint only | Gates Av | `broadway_gates_av` | Broadway J/Z | — |
| 118 | Segment LED298 endpoint only | Marcy Av | `broadway_marcy_av` | Broadway J/Z | — |

LED71 reuses station 83 (Broadway Junction), LED72 reuses station 99
(Alabama Av), and LED73 reuses station 100 (Van Siclen Av). Segment LED296
references station 111 (Myrtle Av) and station 117 (Gates Av). Segment LED298
references station 118 (Marcy Av) and station 111 (Myrtle Av). No GTFS parent
IDs were added without an authoritative source.

The three Broadway J station LEDs confirmed in the next review reuse station
117 for Gates Av and append the following route-specific entities:

| ID | LED | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 119 | LED69 | Halsey St | `broadway_halsey_st` | Broadway J | — |
| 120 | LED70 | Chauncey St | `broadway_chauncey_st` | Broadway J | — |

LED68 reuses station 117 (Gates Av), previously introduced as an endpoint-only
entity for segment LED296. The existing Canarsie L Halsey St entity is distinct
from Broadway J Halsey St. No GTFS parent IDs were added.

The 13 human-confirmed Eastern Brooklyn station LEDs reuse a shared entity for
each route-specific pair at President St-Medgar Evers College, Sterling St,
Winthrop St, Church Av, Beverly Rd, and Flatbush Av-Brooklyn College. LED250
reuses the existing Newkirk Av-Little Haiti entity, station 91. The segment
LED302 runs from Franklin Av-Medgar Evers College (station 89) to Crown
Hts-Utica Av (station 127). The new station entities are:

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 121 | LED141, LED221 | President St-Medgar Evers College | `irt_president_st_medgar_evers_college` | IRT Eastern Pkwy 2/3/4/5 | — |
| 122 | LED142, LED222 | Sterling St | `irt_sterling_st` | IRT Eastern Pkwy 2/3/4/5 | — |
| 123 | LED143, LED223 | Winthrop St | `irt_winthrop_st` | IRT Eastern Pkwy 2/3/4/5 | — |
| 124 | LED144, LED224 | Church Av | `irt_church_av` | IRT Eastern Pkwy 2/5 | — |
| 125 | LED145, LED225 | Beverly Rd | `irt_beverly_rd` | IRT Eastern Pkwy 2/5 | — |
| 126 | LED146, LED226 | Flatbush Av-Brooklyn College | `irt_flatbush_av_brooklyn_college` | IRT Eastern Pkwy 2/5 | — |
| 127 | LED302 endpoint only | Crown Hts-Utica Av | `irt_crown_hts_utica_av` | IRT Eastern Pkwy 3/4 | — |

LED302 remains a segment and has no station identity of its own. No GTFS parent
IDs were added without authoritative source data.

## Latest human-confirmed J/Z, L, and IRT Eastern Parkway entities

The 17 human-confirmed station LEDs added after review of CPL-derived
positions on the final V1.1 artwork reuse the same registry identity for
route-specific LEDs at a shared physical station. These rows add 12 new
entities; no GTFS parent IDs were inferred.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 128 | LED74 | Cleveland St | `broadway_cleveland_st` | Broadway J | — |
| 129 | LED75, LED245 | Norwood Av | `broadway_norwood_av` | Broadway J/Z | — |
| 130 | LED76, LED246 | Crescent St | `broadway_crescent_st` | Broadway J/Z | — |
| 131 | LED77 | Cypress Hills | `broadway_cypress_hills` | Broadway J | — |
| 132 | LED78 | Bedford Av | `canarsie_bedford_av` | Canarsie L | — |
| 133 | LED79 | Lorimer St | `canarsie_lorimer_st` | Canarsie L | — |
| 134 | LED80 | Graham Av | `canarsie_graham_av` | Canarsie L | — |
| 135 | LED81 | Grand St | `canarsie_grand_st` | Canarsie L | — |
| 136 | LED82 | Montrose Av | `canarsie_montrose_av` | Canarsie L | — |
| 137 | LED137, LED218 | Bergen St | `irt_bergen_st` | IRT Eastern Pkwy 2/3 | — |
| 138 | LED138, LED219 | Grand Army Plaza | `irt_grand_army_plaza` | IRT Eastern Pkwy 2/3 | — |
| 139 | LED139, LED220 | Eastern Pkwy-Brooklyn Museum | `irt_eastern_pkwy_brooklyn_museum` | IRT Eastern Pkwy 2/3 | — |

Canarsie L Lorimer St (ID 133) is a distinct physical station from Broadway
J/M Lorimer St. Canarsie L Grand St (ID 135) is distinct from the Manhattan
Grand St station. Route-specific physical LEDs remain separate worksheet
rows. The three IRT station pairs share entities because each pair identifies
the same physical station on the 2 and 3 routes.

## Master worksheet linkage

Every currently verified station row references `station_id` and
`station_key`. Verified segments retain their readable endpoint names and also
reference `from_station_id` / `to_station_id` (and corresponding endpoint
keys). Both endpoint IDs must resolve to entries in `station_registry.csv`.

The registry is mapping data only. Firmware is unchanged by this phase, and
the logical model's numeric `StationId` remains the intended consumer type.

## A/C Fulton, J/Z/M, and IRT human-confirmed stations

These 14 route-specific station LEDs were confirmed from the CPL-derived final
V1.1 artwork. The A and C LEDs at Utica Av share one physical station entity;
Z Gates Av reuses the existing J/Z Gates Av identity; and both 3 and 4
Crown Hts-Utica Av LEDs reuse station 127. Ten genuinely new station entities
were appended. No GTFS parent IDs were added.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 140 | LED124 | Kingston-Throop Avs | `fulton_line_kingston_throop_avs` | IND Fulton C | — |
| 141 | LED125, LED211 | Utica Av | `fulton_line_utica_av` | IND Fulton A/C | — |
| 142 | LED126 | Ralph Av | `fulton_line_ralph_av` | IND Fulton C | — |
| 143 | LED127 | Rockaway Av | `fulton_line_rockaway_av` | IND Fulton C | — |
| 144 | LED253 | Forest Av | `broadway_forest_av` | Broadway M | — |
| 145 | LED254 | Seneca Av | `broadway_seneca_av` | Broadway M | — |
| 146 | LED195 | Nostrand Av | `irt_nostrand_av` | IRT Eastern Pkwy 3 | — |
| 147 | LED196 | Kingston Av | `irt_kingston_av` | IRT Eastern Pkwy 3 | — |
| 148 | LED199 | Sutter Av-Rutland Rd | `irt_sutter_av_rutland_rd` | IRT Eastern Pkwy 3 | — |
| 149 | LED200 | Saratoga Av | `irt_saratoga_av` | IRT Eastern Pkwy 3 | — |

LED241 (Z / Gates Av) reuses station 117. LED197 and LED198 (4 and 3 / Crown
Hts-Utica Av) reuse station 127.

## A/C Fulton, Shuttle, and route-specific confirmation additions

The following 10 station LEDs were human-confirmed from the CPL-derived final
V1.1 artwork. Four new station identities were appended; existing identities
were reused when the route-specific LED denotes the same physical station.
The C Fulton corridor entities are intentionally distinct from same-named
stations on the G and IRT corridors.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 85 | LED119 | Hoyt-Schermerhorn Sts | `hoyt_schermerhorn_sts` | A/C/G | — |
| 89 | LED140 | Franklin Av-Medgar Evers College | `franklin_av_medgar_evers_college` | IRT Eastern Pkwy 2/3/4/5 | — |
| 105 | LED214 | Hoyt St | `irt_hoyt_st` | IRT 2/3 | — |
| 106 | LED216 | Nevins St | `irt_nevins_st` | IRT 2/3/4/5 | — |
| 120 | LED242 | Chauncey St | `broadway_chauncey_st` | Broadway J/Z | — |
| 150 | LED121 | Clinton-Washington Avs | `fulton_line_clinton_washington_avs` | IND Fulton C | — |
| 151 | LED122 | Franklin Av | `fulton_line_franklin_av` | IND Fulton C | — |
| 152 | LED123, LED210 | Nostrand Av | `fulton_line_nostrand_av` | IND Fulton A/C | — |
| 153 | LED194 | Park Pl | `franklin_shuttle_park_pl` | Franklin Av Shuttle S | — |

No GTFS parent IDs were added. The corrected human-confirmed mappings
supersede the former inference-only labels: LED121 is C /
Clinton-Washington Avs and LED122 is C / Franklin Av.

## Downtown and Rockaway human-confirmed additions

The following 13 LEDs were human-confirmed from the CPL-derived final V1.1
artwork. Existing station entities were reused at Atlantic Av-Barclays Ctr
(ID 2) and DeKalb Av (ID 1). The A and Rockaway Park Shuttle LEDs at each
shared Rockaway stop use one physical station identity. Five new physical
station entities were appended; LED290 is a segment and has no station
identity. No GTFS parent IDs were added.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 2 | LED136 | Atlantic Av-Barclays Ctr | `brighton_line_atlantic_av_barclays_ctr` | IRT Eastern Pkwy 3 / transfer complex | — |
| 1 | LED191 | DeKalb Av | `brighton_line_dekalb_av` | R / transfer complex | — |
| 154 | LED269, LED270 | Broad Channel | `rockaway_line_broad_channel` | IND Rockaway A/S | — |
| 155 | LED277, LED278 | Beach 90 St | `rockaway_line_beach_90_st` | IND Rockaway A/S | — |
| 156 | LED279, LED280 | Beach 98 St | `rockaway_line_beach_98_st` | IND Rockaway A/S | — |
| 157 | LED281, LED282 | Beach 105 St | `rockaway_line_beach_105_st` | IND Rockaway A/S | — |
| 158 | LED283, LED284 | Rockaway Park-Beach 116 St | `rockaway_line_rockaway_park_beach_116_st` | IND Rockaway A/S | — |

LED290 is HUMAN_CONFIRMED as the Q segment from 7 Av (station 3) to Prospect
Park (station 4). It reuses those endpoint IDs and does not create a registry
entity.

## Fourth Avenue, West End, Crosstown, Culver, Sea Beach, and Fulton confirmations

The following 31 station LEDs were human-confirmed from the manufactured CPL,
accepted projection, and final V1.1 artwork. Twenty-three new station
identities were appended as IDs 159-181. The R / 4 Av-9 St LED reuses station
86, and the J/Z/M Marcy Av LEDs reuse station 118. Route-specific F/G LEDs
share the same physical station entities at Bergen St, Carroll St, and
Smith-9 Sts; the J/M LEDs share the Hewes St entity. No GTFS parent IDs were
added.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 86 | LED18 | 4 Av-9 St | `fourth_avenue_4_av_9_st` | 4 Av-9 St F/G/R | — |
| 118 | LED62, LED234, LED235 | Marcy Av | `broadway_marcy_av` | Broadway J/M/Z | — |
| 159 | LED17 | Union St | `fourth_avenue_union_st` | Fourth Avenue R | — |
| 160 | LED19 | Prospect Av | `fourth_avenue_prospect_av` | Fourth Avenue R | — |
| 161 | LED30 | 62 St | `west_end_62_st` | West End D | — |
| 162 | LED31 | 71 St | `west_end_71_st` | West End D | — |
| 163 | LED32 | 79 St | `west_end_79_st` | West End D | — |
| 164 | LED33 | 18 Av | `west_end_18_av` | West End D | — |
| 165 | LED34 | 20 Av | `west_end_20_av` | West End D | — |
| 166 | LED35 | Bay Pkwy | `west_end_bay_pkwy` | West End D | — |
| 167 | LED36 | 25 Av | `west_end_25_av` | West End D | — |
| 168 | LED37 | Bay 50 St | `west_end_bay_50_st` | West End D | — |
| 169 | LED95 | Nassau Av | `crosstown_nassau_av` | Crosstown G | — |
| 170 | LED96 | Lorimer St | `crosstown_lorimer_st` | Crosstown G | — |
| 171 | LED97, LED247 | Bergen St | `culver_line_bergen_st` | Culver F/G | — |
| 172 | LED98, LED248 | Carroll St | `culver_line_carroll_st` | Culver F/G | — |
| 173 | LED99, LED249 | Smith-9 Sts | `culver_line_smith_9_sts` | Culver F/G | — |
| 174 | LED105 | 18 Av | `sea_beach_18_av` | Sea Beach N | — |
| 175 | LED106 | 20 Av | `sea_beach_20_av` | Sea Beach N | — |
| 176 | LED107 | Bay Pkwy | `sea_beach_bay_pkwy` | Sea Beach N | — |
| 177 | LED108 | Kings Hwy | `sea_beach_kings_hwy` | Sea Beach N | — |
| 178 | LED109 | Avenue U | `sea_beach_avenue_u` | Sea Beach N | — |
| 179 | LED110 | 86 St | `sea_beach_86_st` | Sea Beach N | — |
| 180 | LED63, LED236 | Hewes St | `broadway_hewes_st` | Broadway J/M | — |
| 181 | LED120 | Lafayette Av | `fulton_line_lafayette_av` | IND Fulton C | — |

LED96's human-confirmed G / Lorimer St identity supersedes the historical
G / Metropolitan Av (G29) value. The station-registry and GTFS naming
inconsistency is flagged for review; the human assignment remains authoritative
and no GTFS ID was inferred. The same-name West End D, Culver F, and Sea Beach
N stations are distinct registry entities.

## Earlier confirmed segments and endpoint-only identities

The 13 newly confirmed segments raise the verified segment total to 20. Three
endpoint identities were appended as IDs 182-184; these are registry entities,
not new LED station assignments. All endpoints reuse existing station IDs
where available.

| LED | Route | Segment | Endpoint IDs |
|---|---|---|---|
| LED305 | S | Broad Channel -> Beach 90 St | 154 -> 155 |
| LED308 | D | Grand St -> Atlantic Av-Barclays Ctr | 182 -> 2 |
| LED309 | B | Grand St -> DeKalb Av | 182 -> 1 |
| LED310 | Q | Canal St -> DeKalb Av | 183 -> 1 |
| LED311 | N | Canal St -> Atlantic Av-Barclays Ctr | 183 -> 2 |
| LED291 | F | York St -> East Broadway | 107 -> 184 |
| LED292 | N | Atlantic Av-Barclays Ctr -> 36 St | 2 -> 78 |
| LED295 | G | Lorimer St -> Broadway | 170 -> 35 |
| LED297 | M | Hewes St -> Lorimer St | 180 -> 109 |
| LED300 | 2 | Franklin Av-Medgar Evers College -> President St-Medgar Evers College | 89 -> 121 |
| LED301 | 3 | Franklin Av-Medgar Evers College -> Nostrand Av | 89 -> 146 |
| LED304 | S | Botanic Garden -> Park Pl | 90 -> 153 |
| LED303 | 5 | Atlantic Av-Barclays Ctr -> Franklin Av-Medgar Evers College | 2 -> 89 |

For the Manhattan Bridge set, the human-confirmed route assignments are
LED308=D and LED309=B, reversing the older bridge-audit values; LED310=Q and
LED311=N. Their endpoints follow the accepted final-map topology. LED291's
human-confirmed F assignment supersedes the old D assignment. LED304 remains
Botanic Garden -> Park Pl as explicitly confirmed, despite its proximity to
the Franklin Av-Medgar Evers College complex. These historical conflicts are
superseded, not applied to current semantics.

## Final 13 human-confirmed mappings

The final 13 assignments complete the 302-map-LED semantic set. Four
append-only station entities were needed; every other station and segment
endpoint reuses an existing identity. These additions contain no GTFS IDs.

| ID | LED(s) | Display name | Station key | Corridor/services | GTFS parent |
|---:|---|---|---|---|---|
| 185 | LED255-LED258 | Delancey St–Essex St | `manhattan_bridge_delancey_st` | IND Sixth Avenue F / Broadway J/M/Z | — |
| 186 | LED187 | Greenpoint Av | `crosstown_greenpoint_av` | Crosstown G | — |
| 187 | LED201 | Rockaway Av | `irt_rockaway_av` | IRT Eastern Pkwy 3 | — |
| 188 | LED202 | Junius St | `irt_junius_st` | IRT Eastern Pkwy 3 | — |

LED188 reuses York St ID107; LED259 reuses East Broadway ID184. The four
Delancey St–Essex St route-specific station LEDs share ID185. The five final
segments reuse endpoint IDs only:

| LED | Route | Segment | Endpoint IDs |
|---|---|---|---|
| LED187 | G | Greenpoint Av -> Nassau Av | 186 -> 169 |
| LED285 | B | Grand St -> DeKalb Av | 182 -> 1 |
| LED286 | D | Grand St -> Atlantic Av-Barclays Ctr | 182 -> 2 |
| LED287 | A | Hoyt-Schermerhorn Sts -> Nostrand Av | 85 -> 152 |
| LED288 | C | Lafayette Av -> Clinton-Washington Avs | 181 -> 150 |

The segment entities above are represented by endpoint references only; no
registry ID is assigned to a segment LED. Segment endpoint spelling follows
the existing registry display names.
