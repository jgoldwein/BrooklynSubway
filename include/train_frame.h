// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "gtfs_adapter.h"
#include "train_position_resolver.h"

#include <stddef.h>
#include <stdint.h>

// Phase 4G: purely logical train occupancy frame.
//
//   resolved trains -> TrainFrame (this file) -> FUTURE renderer -> FUTURE LED driver
//
// The frame records only HOW MANY resolved trains map to each canonical global
// physical LED index (led::map order, 0..305). It has no colour, brightness,
// animation or persistence, no dependency on any LED driver, and it never
// touches GPIO or LED output. Only the 302 semantic map LEDs can be occupied;
// the four status LEDs (and U23, which is not addressable) never can.
//
// Input contract: one resolved result per train. The upstream Phase 4E decoder
// already merges TripUpdate/VehiclePosition entities by trip_id within a feed,
// so the caller passes that merged set; the frame does NOT deduplicate.
namespace brooklyn::semantic {

constexpr uint16_t kFrameLedCount = 306;      // == led::map::TOTAL_LED_COUNT
constexpr uint8_t kOccupancyMax = 255;        // per-LED count saturates here

struct TrainFrame {
  uint8_t occupancy[kFrameLedCount];  // trains on each global LED (saturating)
  uint32_t resolvedTrains;   // accepted trains (including saturated ones)
  uint32_t excessTrains;     // resolvedTrains - occupiedLeds
  uint16_t occupiedLeds;     // LEDs with occupancy >= 1
  uint16_t collisionLeds;    // LEDs with occupancy >= 2
  uint16_t saturatedTrains;  // accepted trains not counted because a LED was at 255
  uint16_t ignoredTrains;    // results that are not a map Match (no fallback)
  uint16_t rejectedTrains;   // Match results failing identity validation
  uint8_t maxOccupancy;
};

enum class FrameAdd : uint8_t {
  Added,     // counted on exactly one map LED
  Ignored,   // not a successful Match; nothing recorded
  Rejected,  // claimed Match but identity is inconsistent; nothing recorded
};

void clearFrame(TrainFrame &frame);

// Adds one resolved train. Only a final Match with a self-consistent identity
// is counted: the global index must be a map LED in led::map, the semantic
// record must be the canonical kMapLeds entry for that index, and the
// channel/channelIndex must agree with led::map. Status LEDs are rejected.
FrameAdd addResolvedTrain(TrainFrame &frame, const ResolveResult &resolved);

// Adds an adapter result: counted only when status == Match.
FrameAdd addGtfsResult(TrainFrame &frame, const GtfsMapResult &result);

// Clears then adds every result. Order-independent. Returns trains Added.
size_t composeTrainFrame(TrainFrame &frame, const GtfsMapResult *results, size_t count);

uint8_t frameOccupancy(const TrainFrame &frame, uint16_t globalIndex);  // 0 when out of range
bool frameOccupied(const TrainFrame &frame, uint16_t globalIndex);

}  // namespace brooklyn::semantic
