// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "train_position_resolver.h"

namespace brooklyn::semantic {
namespace {

constexpr ResolveResult kInvalid{ResolveStatus::Invalid, kNoGlobalIndex, 0, 0,
                                 nullptr};
constexpr ResolveResult kNoMatch{ResolveStatus::NoMatch, kNoGlobalIndex, 0, 0,
                                 nullptr};
constexpr ResolveResult kAmbiguous{ResolveStatus::Ambiguous, kNoGlobalIndex, 0,
                                   0, nullptr};

bool validStationId(uint16_t id) { return id >= 1 && id <= kRegistryCount; }

bool validRoute(Route route) {
  const uint8_t value = static_cast<uint8_t>(route);
  return value >= 1 && value <= kRouteCount;
}

}  // namespace

ResolveResult resolveTrainPositionIn(const MapLed *table, uint16_t count,
                                     const TrainMapPosition &position) {
  if (table == nullptr || !validRoute(position.route)) {
    return kInvalid;
  }

  if (position.kind == PositionKind::Station) {
    if (!validStationId(position.stationId)) {
      return kInvalid;
    }
  } else if (position.kind == PositionKind::Segment) {
    if (!validStationId(position.endpointA) ||
        !validStationId(position.endpointB) ||
        position.endpointA == position.endpointB) {
      return kInvalid;
    }
  } else {
    return kInvalid;
  }

  const MapLed *found = nullptr;
  uint16_t matches = 0;
  bool slotOutOfRange = false;
  for (uint16_t i = 0; i < count; ++i) {
    const MapLed &led = table[i];
    if (led.route != position.route) {
      continue;
    }
    bool hit = false;
    if (position.kind == PositionKind::Station) {
      hit = led.role == Role::Station && led.stationId == position.stationId;
    } else if (led.role == Role::Segment) {
      const bool forward = led.endpointA == position.endpointA &&
                           led.endpointB == position.endpointB;
      const bool reverse = led.endpointA == position.endpointB &&
                           led.endpointB == position.endpointA;
      if (forward || reverse) {
        if (position.segmentSlot == kAnySegmentSlot) {
          hit = true;
        } else if (position.segmentSlot >= led.segmentSlotCount) {
          slotOutOfRange = true;
        } else {
          const uint8_t stored =
              forward ? position.segmentSlot
                      : static_cast<uint8_t>(led.segmentSlotCount - 1 -
                                             position.segmentSlot);
          hit = led.segmentSlot == stored;
        }
      }
    }
    if (hit) {
      found = &led;
      ++matches;
    }
  }

  if (slotOutOfRange) {
    return kInvalid;
  }
  if (matches == 0) {
    return kNoMatch;
  }
  if (matches > 1) {
    return kAmbiguous;
  }
  return ResolveResult{ResolveStatus::Match, found->globalOrder, found->channel,
                       found->channelIndex, found};
}

uint8_t segmentPixelCount(Route route, uint16_t endpointA, uint16_t endpointB) {
  if (!validRoute(route) || !validStationId(endpointA) ||
      !validStationId(endpointB) || endpointA == endpointB) {
    return 0;
  }
  for (uint16_t i = 0; i < kMapLedCount; ++i) {
    const MapLed &led = kMapLeds[i];
    if (led.route == route && led.role == Role::Segment &&
        ((led.endpointA == endpointA && led.endpointB == endpointB) ||
         (led.endpointA == endpointB && led.endpointB == endpointA))) {
      return led.segmentSlotCount;
    }
  }
  return 0;
}

ResolveResult resolveTrainPosition(const TrainMapPosition &position) {
  return resolveTrainPositionIn(kMapLeds, kMapLedCount, position);
}

}  // namespace brooklyn::semantic
