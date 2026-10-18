// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "generated/semantic_tables.h"

#include <stdint.h>

// Read-only semantic lookups. Nothing here touches GPIO or drives LED output.
namespace brooklyn::semantic {

static_assert(kRouteCount < 32, "route visibility mask must fit in uint32_t");
constexpr uint32_t kAllRoutesVisibleMask =
    (uint32_t{1} << kRouteCount) - 1;

// globalIndex uses led::map global physical order (0..305).
// Returns nullptr for status LEDs and out-of-range values.
const MapLed *semanticForLed(uint16_t globalIndex);
const MapLed *semanticForPhysicalLed(uint8_t channel, uint8_t channelIndex);
const Station *stationById(uint16_t stationId);

bool isStationLed(uint16_t globalIndex);
bool isSegmentLed(uint16_t globalIndex);

Route routeForLed(uint16_t globalIndex);  // Route::None when no map LED
const char *routeName(Route route);       // "" for Route::None / invalid
const char *stationName(uint16_t stationId);  // nullptr when unknown
inline uint32_t routeVisibilityBit(Route route) {
  const uint8_t value = static_cast<uint8_t>(route);
  return value == 0 || value > kRouteCount
             ? 0
             : uint32_t{1} << (value - 1);
}
inline bool routeVisible(uint32_t visibilityMask, Route route) {
  const uint32_t bit = routeVisibilityBit(route);
  return bit != 0 && (visibilityMask & bit) != 0;
}

// Cross-checks the generated tables against led::map. Read-only and not
// called during normal boot; returns false on the first inconsistency.
bool selfCheck();

}  // namespace brooklyn::semantic
