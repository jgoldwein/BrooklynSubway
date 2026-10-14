// AUTO-GENERATED -- DO NOT EDIT
// Source: Phase 3B semantic mapping
// Baseline tag: phase-3b-semantic-complete
// Baseline commit: f6622cf1af7621c3135088eb217841271b7b5f0f
// Generator: tools/generate_semantic_tables.py
// Source SHA-256 (line endings normalized to LF):
//   phase3b_semantic_mapping_worksheet.csv: 1abfb45f837acf77b10bb94bb6af609a1a0a57ec0a6860c1010fb6cdc20cb64f
//   station_registry.csv: 72f847a0f68f5ce38eb13d1eee6cf561a508786802085ee74a123833e1a4070f
//   data/led_chain_map.csv: d16b1921f78c490f99c71bb3024311627c05d32a693605a066a9f2b635d33c8c
//   data/segment_pixel_order.csv (Phase 4 supplemental): 14945698b975110e866bceb70df391f610ae735d5776c5bc5aadc1eba6284dd4
// Regenerate with: python tools/generate_semantic_tables.py

#include "generated/semantic_tables.h"

namespace brooklyn::semantic {

namespace {

constexpr uint16_t kChannelOffsets[8] = {0, 37, 75, 112, 149, 191, 228, 266};
constexpr uint8_t kChannelCounts[8] = {37, 38, 37, 37, 42, 37, 38, 40};
}  // namespace

const char *const kRouteNames[kRouteCount + 1] = {"", "2", "3", "4", "5", "A", "B", "C", "D", "F", "G", "J", "L", "M", "N", "Q", "R", "S", "Z"};

const Station kStations[kRegistryCount] = {
    {1, "DeKalb Av", nullptr},
    {2, "Atlantic Av-Barclays Ctr", nullptr},
    {3, "7 Av", nullptr},
    {4, "Prospect Park", nullptr},
    {5, "Church Av", nullptr},
    {6, "Newkirk Plaza", nullptr},
    {7, "Kings Hwy", nullptr},
    {8, "Sheepshead Bay", nullptr},
    {9, "Beverley Rd", nullptr},
    {10, "Cortelyou Rd", nullptr},
    {11, "Parkside Av", nullptr},
    {12, "Avenue H", nullptr},
    {13, "Avenue J", nullptr},
    {14, "Avenue M", nullptr},
    {15, "Avenue U", nullptr},
    {16, "Neck Rd", nullptr},
    {17, "Brighton Beach", nullptr},
    {18, "Ocean Pkwy", nullptr},
    {19, "Church Av", nullptr},
    {20, "Fort Hamilton Pkwy", nullptr},
    {21, "7 Av", nullptr},
    {22, "15 St-Prospect Park", nullptr},
    {23, "Ditmas Av", nullptr},
    {24, "18 Av", nullptr},
    {25, "Avenue I", nullptr},
    {26, "Bay Pkwy", nullptr},
    {27, "Avenue N", nullptr},
    {28, "Avenue P", nullptr},
    {29, "Kings Hwy", nullptr},
    {30, "Avenue U", nullptr},
    {31, "Avenue X", nullptr},
    {32, "Neptune Av", nullptr},
    {33, "W 8 St-NY Aquarium", nullptr},
    {34, "Coney Island-Stillwell Av", nullptr},
    {35, "Broadway", "G30"},
    {36, "Flushing Av", "G31"},
    {37, "Myrtle-Willoughby Avs", "G32"},
    {38, "Bedford-Nostrand Avs", "G33"},
    {39, "Classon Av", "G34"},
    {40, "Clinton-Washington Avs", "G35"},
    {41, "Fulton St", "G36"},
    {42, "45 St", "R39"},
    {43, "53 St", "R40"},
    {44, "77 St", "R43"},
    {45, "86 St", "R44"},
    {46, "DeKalb Av", "L16"},
    {47, "Halsey St", "L19"},
    {48, "Wilson Av", "L20"},
    {49, "Bushwick Av-Aberdeen St", "L21"},
    {50, "Atlantic Av", "L24"},
    {51, "Sutter Av", "L25"},
    {52, "Livonia Av", "L26"},
    {53, "New Lots Av", "L27"},
    {54, "East 105 St", "L28"},
    {55, "Grant Av", nullptr},
    {56, "80 St", nullptr},
    {57, "88 St", nullptr},
    {58, "Rockaway Blvd", nullptr},
    {59, "104 St", nullptr},
    {60, "111 St", nullptr},
    {61, "Ozone Park-Lefferts Blvd", nullptr},
    {62, "Howard Beach-JFK Airport", nullptr},
    {63, "Far Rockaway-Mott Av", nullptr},
    {64, "Beach 25 St", nullptr},
    {65, "Beach 36 St", nullptr},
    {66, "Beach 44 St", nullptr},
    {67, "Beach 60 St", nullptr},
    {68, "Beach 67 St", nullptr},
    {69, "Canarsie-Rockaway Pkwy", "L29"},
    {70, "Myrtle-Wyckoff Avs", "L17"},
    {71, "Bay Ridge-95 St", "R45"},
    {72, "Bay Ridge Av", "R42"},
    {73, "25 St", "R35"},
    {74, "9 Av", "B12"},
    {75, "Fort Hamilton Pkwy", "B13"},
    {76, "50 St", "B14"},
    {77, "55 St", "B15"},
    {78, "36 St", "R36"},
    {79, "59 St", "R41"},
    {80, "8 Av", "N02"},
    {81, "Fort Hamilton Pkwy", "N03"},
    {82, "New Utrecht Av", "N04"},
    {83, "Broadway Junction", nullptr},
    {84, "Jay St-MetroTech", nullptr},
    {85, "Hoyt-Schermerhorn Sts", nullptr},
    {86, "4 Av-9 St", "F23"},
    {87, "Franklin Av-Fulton St", nullptr},
    {88, "Borough Hall", nullptr},
    {89, "Franklin Av-Medgar Evers College", nullptr},
    {90, "Botanic Garden", nullptr},
    {91, "Newkirk Av-Little Haiti", nullptr},
    {92, "Aqueduct Racetrack", nullptr},
    {93, "Aqueduct-North Conduit Av", nullptr},
    {94, "Liberty Av", "A52"},
    {95, "Van Siclen Av", "A53"},
    {96, "Shepherd Av", "A54"},
    {97, "Euclid Av", "A55"},
    {98, "New Lots Av", nullptr},
    {99, "Alabama Av", "J24"},
    {100, "Van Siclen Av", "J23"},
    {101, "Van Siclen Av", nullptr},
    {102, "Court St", "R28"},
    {103, "High St", "A40"},
    {104, "Clark St", nullptr},
    {105, "Hoyt St", nullptr},
    {106, "Nevins St", nullptr},
    {107, "York St", nullptr},
    {108, "Pennsylvania Av", nullptr},
    {109, "Lorimer St", nullptr},
    {110, "Flushing Av", nullptr},
    {111, "Myrtle Av", nullptr},
    {112, "Kosciuszko St", nullptr},
    {113, "Morgan Av", nullptr},
    {114, "Jefferson St", nullptr},
    {115, "Central Av", nullptr},
    {116, "Knickerbocker Av", nullptr},
    {117, "Gates Av", nullptr},
    {118, "Marcy Av", nullptr},
    {119, "Halsey St", nullptr},
    {120, "Chauncey St", nullptr},
    {121, "President St-Medgar Evers College", nullptr},
    {122, "Sterling St", nullptr},
    {123, "Winthrop St", nullptr},
    {124, "Church Av", nullptr},
    {125, "Beverly Rd", nullptr},
    {126, "Flatbush Av-Brooklyn College", nullptr},
    {127, "Crown Hts-Utica Av", nullptr},
    {128, "Cleveland St", nullptr},
    {129, "Norwood Av", nullptr},
    {130, "Crescent St", nullptr},
    {131, "Cypress Hills", nullptr},
    {132, "Bedford Av", nullptr},
    {133, "Lorimer St", nullptr},
    {134, "Graham Av", nullptr},
    {135, "Grand St", nullptr},
    {136, "Montrose Av", nullptr},
    {137, "Bergen St", nullptr},
    {138, "Grand Army Plaza", nullptr},
    {139, "Eastern Pkwy-Brooklyn Museum", nullptr},
    {140, "Kingston-Throop Avs", nullptr},
    {141, "Utica Av", nullptr},
    {142, "Ralph Av", nullptr},
    {143, "Rockaway Av", nullptr},
    {144, "Forest Av", nullptr},
    {145, "Seneca Av", nullptr},
    {146, "Nostrand Av", nullptr},
    {147, "Kingston Av", nullptr},
    {148, "Sutter Av-Rutland Rd", nullptr},
    {149, "Saratoga Av", nullptr},
    {150, "Clinton-Washington Avs", nullptr},
    {151, "Franklin Av", nullptr},
    {152, "Nostrand Av", nullptr},
    {153, "Park Pl", nullptr},
    {154, "Broad Channel", nullptr},
    {155, "Beach 90 St", nullptr},
    {156, "Beach 98 St", nullptr},
    {157, "Beach 105 St", nullptr},
    {158, "Rockaway Park-Beach 116 St", nullptr},
    {159, "Union St", nullptr},
    {160, "Prospect Av", nullptr},
    {161, "62 St", nullptr},
    {162, "71 St", nullptr},
    {163, "79 St", nullptr},
    {164, "18 Av", nullptr},
    {165, "20 Av", nullptr},
    {166, "Bay Pkwy", nullptr},
    {167, "25 Av", nullptr},
    {168, "Bay 50 St", nullptr},
    {169, "Nassau Av", nullptr},
    {170, "Lorimer St", nullptr},
    {171, "Bergen St", nullptr},
    {172, "Carroll St", nullptr},
    {173, "Smith-9 Sts", nullptr},
    {174, "18 Av", nullptr},
    {175, "20 Av", nullptr},
    {176, "Bay Pkwy", nullptr},
    {177, "Kings Hwy", nullptr},
    {178, "Avenue U", nullptr},
    {179, "86 St", nullptr},
    {180, "Hewes St", nullptr},
    {181, "Lafayette Av", nullptr},
    {182, "Grand St", nullptr},
    {183, "Canal St", nullptr},
    {184, "East Broadway", nullptr},
    {185, "Delancey St\342\200\223Essex St", nullptr},
    {186, "Greenpoint Av", nullptr},
    {187, "Rockaway Av", nullptr},
    {188, "Junius St", nullptr},
};

const MapLed kMapLeds[kMapLedCount] = {
    // designator, global, station, endpointA,     endpointB, ch, idx, route, role, slot, slotCount
    {42, 0, 24, 0, 0, 1, 0, Route::RouteF, Role::Station, 0, 0},  // LED42 F STATION
    {41, 1, 23, 0, 0, 1, 1, Route::RouteF, Role::Station, 0, 0},  // LED41 F STATION
    {40, 2, 19, 0, 0, 1, 2, Route::RouteF, Role::Station, 0, 0},  // LED40 F STATION
    {229, 3, 19, 0, 0, 1, 3, Route::RouteG, Role::Station, 0, 0},  // LED229 G STATION
    {228, 4, 20, 0, 0, 1, 4, Route::RouteG, Role::Station, 0, 0},  // LED228 G STATION
    {39, 5, 20, 0, 0, 1, 5, Route::RouteF, Role::Station, 0, 0},  // LED39 F STATION
    {227, 6, 22, 0, 0, 1, 6, Route::RouteG, Role::Station, 0, 0},  // LED227 G STATION
    {38, 7, 22, 0, 0, 1, 7, Route::RouteF, Role::Station, 0, 0},  // LED38 F STATION
    {100, 8, 21, 0, 0, 1, 8, Route::RouteF, Role::Station, 0, 0},  // LED100 F STATION
    {206, 9, 21, 0, 0, 1, 9, Route::RouteG, Role::Station, 0, 0},  // LED206 G STATION
    {18, 10, 86, 0, 0, 1, 10, Route::RouteR, Role::Station, 0, 0},  // LED18 R STATION
    {168, 11, 86, 0, 0, 1, 11, Route::RouteG, Role::Station, 0, 0},  // LED168 G STATION
    {99, 12, 173, 0, 0, 1, 12, Route::RouteF, Role::Station, 0, 0},  // LED99 F STATION
    {169, 13, 86, 0, 0, 1, 13, Route::RouteF, Role::Station, 0, 0},  // LED169 F STATION
    {19, 14, 160, 0, 0, 1, 14, Route::RouteR, Role::Station, 0, 0},  // LED19 R STATION
    {20, 15, 73, 0, 0, 1, 15, Route::RouteR, Role::Station, 0, 0},  // LED20 R STATION
    {173, 16, 78, 0, 0, 1, 16, Route::RouteR, Role::Station, 0, 0},  // LED173 R STATION
    {101, 17, 78, 0, 0, 1, 17, Route::RouteN, Role::Station, 0, 0},  // LED101 N STATION
    {172, 18, 78, 0, 0, 1, 18, Route::RouteD, Role::Station, 0, 0},  // LED172 D STATION
    {21, 19, 42, 0, 0, 1, 19, Route::RouteR, Role::Station, 0, 0},  // LED21 R STATION
    {22, 20, 43, 0, 0, 1, 20, Route::RouteR, Role::Station, 0, 0},  // LED22 R STATION
    {26, 21, 74, 0, 0, 1, 21, Route::RouteD, Role::Station, 0, 0},  // LED26 D STATION
    {27, 22, 75, 0, 0, 1, 22, Route::RouteD, Role::Station, 0, 0},  // LED27 D STATION
    {28, 23, 76, 0, 0, 1, 23, Route::RouteD, Role::Station, 0, 0},  // LED28 D STATION
    {102, 24, 79, 0, 0, 1, 24, Route::RouteN, Role::Station, 0, 0},  // LED102 N STATION
    {174, 25, 79, 0, 0, 1, 25, Route::RouteR, Role::Station, 0, 0},  // LED174 R STATION
    {23, 26, 72, 0, 0, 1, 26, Route::RouteR, Role::Station, 0, 0},  // LED23 R STATION
    {24, 27, 44, 0, 0, 1, 27, Route::RouteR, Role::Station, 0, 0},  // LED24 R STATION
    {25, 28, 45, 0, 0, 1, 28, Route::RouteR, Role::Station, 0, 0},  // LED25 R STATION
    {6, 29, 71, 0, 0, 1, 29, Route::RouteR, Role::Station, 0, 0},  // LED6 R STATION
    {103, 30, 80, 0, 0, 1, 30, Route::RouteN, Role::Station, 0, 0},  // LED103 N STATION
    {104, 31, 81, 0, 0, 1, 31, Route::RouteN, Role::Station, 0, 0},  // LED104 N STATION
    {171, 32, 82, 0, 0, 1, 32, Route::RouteN, Role::Station, 0, 0},  // LED171 N STATION
    {29, 33, 77, 0, 0, 1, 33, Route::RouteD, Role::Station, 0, 0},  // LED29 D STATION
    {30, 34, 161, 0, 0, 1, 34, Route::RouteD, Role::Station, 0, 0},  // LED30 D STATION
    {31, 35, 162, 0, 0, 1, 35, Route::RouteD, Role::Station, 0, 0},  // LED31 D STATION
    {32, 36, 163, 0, 0, 1, 36, Route::RouteD, Role::Station, 0, 0},  // LED32 D STATION
    {208, 37, 17, 0, 0, 2, 0, Route::RouteB, Role::Station, 0, 0},  // LED208 B STATION
    {115, 38, 17, 0, 0, 2, 1, Route::RouteQ, Role::Station, 0, 0},  // LED115 Q STATION
    {207, 39, 8, 0, 0, 2, 2, Route::RouteB, Role::Station, 0, 0},  // LED207 B STATION
    {114, 40, 8, 0, 0, 2, 3, Route::RouteQ, Role::Station, 0, 0},  // LED114 Q STATION
    {113, 41, 0, 16, 8, 2, 4, Route::RouteQ, Role::Segment, 0, 1},  // LED113 Q SEGMENT
    {289, 42, 16, 0, 0, 2, 5, Route::RouteQ, Role::Station, 0, 0},  // LED289 Q STATION
    {61, 43, 15, 0, 0, 2, 6, Route::RouteQ, Role::Station, 0, 0},  // LED61 Q STATION
    {233, 44, 7, 0, 0, 2, 7, Route::RouteB, Role::Station, 0, 0},  // LED233 B STATION
    {60, 45, 7, 0, 0, 2, 8, Route::RouteQ, Role::Station, 0, 0},  // LED60 Q STATION
    {59, 46, 14, 0, 0, 2, 9, Route::RouteQ, Role::Station, 0, 0},  // LED59 Q STATION
    {58, 47, 13, 0, 0, 2, 10, Route::RouteQ, Role::Station, 0, 0},  // LED58 Q STATION
    {57, 48, 12, 0, 0, 2, 11, Route::RouteQ, Role::Station, 0, 0},  // LED57 Q STATION
    {43, 49, 25, 0, 0, 2, 12, Route::RouteF, Role::Station, 0, 0},  // LED43 F STATION
    {44, 50, 26, 0, 0, 2, 13, Route::RouteF, Role::Station, 0, 0},  // LED44 F STATION
    {106, 51, 175, 0, 0, 2, 14, Route::RouteN, Role::Station, 0, 0},  // LED106 N STATION
    {105, 52, 174, 0, 0, 2, 15, Route::RouteN, Role::Station, 0, 0},  // LED105 N STATION
    {33, 53, 164, 0, 0, 2, 16, Route::RouteD, Role::Station, 0, 0},  // LED33 D STATION
    {34, 54, 165, 0, 0, 2, 17, Route::RouteD, Role::Station, 0, 0},  // LED34 D STATION
    {107, 55, 176, 0, 0, 2, 18, Route::RouteN, Role::Station, 0, 0},  // LED107 N STATION
    {45, 56, 27, 0, 0, 2, 19, Route::RouteF, Role::Station, 0, 0},  // LED45 F STATION
    {46, 57, 28, 0, 0, 2, 20, Route::RouteF, Role::Station, 0, 0},  // LED46 F STATION
    {47, 58, 29, 0, 0, 2, 21, Route::RouteF, Role::Station, 0, 0},  // LED47 F STATION
    {48, 59, 30, 0, 0, 2, 22, Route::RouteF, Role::Station, 0, 0},  // LED48 F STATION
    {49, 60, 31, 0, 0, 2, 23, Route::RouteF, Role::Station, 0, 0},  // LED49 F STATION
    {111, 61, 32, 0, 0, 2, 24, Route::RouteF, Role::Station, 0, 0},  // LED111 F STATION
    {116, 62, 18, 0, 0, 2, 25, Route::RouteQ, Role::Station, 0, 0},  // LED116 Q STATION
    {175, 63, 33, 0, 0, 2, 26, Route::RouteQ, Role::Station, 0, 0},  // LED175 Q STATION
    {112, 64, 33, 0, 0, 2, 27, Route::RouteF, Role::Station, 0, 0},  // LED112 F STATION
    {110, 65, 179, 0, 0, 2, 28, Route::RouteN, Role::Station, 0, 0},  // LED110 N STATION
    {109, 66, 178, 0, 0, 2, 29, Route::RouteN, Role::Station, 0, 0},  // LED109 N STATION
    {108, 67, 177, 0, 0, 2, 30, Route::RouteN, Role::Station, 0, 0},  // LED108 N STATION
    {35, 68, 166, 0, 0, 2, 31, Route::RouteD, Role::Station, 0, 0},  // LED35 D STATION
    {36, 69, 167, 0, 0, 2, 32, Route::RouteD, Role::Station, 0, 0},  // LED36 D STATION
    {37, 70, 168, 0, 0, 2, 33, Route::RouteD, Role::Station, 0, 0},  // LED37 D STATION
    {167, 71, 34, 0, 0, 2, 34, Route::RouteD, Role::Station, 0, 0},  // LED167 D STATION
    {166, 72, 34, 0, 0, 2, 35, Route::RouteN, Role::Station, 0, 0},  // LED166 N STATION
    {165, 73, 34, 0, 0, 2, 36, Route::RouteF, Role::Station, 0, 0},  // LED165 F STATION
    {7, 74, 34, 0, 0, 2, 37, Route::RouteQ, Role::Station, 0, 0},  // LED7 Q STATION
    {226, 75, 126, 0, 0, 3, 0, Route::Route5, Role::Station, 0, 0},  // LED226 5 STATION
    {146, 76, 126, 0, 0, 3, 1, Route::Route2, Role::Station, 0, 0},  // LED146 2 STATION
    {147, 77, 91, 0, 0, 3, 2, Route::Route2, Role::Station, 0, 0},  // LED147 2 STATION
    {250, 78, 91, 0, 0, 3, 3, Route::Route5, Role::Station, 0, 0},  // LED250 5 STATION
    {225, 79, 125, 0, 0, 3, 4, Route::Route5, Role::Station, 0, 0},  // LED225 5 STATION
    {145, 80, 125, 0, 0, 3, 5, Route::Route2, Role::Station, 0, 0},  // LED145 2 STATION
    {224, 81, 124, 0, 0, 3, 6, Route::Route5, Role::Station, 0, 0},  // LED224 5 STATION
    {144, 82, 124, 0, 0, 3, 7, Route::Route2, Role::Station, 0, 0},  // LED144 2 STATION
    {143, 83, 123, 0, 0, 3, 8, Route::Route2, Role::Station, 0, 0},  // LED143 2 STATION
    {223, 84, 123, 0, 0, 3, 9, Route::Route5, Role::Station, 0, 0},  // LED223 5 STATION
    {222, 85, 122, 0, 0, 3, 10, Route::Route5, Role::Station, 0, 0},  // LED222 5 STATION
    {142, 86, 122, 0, 0, 3, 11, Route::Route2, Role::Station, 0, 0},  // LED142 2 STATION
    {141, 87, 121, 0, 0, 3, 12, Route::Route2, Role::Station, 0, 0},  // LED141 2 STATION
    {221, 88, 121, 0, 0, 3, 13, Route::Route5, Role::Station, 0, 0},  // LED221 5 STATION
    {196, 89, 147, 0, 0, 3, 14, Route::Route3, Role::Station, 0, 0},  // LED196 3 STATION
    {195, 90, 146, 0, 0, 3, 15, Route::Route3, Role::Station, 0, 0},  // LED195 3 STATION
    {302, 91, 0, 89, 127, 3, 16, Route::Route4, Role::Segment, 0, 1},  // LED302 4 SEGMENT
    {301, 92, 0, 89, 146, 3, 17, Route::Route3, Role::Segment, 0, 1},  // LED301 3 SEGMENT
    {300, 93, 0, 89, 121, 3, 18, Route::Route2, Role::Segment, 0, 1},  // LED300 2 SEGMENT
    {183, 94, 90, 0, 0, 3, 19, Route::RouteS, Role::Station, 0, 0},  // LED183 S STATION
    {180, 95, 89, 0, 0, 3, 20, Route::Route2, Role::Station, 0, 0},  // LED180 2 STATION
    {140, 96, 89, 0, 0, 3, 21, Route::Route3, Role::Station, 0, 0},  // LED140 3 STATION
    {181, 97, 89, 0, 0, 3, 22, Route::Route4, Role::Station, 0, 0},  // LED181 4 STATION
    {304, 98, 0, 90, 153, 3, 23, Route::RouteS, Role::Segment, 0, 1},  // LED304 S SEGMENT
    {182, 99, 89, 0, 0, 3, 24, Route::Route5, Role::Station, 0, 0},  // LED182 5 STATION
    {303, 100, 0, 2, 89, 3, 25, Route::Route5, Role::Segment, 0, 1},  // LED303 5 SEGMENT
    {290, 101, 0, 3, 4, 3, 26, Route::RouteQ, Role::Segment, 0, 1},  // LED290 Q SEGMENT
    {185, 102, 4, 0, 0, 3, 27, Route::RouteS, Role::Station, 0, 0},  // LED185 S STATION
    {184, 103, 4, 0, 0, 3, 28, Route::RouteB, Role::Station, 0, 0},  // LED184 B STATION
    {51, 104, 4, 0, 0, 3, 29, Route::RouteQ, Role::Station, 0, 0},  // LED51 Q STATION
    {52, 105, 11, 0, 0, 3, 30, Route::RouteQ, Role::Station, 0, 0},  // LED52 Q STATION
    {53, 106, 5, 0, 0, 3, 31, Route::RouteQ, Role::Station, 0, 0},  // LED53 Q STATION
    {231, 107, 5, 0, 0, 3, 32, Route::RouteB, Role::Station, 0, 0},  // LED231 B STATION
    {54, 108, 9, 0, 0, 3, 33, Route::RouteQ, Role::Station, 0, 0},  // LED54 Q STATION
    {55, 109, 10, 0, 0, 3, 34, Route::RouteQ, Role::Station, 0, 0},  // LED55 Q STATION
    {56, 110, 6, 0, 0, 3, 35, Route::RouteQ, Role::Station, 0, 0},  // LED56 Q STATION
    {232, 111, 6, 0, 0, 3, 36, Route::RouteB, Role::Station, 0, 0},  // LED232 B STATION
    {220, 112, 139, 0, 0, 4, 0, Route::Route2, Role::Station, 0, 0},  // LED220 2 STATION
    {139, 113, 139, 0, 0, 4, 1, Route::Route3, Role::Station, 0, 0},  // LED139 3 STATION
    {230, 114, 3, 0, 0, 4, 2, Route::RouteB, Role::Station, 0, 0},  // LED230 B STATION
    {50, 115, 3, 0, 0, 4, 3, Route::RouteQ, Role::Station, 0, 0},  // LED50 Q STATION
    {138, 116, 138, 0, 0, 4, 4, Route::Route3, Role::Station, 0, 0},  // LED138 3 STATION
    {219, 117, 138, 0, 0, 4, 5, Route::Route2, Role::Station, 0, 0},  // LED219 2 STATION
    {137, 118, 137, 0, 0, 4, 6, Route::Route3, Role::Station, 0, 0},  // LED137 3 STATION
    {218, 119, 137, 0, 0, 4, 7, Route::Route2, Role::Station, 0, 0},  // LED218 2 STATION
    {17, 120, 159, 0, 0, 4, 8, Route::RouteR, Role::Station, 0, 0},  // LED17 R STATION
    {292, 121, 0, 2, 78, 4, 9, Route::RouteN, Role::Segment, 0, 1},  // LED292 N SEGMENT
    {249, 122, 173, 0, 0, 4, 10, Route::RouteG, Role::Station, 0, 0},  // LED249 G STATION
    {248, 123, 172, 0, 0, 4, 11, Route::RouteG, Role::Station, 0, 0},  // LED248 G STATION
    {98, 124, 172, 0, 0, 4, 12, Route::RouteF, Role::Station, 0, 0},  // LED98 F STATION
    {97, 125, 171, 0, 0, 4, 13, Route::RouteF, Role::Station, 0, 0},  // LED97 F STATION
    {247, 126, 171, 0, 0, 4, 14, Route::RouteG, Role::Station, 0, 0},  // LED247 G STATION
    {156, 127, 85, 0, 0, 4, 15, Route::RouteG, Role::Station, 0, 0},  // LED156 G STATION
    {155, 128, 85, 0, 0, 4, 16, Route::RouteA, Role::Station, 0, 0},  // LED155 A STATION
    {119, 129, 85, 0, 0, 4, 17, Route::RouteC, Role::Station, 0, 0},  // LED119 C STATION
    {164, 130, 2, 0, 0, 4, 18, Route::RouteR, Role::Station, 0, 0},  // LED164 R STATION
    {162, 131, 2, 0, 0, 4, 19, Route::RouteN, Role::Station, 0, 0},  // LED162 N STATION
    {161, 132, 2, 0, 0, 4, 20, Route::RouteD, Role::Station, 0, 0},  // LED161 D STATION
    {157, 133, 2, 0, 0, 4, 21, Route::Route2, Role::Station, 0, 0},  // LED157 2 STATION
    {136, 134, 2, 0, 0, 4, 22, Route::Route3, Role::Station, 0, 0},  // LED136 3 STATION
    {158, 135, 2, 0, 0, 4, 23, Route::Route4, Role::Station, 0, 0},  // LED158 4 STATION
    {159, 136, 2, 0, 0, 4, 24, Route::Route5, Role::Station, 0, 0},  // LED159 5 STATION
    {163, 137, 2, 0, 0, 4, 25, Route::RouteQ, Role::Station, 0, 0},  // LED163 Q STATION
    {160, 138, 2, 0, 0, 4, 26, Route::RouteB, Role::Station, 0, 0},  // LED160 B STATION
    {191, 139, 1, 0, 0, 4, 27, Route::RouteR, Role::Station, 0, 0},  // LED191 R STATION
    {217, 140, 106, 0, 0, 4, 28, Route::Route5, Role::Station, 0, 0},  // LED217 5 STATION
    {215, 141, 106, 0, 0, 4, 29, Route::Route4, Role::Station, 0, 0},  // LED215 4 STATION
    {135, 142, 106, 0, 0, 4, 30, Route::Route3, Role::Station, 0, 0},  // LED135 3 STATION
    {216, 143, 106, 0, 0, 4, 31, Route::Route2, Role::Station, 0, 0},  // LED216 2 STATION
    {214, 144, 105, 0, 0, 4, 32, Route::Route2, Role::Station, 0, 0},  // LED214 2 STATION
    {134, 145, 105, 0, 0, 4, 33, Route::Route3, Role::Station, 0, 0},  // LED134 3 STATION
    {177, 146, 88, 0, 0, 4, 34, Route::Route2, Role::Station, 0, 0},  // LED177 2 STATION
    {133, 147, 88, 0, 0, 4, 35, Route::Route3, Role::Station, 0, 0},  // LED133 3 STATION
    {178, 148, 88, 0, 0, 4, 36, Route::Route4, Role::Station, 0, 0},  // LED178 4 STATION
    {190, 149, 1, 0, 0, 5, 0, Route::RouteQ, Role::Station, 0, 0},  // LED190 Q STATION
    {293, 150, 0, 84, 1, 5, 1, Route::RouteR, Role::Segment, 0, 1},  // LED293 R SEGMENT
    {154, 151, 84, 0, 0, 5, 2, Route::RouteR, Role::Station, 0, 0},  // LED154 R STATION
    {153, 152, 84, 0, 0, 5, 3, Route::RouteF, Role::Station, 0, 0},  // LED153 F STATION
    {118, 153, 84, 0, 0, 5, 4, Route::RouteC, Role::Station, 0, 0},  // LED118 C STATION
    {152, 154, 84, 0, 0, 5, 5, Route::RouteA, Role::Station, 0, 0},  // LED152 A STATION
    {179, 155, 88, 0, 0, 5, 6, Route::Route5, Role::Station, 0, 0},  // LED179 5 STATION
    {10, 156, 102, 0, 0, 5, 7, Route::RouteR, Role::Station, 0, 0},  // LED10 R STATION
    {213, 157, 104, 0, 0, 5, 8, Route::Route2, Role::Station, 0, 0},  // LED213 2 STATION
    {132, 158, 104, 0, 0, 5, 9, Route::Route3, Role::Station, 0, 0},  // LED132 3 STATION
    {209, 159, 103, 0, 0, 5, 10, Route::RouteA, Role::Station, 0, 0},  // LED209 A STATION
    {117, 160, 103, 0, 0, 5, 11, Route::RouteC, Role::Station, 0, 0},  // LED117 C STATION
    {294, 161, 0, 84, 107, 5, 12, Route::RouteF, Role::Segment, 0, 1},  // LED294 F SEGMENT
    {311, 162, 0, 183, 2, 5, 13, Route::RouteN, Role::Segment, 0, 1},  // LED311 N SEGMENT
    {310, 163, 0, 183, 1, 5, 14, Route::RouteQ, Role::Segment, 0, 1},  // LED310 Q SEGMENT
    {309, 164, 0, 182, 1, 5, 15, Route::RouteB, Role::Segment, 1, 2},  // LED309 B SEGMENT
    {308, 165, 0, 182, 2, 5, 16, Route::RouteD, Role::Segment, 1, 2},  // LED308 D SEGMENT
    {188, 166, 107, 0, 0, 5, 17, Route::RouteF, Role::Station, 0, 0},  // LED188 F STATION
    {291, 167, 0, 107, 184, 5, 18, Route::RouteF, Role::Segment, 0, 1},  // LED291 F SEGMENT
    {285, 168, 0, 182, 1, 5, 19, Route::RouteB, Role::Segment, 0, 2},  // LED285 B SEGMENT
    {286, 169, 0, 182, 2, 5, 20, Route::RouteD, Role::Segment, 0, 2},  // LED286 D SEGMENT
    {255, 170, 185, 0, 0, 5, 21, Route::RouteF, Role::Station, 0, 0},  // LED255 F STATION
    {256, 171, 185, 0, 0, 5, 22, Route::RouteM, Role::Station, 0, 0},  // LED256 M STATION
    {257, 172, 185, 0, 0, 5, 23, Route::RouteJ, Role::Station, 0, 0},  // LED257 J STATION
    {258, 173, 185, 0, 0, 5, 24, Route::RouteZ, Role::Station, 0, 0},  // LED258 Z STATION
    {259, 174, 184, 0, 0, 5, 25, Route::RouteF, Role::Station, 0, 0},  // LED259 F STATION
    {234, 175, 118, 0, 0, 5, 26, Route::RouteZ, Role::Station, 0, 0},  // LED234 Z STATION
    {62, 176, 118, 0, 0, 5, 27, Route::RouteJ, Role::Station, 0, 0},  // LED62 J STATION
    {235, 177, 118, 0, 0, 5, 28, Route::RouteM, Role::Station, 0, 0},  // LED235 M STATION
    {236, 178, 180, 0, 0, 5, 29, Route::RouteM, Role::Station, 0, 0},  // LED236 M STATION
    {63, 179, 180, 0, 0, 5, 30, Route::RouteJ, Role::Station, 0, 0},  // LED63 J STATION
    {297, 180, 0, 180, 109, 5, 31, Route::RouteM, Role::Segment, 0, 1},  // LED297 M SEGMENT
    {9, 181, 35, 0, 0, 5, 32, Route::RouteG, Role::Station, 0, 0},  // LED9 G STATION
    {82, 182, 136, 0, 0, 5, 33, Route::RouteL, Role::Station, 0, 0},  // LED82 L STATION
    {81, 183, 135, 0, 0, 5, 34, Route::RouteL, Role::Station, 0, 0},  // LED81 L STATION
    {80, 184, 134, 0, 0, 5, 35, Route::RouteL, Role::Station, 0, 0},  // LED80 L STATION
    {295, 185, 0, 170, 35, 5, 36, Route::RouteG, Role::Segment, 0, 1},  // LED295 G SEGMENT
    {96, 186, 170, 0, 0, 5, 37, Route::RouteG, Role::Station, 0, 0},  // LED96 G STATION
    {79, 187, 133, 0, 0, 5, 38, Route::RouteL, Role::Station, 0, 0},  // LED79 L STATION
    {78, 188, 132, 0, 0, 5, 39, Route::RouteL, Role::Station, 0, 0},  // LED78 L STATION
    {95, 189, 169, 0, 0, 5, 40, Route::RouteG, Role::Station, 0, 0},  // LED95 G STATION
    {187, 190, 0, 186, 169, 5, 41, Route::RouteG, Role::Segment, 0, 1},  // LED187 G SEGMENT
    {194, 191, 153, 0, 0, 6, 0, Route::RouteS, Role::Station, 0, 0},  // LED194 S STATION
    {176, 192, 87, 0, 0, 6, 1, Route::RouteS, Role::Station, 0, 0},  // LED176 S STATION
    {210, 193, 152, 0, 0, 6, 2, Route::RouteA, Role::Station, 0, 0},  // LED210 A STATION
    {124, 194, 140, 0, 0, 6, 3, Route::RouteC, Role::Station, 0, 0},  // LED124 C STATION
    {123, 195, 152, 0, 0, 6, 4, Route::RouteC, Role::Station, 0, 0},  // LED123 C STATION
    {122, 196, 151, 0, 0, 6, 5, Route::RouteC, Role::Station, 0, 0},  // LED122 C STATION
    {121, 197, 150, 0, 0, 6, 6, Route::RouteC, Role::Station, 0, 0},  // LED121 C STATION
    {288, 198, 0, 181, 150, 6, 7, Route::RouteC, Role::Segment, 0, 1},  // LED288 C SEGMENT
    {120, 199, 181, 0, 0, 6, 8, Route::RouteC, Role::Station, 0, 0},  // LED120 C STATION
    {287, 200, 0, 85, 152, 6, 9, Route::RouteA, Role::Segment, 0, 1},  // LED287 A SEGMENT
    {189, 201, 1, 0, 0, 6, 10, Route::RouteB, Role::Station, 0, 0},  // LED189 B STATION
    {16, 202, 41, 0, 0, 6, 11, Route::RouteG, Role::Station, 0, 0},  // LED16 G STATION
    {15, 203, 40, 0, 0, 6, 12, Route::RouteG, Role::Station, 0, 0},  // LED15 G STATION
    {14, 204, 39, 0, 0, 6, 13, Route::RouteG, Role::Station, 0, 0},  // LED14 G STATION
    {13, 205, 38, 0, 0, 6, 14, Route::RouteG, Role::Station, 0, 0},  // LED13 G STATION
    {12, 206, 37, 0, 0, 6, 15, Route::RouteG, Role::Station, 0, 0},  // LED12 G STATION
    {11, 207, 36, 0, 0, 6, 16, Route::RouteG, Role::Station, 0, 0},  // LED11 G STATION
    {298, 208, 0, 118, 111, 6, 17, Route::RouteZ, Role::Segment, 0, 1},  // LED298 Z SEGMENT
    {64, 209, 109, 0, 0, 6, 18, Route::RouteJ, Role::Station, 0, 0},  // LED64 J STATION
    {237, 210, 109, 0, 0, 6, 19, Route::RouteM, Role::Station, 0, 0},  // LED237 M STATION
    {238, 211, 110, 0, 0, 6, 20, Route::RouteM, Role::Station, 0, 0},  // LED238 M STATION
    {65, 212, 110, 0, 0, 6, 21, Route::RouteJ, Role::Station, 0, 0},  // LED65 J STATION
    {240, 213, 111, 0, 0, 6, 22, Route::RouteZ, Role::Station, 0, 0},  // LED240 Z STATION
    {66, 214, 111, 0, 0, 6, 23, Route::RouteJ, Role::Station, 0, 0},  // LED66 J STATION
    {239, 215, 111, 0, 0, 6, 24, Route::RouteM, Role::Station, 0, 0},  // LED239 M STATION
    {192, 216, 115, 0, 0, 6, 25, Route::RouteM, Role::Station, 0, 0},  // LED192 M STATION
    {67, 217, 112, 0, 0, 6, 26, Route::RouteJ, Role::Station, 0, 0},  // LED67 J STATION
    {296, 218, 0, 111, 117, 6, 27, Route::RouteZ, Role::Segment, 0, 1},  // LED296 Z SEGMENT
    {68, 219, 117, 0, 0, 6, 28, Route::RouteJ, Role::Station, 0, 0},  // LED68 J STATION
    {241, 220, 117, 0, 0, 6, 29, Route::RouteZ, Role::Station, 0, 0},  // LED241 Z STATION
    {193, 221, 116, 0, 0, 6, 30, Route::RouteM, Role::Station, 0, 0},  // LED193 M STATION
    {86, 222, 70, 0, 0, 6, 31, Route::RouteL, Role::Station, 0, 0},  // LED86 L STATION
    {170, 223, 70, 0, 0, 6, 32, Route::RouteM, Role::Station, 0, 0},  // LED170 M STATION
    {254, 224, 145, 0, 0, 6, 33, Route::RouteM, Role::Station, 0, 0},  // LED254 M STATION
    {85, 225, 46, 0, 0, 6, 34, Route::RouteL, Role::Station, 0, 0},  // LED85 L STATION
    {84, 226, 114, 0, 0, 6, 35, Route::RouteL, Role::Station, 0, 0},  // LED84 L STATION
    {83, 227, 113, 0, 0, 6, 36, Route::RouteL, Role::Station, 0, 0},  // LED83 L STATION
    {198, 228, 127, 0, 0, 7, 0, Route::Route3, Role::Station, 0, 0},  // LED198 3 STATION
    {197, 229, 127, 0, 0, 7, 1, Route::Route4, Role::Station, 0, 0},  // LED197 4 STATION
    {199, 230, 148, 0, 0, 7, 2, Route::Route3, Role::Station, 0, 0},  // LED199 3 STATION
    {200, 231, 149, 0, 0, 7, 3, Route::Route3, Role::Station, 0, 0},  // LED200 3 STATION
    {201, 232, 187, 0, 0, 7, 4, Route::Route3, Role::Station, 0, 0},  // LED201 3 STATION
    {90, 233, 50, 0, 0, 7, 5, Route::RouteL, Role::Station, 0, 0},  // LED90 L STATION
    {128, 234, 94, 0, 0, 7, 6, Route::RouteC, Role::Station, 0, 0},  // LED128 C STATION
    {129, 235, 95, 0, 0, 7, 7, Route::RouteC, Role::Station, 0, 0},  // LED129 C STATION
    {130, 236, 96, 0, 0, 7, 8, Route::RouteC, Role::Station, 0, 0},  // LED130 C STATION
    {212, 237, 97, 0, 0, 7, 9, Route::RouteA, Role::Station, 0, 0},  // LED212 A STATION
    {131, 238, 97, 0, 0, 7, 10, Route::RouteC, Role::Station, 0, 0},  // LED131 C STATION
    {245, 239, 129, 0, 0, 7, 11, Route::RouteZ, Role::Station, 0, 0},  // LED245 Z STATION
    {246, 240, 130, 0, 0, 7, 12, Route::RouteZ, Role::Station, 0, 0},  // LED246 Z STATION
    {77, 241, 131, 0, 0, 7, 13, Route::RouteJ, Role::Station, 0, 0},  // LED77 J STATION
    {76, 242, 130, 0, 0, 7, 14, Route::RouteJ, Role::Station, 0, 0},  // LED76 J STATION
    {75, 243, 129, 0, 0, 7, 15, Route::RouteJ, Role::Station, 0, 0},  // LED75 J STATION
    {74, 244, 128, 0, 0, 7, 16, Route::RouteJ, Role::Station, 0, 0},  // LED74 J STATION
    {73, 245, 100, 0, 0, 7, 17, Route::RouteJ, Role::Station, 0, 0},  // LED73 J STATION
    {244, 246, 100, 0, 0, 7, 18, Route::RouteZ, Role::Station, 0, 0},  // LED244 Z STATION
    {243, 247, 99, 0, 0, 7, 19, Route::RouteZ, Role::Station, 0, 0},  // LED243 Z STATION
    {72, 248, 99, 0, 0, 7, 20, Route::RouteJ, Role::Station, 0, 0},  // LED72 J STATION
    {151, 249, 83, 0, 0, 7, 21, Route::RouteL, Role::Station, 0, 0},  // LED151 L STATION
    {71, 250, 83, 0, 0, 7, 22, Route::RouteJ, Role::Station, 0, 0},  // LED71 J STATION
    {150, 251, 83, 0, 0, 7, 23, Route::RouteZ, Role::Station, 0, 0},  // LED150 Z STATION
    {148, 252, 83, 0, 0, 7, 24, Route::RouteC, Role::Station, 0, 0},  // LED148 C STATION
    {149, 253, 83, 0, 0, 7, 25, Route::RouteA, Role::Station, 0, 0},  // LED149 A STATION
    {299, 254, 0, 83, 50, 7, 26, Route::RouteL, Role::Segment, 0, 1},  // LED299 L SEGMENT
    {127, 255, 143, 0, 0, 7, 27, Route::RouteC, Role::Station, 0, 0},  // LED127 C STATION
    {126, 256, 142, 0, 0, 7, 28, Route::RouteC, Role::Station, 0, 0},  // LED126 C STATION
    {211, 257, 141, 0, 0, 7, 29, Route::RouteA, Role::Station, 0, 0},  // LED211 A STATION
    {125, 258, 141, 0, 0, 7, 30, Route::RouteC, Role::Station, 0, 0},  // LED125 C STATION
    {69, 259, 119, 0, 0, 7, 31, Route::RouteJ, Role::Station, 0, 0},  // LED69 J STATION
    {242, 260, 120, 0, 0, 7, 32, Route::RouteZ, Role::Station, 0, 0},  // LED242 Z STATION
    {70, 261, 120, 0, 0, 7, 33, Route::RouteJ, Role::Station, 0, 0},  // LED70 J STATION
    {89, 262, 49, 0, 0, 7, 34, Route::RouteL, Role::Station, 0, 0},  // LED89 L STATION
    {88, 263, 48, 0, 0, 7, 35, Route::RouteL, Role::Station, 0, 0},  // LED88 L STATION
    {87, 264, 47, 0, 0, 7, 36, Route::RouteL, Role::Station, 0, 0},  // LED87 L STATION
    {253, 265, 144, 0, 0, 7, 37, Route::RouteM, Role::Station, 0, 0},  // LED253 M STATION
    {284, 270, 158, 0, 0, 8, 4, Route::RouteA, Role::Station, 0, 0},  // LED284 A STATION
    {283, 271, 158, 0, 0, 8, 5, Route::RouteS, Role::Station, 0, 0},  // LED283 S STATION
    {281, 272, 157, 0, 0, 8, 6, Route::RouteS, Role::Station, 0, 0},  // LED281 S STATION
    {282, 273, 157, 0, 0, 8, 7, Route::RouteA, Role::Station, 0, 0},  // LED282 A STATION
    {279, 274, 156, 0, 0, 8, 8, Route::RouteS, Role::Station, 0, 0},  // LED279 S STATION
    {280, 275, 156, 0, 0, 8, 9, Route::RouteA, Role::Station, 0, 0},  // LED280 A STATION
    {278, 276, 155, 0, 0, 8, 10, Route::RouteA, Role::Station, 0, 0},  // LED278 A STATION
    {277, 277, 155, 0, 0, 8, 11, Route::RouteS, Role::Station, 0, 0},  // LED277 S STATION
    {305, 278, 0, 154, 155, 8, 12, Route::RouteS, Role::Segment, 0, 1},  // LED305 S SEGMENT
    {276, 279, 68, 0, 0, 8, 13, Route::RouteA, Role::Station, 0, 0},  // LED276 A STATION
    {275, 280, 67, 0, 0, 8, 14, Route::RouteA, Role::Station, 0, 0},  // LED275 A STATION
    {274, 281, 66, 0, 0, 8, 15, Route::RouteA, Role::Station, 0, 0},  // LED274 A STATION
    {273, 282, 65, 0, 0, 8, 16, Route::RouteA, Role::Station, 0, 0},  // LED273 A STATION
    {272, 283, 64, 0, 0, 8, 17, Route::RouteA, Role::Station, 0, 0},  // LED272 A STATION
    {271, 284, 63, 0, 0, 8, 18, Route::RouteA, Role::Station, 0, 0},  // LED271 A STATION
    {269, 285, 154, 0, 0, 8, 19, Route::RouteA, Role::Station, 0, 0},  // LED269 A STATION
    {270, 286, 154, 0, 0, 8, 20, Route::RouteS, Role::Station, 0, 0},  // LED270 S STATION
    {268, 287, 62, 0, 0, 8, 21, Route::RouteA, Role::Station, 0, 0},  // LED268 A STATION
    {267, 288, 93, 0, 0, 8, 22, Route::RouteA, Role::Station, 0, 0},  // LED267 A STATION
    {266, 289, 92, 0, 0, 8, 23, Route::RouteA, Role::Station, 0, 0},  // LED266 A STATION
    {265, 290, 61, 0, 0, 8, 24, Route::RouteA, Role::Station, 0, 0},  // LED265 A STATION
    {264, 291, 60, 0, 0, 8, 25, Route::RouteA, Role::Station, 0, 0},  // LED264 A STATION
    {263, 292, 59, 0, 0, 8, 26, Route::RouteA, Role::Station, 0, 0},  // LED263 A STATION
    {262, 293, 58, 0, 0, 8, 27, Route::RouteA, Role::Station, 0, 0},  // LED262 A STATION
    {261, 294, 57, 0, 0, 8, 28, Route::RouteA, Role::Station, 0, 0},  // LED261 A STATION
    {260, 295, 56, 0, 0, 8, 29, Route::RouteA, Role::Station, 0, 0},  // LED260 A STATION
    {186, 296, 55, 0, 0, 8, 30, Route::RouteA, Role::Station, 0, 0},  // LED186 A STATION
    {205, 297, 98, 0, 0, 8, 31, Route::Route3, Role::Station, 0, 0},  // LED205 3 STATION
    {204, 298, 101, 0, 0, 8, 32, Route::Route3, Role::Station, 0, 0},  // LED204 3 STATION
    {203, 299, 108, 0, 0, 8, 33, Route::Route3, Role::Station, 0, 0},  // LED203 3 STATION
    {91, 300, 51, 0, 0, 8, 34, Route::RouteL, Role::Station, 0, 0},  // LED91 L STATION
    {202, 301, 188, 0, 0, 8, 35, Route::Route3, Role::Station, 0, 0},  // LED202 3 STATION
    {92, 302, 52, 0, 0, 8, 36, Route::RouteL, Role::Station, 0, 0},  // LED92 L STATION
    {93, 303, 53, 0, 0, 8, 37, Route::RouteL, Role::Station, 0, 0},  // LED93 L STATION
    {94, 304, 54, 0, 0, 8, 38, Route::RouteL, Role::Station, 0, 0},  // LED94 L STATION
    {8, 305, 69, 0, 0, 8, 39, Route::RouteL, Role::Station, 0, 0},  // LED8 L STATION
};

namespace {
// CSR-style reverse index: leds for station id live at [kStationLedOffsets[id-1], kStationLedOffsets[id]).
constexpr uint16_t kStationLedOffsets[kRegistryCount + 1] = {
    0, 3, 12, 14, 17, 19, 21, 23, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 35, 36, 38, 40, 42, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
    54, 56, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    90, 91, 92, 93, 94, 95, 97, 98, 99, 100, 101, 102, 103, 104, 107, 109,
    110, 111, 112, 117, 121, 124, 127, 128, 132, 136, 137, 139, 140, 141, 142, 143,
    144, 146, 147, 149, 151, 152, 153, 155, 157, 159, 163, 164, 165, 167, 169, 172,
    173, 174, 175, 176, 177, 179, 182, 183, 185, 187, 189, 191, 193, 195, 197, 199,
    200, 202, 204, 205, 206, 207, 208, 209, 210, 212, 214, 216, 217, 219, 220, 221,
    222, 223, 224, 225, 226, 227, 228, 229, 231, 232, 234, 236, 238, 240, 242, 243,
    244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 256, 258, 260, 261, 262,
    263, 264, 265, 266, 268, 269, 269, 269, 270, 274, 274, 275, 276,
};
constexpr uint16_t kStationLedRecords[kStationLedCount] = {
    139, 149, 201, 130, 131, 132, 133, 134, 135, 136, 137, 138, 114, 115, 102, 103,
    104, 106, 107, 110, 111, 44, 45, 39, 40, 108, 109, 105, 48, 47, 46, 43,
    42, 37, 38, 62, 2, 3, 4, 5, 8, 9, 6, 7, 1, 0, 49, 50,
    56, 57, 58, 59, 60, 61, 63, 64, 71, 72, 73, 74, 181, 207, 206, 205,
    204, 203, 202, 19, 20, 27, 28, 225, 264, 263, 262, 233, 296, 298, 299, 300,
    292, 291, 290, 289, 288, 287, 286, 283, 280, 279, 278, 277, 276, 275, 301, 222,
    223, 29, 26, 15, 21, 22, 23, 33, 16, 17, 18, 24, 25, 30, 31, 32,
    249, 250, 251, 252, 253, 151, 152, 153, 154, 127, 128, 129, 10, 11, 13, 192,
    146, 147, 148, 155, 95, 96, 97, 99, 94, 77, 78, 285, 284, 234, 235, 236,
    237, 238, 293, 247, 248, 245, 246, 294, 156, 159, 160, 157, 158, 144, 145, 140,
    141, 142, 143, 166, 295, 209, 210, 211, 212, 213, 214, 215, 217, 227, 226, 216,
    221, 219, 220, 175, 176, 177, 259, 260, 261, 87, 88, 85, 86, 83, 84, 81,
    82, 79, 80, 75, 76, 228, 229, 244, 239, 243, 240, 242, 241, 188, 187, 184,
    183, 182, 118, 119, 116, 117, 112, 113, 194, 257, 258, 256, 255, 265, 224, 90,
    89, 230, 231, 197, 196, 193, 195, 191, 281, 282, 272, 273, 270, 271, 268, 269,
    266, 267, 120, 14, 34, 35, 36, 53, 54, 68, 69, 70, 189, 186, 125, 126,
    123, 124, 12, 122, 52, 51, 55, 67, 66, 65, 178, 179, 199, 174, 170, 171,
    172, 173, 232, 297,
};
}  // namespace

static_assert(sizeof(MapLed) == 16, "MapLed layout changed");
static_assert(kChannelOffsets[7] + kChannelCounts[7] == kPhysicalLedCount,
              "channel layout must total kPhysicalLedCount");

const Station *findStation(uint16_t stationId) {
  if (stationId == kNoStation || stationId > kRegistryCount) {
    return nullptr;
  }
  return &kStations[stationId - 1];
}

const MapLed *findMapLed(uint8_t channel, uint8_t channelIndex) {
  if (channel < 1 || channel > 8 || channelIndex >= kChannelCounts[channel - 1]) {
    return nullptr;
  }
  const uint16_t global = kChannelOffsets[channel - 1] + channelIndex;
  uint16_t low = 0;
  uint16_t high = kMapLedCount;
  while (low < high) {
    const uint16_t mid = static_cast<uint16_t>((low + high) / 2);
    if (kMapLeds[mid].globalOrder < global) {
      low = static_cast<uint16_t>(mid + 1);
    } else {
      high = mid;
    }
  }
  if (low < kMapLedCount && kMapLeds[low].globalOrder == global) {
    return &kMapLeds[low];
  }
  return nullptr;
}

const uint16_t *stationLedIndices(uint16_t stationId, uint16_t &count) {
  if (stationId == kNoStation || stationId > kRegistryCount) {
    count = 0;
    return nullptr;
  }
  const uint16_t begin = kStationLedOffsets[stationId - 1];
  count = static_cast<uint16_t>(kStationLedOffsets[stationId] - begin);
  return &kStationLedRecords[begin];
}

}  // namespace brooklyn::semantic
