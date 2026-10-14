// AUTO-GENERATED -- DO NOT EDIT
// Generator: tools/generate_gtfs_stop_table.py
// Source SHA-256 (line endings normalized to LF):
//   data/gtfs_stop_map.csv: 37524a1b035ce62f134c151c8b186dce4709730481e96d2a512d5fc14a5fbfc3
//   data/gtfs_stop_review.csv: 637d5dcfc50343ffbf318aa9423c7523fe7114f6c32fe7c532d3f201136f7a94
//   station_registry.csv: 72f847a0f68f5ce38eb13d1eee6cf561a508786802085ee74a123833e1a4070f
// Regenerate with: python tools/generate_gtfs_stop_table.py

#pragma once

#include <stdint.h>

namespace brooklyn::semantic {

constexpr uint16_t kGtfsStopCount = 197;

// One GTFS *parent* stop (3 chars, e.g. "A41") -> Phase 3B registry station ID.
// Sorted by stopId (byte order) so lookups may binary-search. Several parent
// stops may map to one station (transfer complexes); a stop maps to one station.
struct GtfsStop {
  char stopId[4];  // 3 characters + NUL
  uint16_t stationId;
};

extern const GtfsStop kGtfsStops[kGtfsStopCount];

}  // namespace brooklyn::semantic
