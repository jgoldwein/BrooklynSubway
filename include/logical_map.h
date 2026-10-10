// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_types.h"

#include <stdint.h>

namespace brooklyn::subway {

struct RouteId {
  uint16_t value;
};

struct StationId {
  uint16_t value;
};

struct MapElementId {
  uint16_t value;
};

enum class Direction : uint8_t {
  Unknown,
  Northbound,
  Southbound,
  Eastbound,
  Westbound,
};

enum class MapElementType : uint8_t {
  Unknown,
  Station,
  Segment,
  Status,
};

struct PhysicalLedRef {
  led::PhysicalAddress address;
  led::GlobalPhysicalIndex globalIndex;
  led::BoardLedDesignator designator;
};

struct MapElement {
  MapElementId id;
  MapElementType type;
  RouteId route;
  StationId station;
  StationId fromStation;
  StationId toStation;
  Direction direction;
  PhysicalLedRef physical;
  bool assigned;
};

enum class PositionType : uint8_t {
  Station,
  Segment,
};

struct TrainPosition {
  RouteId route;
  const char *tripId;
  Direction direction;
  PositionType type;
  StationId station;
  StationId fromStation;
  StationId toStation;
  uint64_t observedAtUnixMs;
  uint32_t receivedAtMillis;
};

struct ValidationReport {
  uint16_t elements;
  uint16_t stations;
  uint16_t segments;
  uint16_t assigned;
  uint16_t unassigned;
  uint16_t routes;
  const char *failureReason;
};

uint16_t elementCount();
bool elementAt(uint16_t index, MapElement &element);
bool findElement(MapElementId id, MapElement &element);
bool resolve(MapElementId id, PhysicalLedRef &physical);
bool resolveTrainPosition(const TrainPosition &position,
                          MapElementId *elementIds, uint16_t capacity,
                          uint16_t &resolvedCount);
void clearFramebuffer();
bool setElementColor(MapElementId id, led::Rgb color);
uint16_t setRouteColor(RouteId route, led::Rgb color);
bool validate(ValidationReport &report);
bool runSelfTest(ValidationReport &report, const char *&failureReason);

}  // namespace brooklyn::subway
