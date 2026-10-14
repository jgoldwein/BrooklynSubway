// AUTO-GENERATED -- DO NOT EDIT
// Generator: tools/generate_gtfs_topology_table.py
// Source: data/gtfs_topology.csv
// Static GTFS feed_version: 20260826-X-long-term-supplement-trip-ids
// sha256(trips.txt): 34E06E96CF2A1E9D9F6E88707AA031B9F442FD851954B16E4D41431AD1F48D9D
// sha256(stop_times.txt): DB63C86F7C5CF3BFAD6D0FB53F16E66B15A8A0F41A2EDDDEEB6E45EB2FC049A0
// sha256(stops.txt): 423036143D340315590054C138B76692010886527EA0B3693DABE74FC1B55162
// Regenerate: python tools/generate_gtfs_topology_table.py

#pragma once

#include <stdint.h>

namespace brooklyn::gtfs_topology {

constexpr uint16_t kTopologyEntryCount = 672;
constexpr uint16_t kTopologyPredecessorCount = 691;
constexpr uint8_t kTopologyRouteCount = 20;
constexpr uint8_t kTopologyOriginFlag = 0x80;
constexpr uint8_t kTopologyCountMask = 0x0F;

// GTFS route ids covered, indexed by routeDir >> 1.
extern const char *const kTopologyRoutes[kTopologyRouteCount];

// Key (routeDir, dest); sorted ascending for binary search.
//   routeDir : (route index << 1) | (direction == 'S')
//   dest     : GTFS parent stop id packed base-36 (3 chars, 0-9A-Z)
//   flags    : kTopologyOriginFlag if the stop starts some pattern, low
//              nibble = number of distinct predecessors
//   predOff  : first index into kTopologyPredecessors (packed base-36)
struct TopologyEntry {
  uint16_t dest;
  uint8_t routeDir;
  uint8_t flags;
  uint16_t predOff;
};

extern const TopologyEntry kTopology[kTopologyEntryCount];
extern const uint16_t kTopologyPredecessors[kTopologyPredecessorCount];

}  // namespace brooklyn::gtfs_topology
