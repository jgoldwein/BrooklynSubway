// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "semantic_api.h"

#include "led_map.h"

namespace brooklyn::semantic {

static_assert(kMapLedCount == 302, "expected 302 map LEDs");
static_assert(kRegistryCount == 188, "expected 188 registry stations");
static_assert(kStationLedCount + kSegmentLedCount == kMapLedCount,
              "station + segment records must equal map LEDs");
static_assert(kPhysicalLedCount == led::map::TOTAL_LED_COUNT,
              "generated physical LED count must match led::map");
static_assert(kPhysicalLedCount - kMapLedCount == 4,
              "exactly four status LEDs are non-map");
static_assert(static_cast<uint8_t>(Route::RouteZ) == kRouteCount,
              "route enum and route table disagree");

const MapLed *semanticForLed(uint16_t globalIndex) {
  led::PhysicalAddress address{};
  if (!led::map::toPhysicalAddress(led::GlobalPhysicalIndex{globalIndex},
                                   address)) {
    return nullptr;
  }
  return findMapLed(static_cast<uint8_t>(address.channel), address.index);
}

const MapLed *semanticForPhysicalLed(uint8_t channel, uint8_t channelIndex) {
  return findMapLed(channel, channelIndex);
}

const Station *stationById(uint16_t stationId) { return findStation(stationId); }

bool isStationLed(uint16_t globalIndex) {
  const MapLed *led = semanticForLed(globalIndex);
  return led != nullptr && led->role == Role::Station;
}

bool isSegmentLed(uint16_t globalIndex) {
  const MapLed *led = semanticForLed(globalIndex);
  return led != nullptr && led->role == Role::Segment;
}

Route routeForLed(uint16_t globalIndex) {
  const MapLed *led = semanticForLed(globalIndex);
  return led == nullptr ? Route::None : led->route;
}

const char *routeName(Route route) {
  const uint8_t value = static_cast<uint8_t>(route);
  return value <= kRouteCount ? kRouteNames[value] : "";
}

const char *stationName(uint16_t stationId) {
  const Station *station = findStation(stationId);
  return station == nullptr ? nullptr : station->name;
}

bool selfCheck() {
  uint16_t mapCount = 0;
  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    led::map::PhysicalLed physical{};
    if (!led::map::physicalLed(led::GlobalPhysicalIndex{global}, physical)) {
      return false;
    }
    const MapLed *record = semanticForLed(global);
    if (physical.role == led::LedRole::Status) {
      if (record != nullptr) {
        return false;
      }
      continue;
    }
    if (record == nullptr || record->globalOrder != global ||
        record->designator != physical.designator.value ||
        record->channel != static_cast<uint8_t>(physical.address.channel) ||
        record->channelIndex != physical.address.index ||
        semanticForPhysicalLed(record->channel, record->channelIndex) != record) {
      return false;
    }
    const bool isStation = record->role == Role::Station;
    if (isStation ? (stationById(record->stationId) == nullptr ||
                     record->endpointA != 0 || record->endpointB != 0)
                  : (record->stationId != 0 ||
                     stationById(record->endpointA) == nullptr ||
                     stationById(record->endpointB) == nullptr ||
                     record->endpointA == record->endpointB)) {
      return false;
    }
    ++mapCount;
  }
  return mapCount == kMapLedCount;
}

}  // namespace brooklyn::semantic
