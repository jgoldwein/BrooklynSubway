// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "generated/semantic_tables.h"

#include <stdint.h>

// Pure software resolver: normalized train position -> physical map LED
// identity. It never touches GPIO, LED buffers, or LED output routines.
namespace brooklyn::semantic {

enum class PositionKind : uint8_t {
  Invalid = 0,
  Station = 1,
  Segment = 2,
};

// segmentSlot value meaning "slot not specified".
constexpr uint8_t kAnySegmentSlot = 0xFF;

// Station: route + stationId (endpoints and slot ignored).
// Segment: route + endpointA/B (unordered pair; stationId ignored) + optional
// display slot. Slot N counts pixels from endpointA toward endpointB as the
// caller supplied them: with endpoints reversed relative to the stored record,
// the stored slot is slotCount - 1 - N.
struct TrainMapPosition {
  Route route;
  PositionKind kind;
  uint16_t stationId;
  uint16_t endpointA;
  uint16_t endpointB;
  uint8_t segmentSlot;
};

enum class ResolveStatus : uint8_t {
  Match,      // exactly one map LED represents this position
  NoMatch,    // well-formed query, but no LED represents it (no fallback)
  Ambiguous,  // more than one LED matches (e.g. multi-pixel segment, no slot)
  Invalid,    // malformed query (bad route, kind, station ID, or slot)
};

constexpr uint16_t kNoGlobalIndex = 0xFFFF;

struct ResolveResult {
  ResolveStatus status;
  uint16_t globalIndex;   // kNoGlobalIndex unless Match
  uint8_t channel;        // 0 unless Match
  uint8_t channelIndex;   // 0 unless Match
  const MapLed *semantic;  // nullptr unless Match
};

constexpr TrainMapPosition stationPosition(Route route, uint16_t stationId) {
  return TrainMapPosition{route, PositionKind::Station, stationId, 0, 0,
                          kAnySegmentSlot};
}

constexpr TrainMapPosition segmentPosition(
    Route route, uint16_t endpointA, uint16_t endpointB,
    uint8_t segmentSlot = kAnySegmentSlot) {
  return TrainMapPosition{route, PositionKind::Segment, 0, endpointA, endpointB,
                          segmentSlot};
}

// Resolves against the generated kMapLeds table.
ResolveResult resolveTrainPosition(const TrainMapPosition &position);

// Number of display pixels of the explicit segment for route + unordered
// endpoint pair; 0 when the map has no such segment (or the query is invalid).
uint8_t segmentPixelCount(Route route, uint16_t endpointA, uint16_t endpointB);

// Same logic over an arbitrary table (used by tests to exercise ambiguity).
ResolveResult resolveTrainPositionIn(const MapLed *table, uint16_t count,
                                     const TrainMapPosition &position);

}  // namespace brooklyn::semantic
