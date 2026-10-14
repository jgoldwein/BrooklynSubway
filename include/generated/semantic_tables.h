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

#pragma once

#include <stdint.h>

namespace brooklyn::semantic {

constexpr uint16_t kMapLedCount = 302;
constexpr uint16_t kStationLedCount = 276;
constexpr uint16_t kSegmentLedCount = 26;
constexpr uint16_t kRegistryCount = 188;
constexpr uint16_t kRouteCount = 18;
constexpr uint16_t kPhysicalLedCount = 306;

// Registry station IDs are contiguous 1..kRegistryCount. 0 means "none".
constexpr uint16_t kNoStation = 0;

enum class Route : uint8_t {
  None = 0,
  Route2 = 1,
  Route3 = 2,
  Route4 = 3,
  Route5 = 4,
  RouteA = 5,
  RouteB = 6,
  RouteC = 7,
  RouteD = 8,
  RouteF = 9,
  RouteG = 10,
  RouteJ = 11,
  RouteL = 12,
  RouteM = 13,
  RouteN = 14,
  RouteQ = 15,
  RouteR = 16,
  RouteS = 17,
  RouteZ = 18,
};

enum class Role : uint8_t {
  Station = 1,
  Segment = 2,
};

struct Station {
  uint16_t id;
  const char *name;           // canonical display name (UTF-8)
  const char *gtfsParentStopId;  // nullptr when the registry has none
};

// One record per physical map LED, sorted by global physical order.
// STATION: stationId != 0, endpointA == endpointB == 0.
// SEGMENT: stationId == 0, endpointA/endpointB are registry station IDs.
// segmentSlot is the zero-based pixel position along the stored endpointA ->
// endpointB direction; segmentSlotCount is the number of LEDs in that segment.
// STATION records carry segmentSlot == segmentSlotCount == 0.
struct MapLed {
  uint16_t designator;    // N in "LEDN"
  uint16_t globalOrder;   // 0..305 across channels 1..8
  uint16_t stationId;
  uint16_t endpointA;
  uint16_t endpointB;
  uint8_t channel;        // 1..8
  uint8_t channelIndex;   // 0-based within the channel
  Route route;
  Role role;
  uint8_t segmentSlot;
  uint8_t segmentSlotCount;
};

extern const char *const kRouteNames[kRouteCount + 1];  // [0] == ""
extern const Station kStations[kRegistryCount];
extern const MapLed kMapLeds[kMapLedCount];

// Returns nullptr for unknown IDs.
const Station *findStation(uint16_t stationId);

// Returns nullptr for status LEDs and out-of-range addresses.
const MapLed *findMapLed(uint8_t channel, uint8_t channelIndex);

// Indices into kMapLeds of the STATION LEDs for a registry station
// (one per route that serves it). count == 0 for endpoint-only stations.
const uint16_t *stationLedIndices(uint16_t stationId, uint16_t &count);

}  // namespace brooklyn::semantic
