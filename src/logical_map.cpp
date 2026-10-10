// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "logical_map.h"

#include "led_engine.h"
#include "led_map.h"

namespace brooklyn::subway {
namespace {

constexpr uint16_t EXPECTED_CHANNEL_COUNTS[led::map::CHANNEL_COUNT] = {
    37, 38, 37, 37, 42, 37, 38, 40};
constexpr uint16_t STATUS_LED_COUNT = 4;
constexpr uint16_t LOGICAL_ELEMENT_CAPACITY =
    led::map::TOTAL_LED_COUNT - STATUS_LED_COUNT;

MapElement elements[LOGICAL_ELEMENT_CAPACITY]{};
uint16_t elementCountValue = 0;
bool inventoryInitialized = false;

bool samePhysicalLed(const PhysicalLedRef &left,
                     const PhysicalLedRef &right) {
  return left.address.channel == right.address.channel &&
         left.address.index == right.address.index;
}

bool physicalRefIsValid(const PhysicalLedRef &reference) {
  led::map::PhysicalLed physical{};
  if (!led::map::physicalLed(reference.globalIndex, physical)) {
    return false;
  }
  return physical.role == led::LedRole::Map &&
         physical.address.channel == reference.address.channel &&
         physical.address.index == reference.address.index &&
         physical.designator.value == reference.designator.value;
}

bool validAssignedElement(const MapElement &element) {
  if (!element.assigned || element.route.value == 0 ||
      !physicalRefIsValid(element.physical)) {
    return false;
  }

  if (element.type == MapElementType::Station) {
    return element.station.value != 0;
  }
  if (element.type == MapElementType::Segment) {
    return element.fromStation.value != 0 &&
           element.toStation.value != 0 &&
           element.fromStation.value != element.toStation.value;
  }
  return false;
}

bool validateAssignedPair(const MapElement &left, const MapElement &right,
                          const char *&failureReason) {
  if (left.id.value == right.id.value) {
    failureReason = "duplicate logical map element ID";
    return false;
  }
  if (left.type == MapElementType::Station &&
      right.type == MapElementType::Station &&
      left.station.value == right.station.value &&
      left.route.value != right.route.value &&
      samePhysicalLed(left.physical, right.physical)) {
    failureReason = "route-specific shared station LEDs were collapsed";
    return false;
  }
  if (samePhysicalLed(left.physical, right.physical)) {
    failureReason = "duplicate physical LED assignment";
    return false;
  }
  return true;
}

bool validateAssignedElements(const MapElement *assignedElements,
                              uint16_t count, const char *&failureReason) {
  for (uint16_t i = 0; i < count; ++i) {
    if (!validAssignedElement(assignedElements[i])) {
      failureReason = "assigned map element has invalid identity or physical reference";
      return false;
    }

    for (uint16_t j = 0; j < i; ++j) {
      if (!validateAssignedPair(assignedElements[j], assignedElements[i],
                                failureReason)) {
        return false;
      }
    }
  }
  return true;
}

void initializeInventory() {
  if (inventoryInitialized) {
    return;
  }

  elementCountValue = 0;
  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    led::map::PhysicalLed physical{};
    if (!led::map::physicalLed({global}, physical) ||
        physical.role == led::LedRole::Status) {
      continue;
    }

    MapElement &element = elements[elementCountValue];
    element.id.value = static_cast<uint16_t>(elementCountValue + 1);
    element.type = MapElementType::Unknown;
    element.route = {0};
    element.station = {0};
    element.fromStation = {0};
    element.toStation = {0};
    element.direction = Direction::Unknown;
    element.physical = {physical.address, physical.globalIndex,
                        physical.designator};
    element.assigned = false;
    ++elementCountValue;
  }
  inventoryInitialized = true;
}

void setFailure(ValidationReport &report, const char *reason) {
  report.failureReason = reason;
}

}  // namespace

uint16_t elementCount() {
  initializeInventory();
  return elementCountValue;
}

bool elementAt(uint16_t index, MapElement &element) {
  initializeInventory();
  if (index >= elementCountValue) {
    return false;
  }
  element = elements[index];
  return true;
}

bool findElement(MapElementId id, MapElement &element) {
  initializeInventory();
  if (id.value == 0) {
    return false;
  }
  const uint16_t index = static_cast<uint16_t>(id.value - 1);
  if (index >= elementCountValue || elements[index].id.value != id.value) {
    return false;
  }
  element = elements[index];
  return true;
}

bool resolve(MapElementId id, PhysicalLedRef &physical) {
  MapElement element{};
  if (!findElement(id, element)) {
    return false;
  }
  physical = element.physical;
  return true;
}

bool resolveTrainPosition(const TrainPosition &position,
                          MapElementId *elementIds, uint16_t capacity,
                          uint16_t &resolvedCount) {
  initializeInventory();
  resolvedCount = 0;
  if (position.route.value == 0 || elementIds == nullptr || capacity == 0) {
    return false;
  }

  for (uint16_t i = 0; i < elementCountValue; ++i) {
    const MapElement &element = elements[i];
    if (!element.assigned || element.route.value != position.route.value ||
        element.type != (position.type == PositionType::Station
                             ? MapElementType::Station
                             : MapElementType::Segment) ||
        element.direction != position.direction) {
      continue;
    }

    const bool matches =
        position.type == PositionType::Station
            ? element.station.value == position.station.value
            : element.fromStation.value == position.fromStation.value &&
                  element.toStation.value == position.toStation.value;
    if (matches) {
      if (resolvedCount == capacity) {
        return false;
      }
      elementIds[resolvedCount++] = element.id;
    }
  }
  return resolvedCount > 0;
}

void clearFramebuffer() {
  led::clear();
}

bool setElementColor(MapElementId id, led::Rgb color) {
  PhysicalLedRef physical{};
  return resolve(id, physical) && led::setLed(physical.address, color);
}

uint16_t setRouteColor(RouteId route, led::Rgb color) {
  initializeInventory();
  if (route.value == 0) {
    return 0;
  }
  uint16_t updated = 0;
  for (uint16_t i = 0; i < elementCountValue; ++i) {
    if (elements[i].assigned && elements[i].route.value == route.value &&
        led::setLed(elements[i].physical.address, color)) {
      ++updated;
    }
  }
  return updated;
}

bool validate(ValidationReport &report) {
  initializeInventory();
  report = {};
  report.elements = elementCountValue;

  if (led::map::TOTAL_LED_COUNT != 306 ||
      elementCountValue != LOGICAL_ELEMENT_CAPACITY) {
    setFailure(report, "physical topology or logical inventory count mismatch");
    return false;
  }

  uint16_t totalPhysical = 0;
  for (uint8_t channel = 0; channel < led::map::CHANNEL_COUNT; ++channel) {
    const uint8_t count =
        led::map::channelLedCount(static_cast<led::PhysicalChannel>(channel + 1));
    if (count != EXPECTED_CHANNEL_COUNTS[channel]) {
      setFailure(report, "physical channel count mismatch");
      return false;
    }
    totalPhysical += count;
  }
  if (led::map::CHANNEL_COUNT != 8 || totalPhysical != 306) {
    setFailure(report, "physical topology must contain exactly 8 channels and 306 LEDs");
    return false;
  }

  for (uint8_t statusIndex = 0; statusIndex < STATUS_LED_COUNT;
       ++statusIndex) {
    led::map::PhysicalLed physical{};
    if (!led::map::physicalLed({static_cast<uint16_t>(266 + statusIndex)},
                               physical) ||
        physical.role != led::LedRole::Status ||
        physical.address.channel != led::PhysicalChannel::CH8 ||
        physical.address.index != statusIndex) {
      setFailure(report, "status LEDs must remain outside the subway map inventory");
      return false;
    }
  }

  uint16_t assignedCount = 0;
  for (uint16_t i = 0; i < elementCountValue; ++i) {
    const MapElement &element = elements[i];
    if (!physicalRefIsValid(element.physical)) {
      setFailure(report, "map element has an invalid physical LED reference");
      return false;
    }
    if (element.assigned) {
      ++assignedCount;
      if (element.type == MapElementType::Station) {
        ++report.stations;
      } else if (element.type == MapElementType::Segment) {
        ++report.segments;
      }
    } else if (element.type != MapElementType::Unknown ||
               element.route.value != 0 || element.station.value != 0 ||
               element.fromStation.value != 0 ||
               element.toStation.value != 0 ||
               element.direction != Direction::Unknown) {
      setFailure(report, "unassigned elements must have explicit unknown identities");
      return false;
    }

    for (uint16_t j = 0; j < i; ++j) {
      if (elements[i].id.value == elements[j].id.value) {
        setFailure(report, "duplicate logical map element ID");
        return false;
      }
      if (samePhysicalLed(elements[i].physical, elements[j].physical)) {
        setFailure(report, "duplicate physical LED assignment");
        return false;
      }
    }
  }

  report.assigned = assignedCount;
  report.unassigned = static_cast<uint16_t>(elementCountValue - assignedCount);
  report.routes = 0;
  for (uint16_t i = 0; i < elementCountValue; ++i) {
    if (!elements[i].assigned) {
      continue;
    }
    if (!validAssignedElement(elements[i])) {
      setFailure(report, "assigned map element does not resolve to a valid map LED");
      return false;
    }
    bool routeSeen = false;
    for (uint16_t j = 0; j < i; ++j) {
      if (elements[j].assigned &&
          elements[j].route.value == elements[i].route.value) {
        routeSeen = true;
        break;
      }
    }
    if (!routeSeen) {
      ++report.routes;
    }
  }

  for (uint16_t i = 0; i < elementCountValue; ++i) {
    if (!elements[i].assigned) {
      continue;
    }
    for (uint16_t j = 0; j < i; ++j) {
      if (elements[j].assigned &&
          !validateAssignedPair(elements[j], elements[i],
                                report.failureReason)) {
        return false;
      }
    }
  }
  return true;
}

bool runSelfTest(ValidationReport &report, const char *&failureReason) {
  if (!validate(report)) {
    failureReason = report.failureReason;
    return false;
  }
  if (elementCount() != 302 || report.unassigned != 302 ||
      report.assigned != 0) {
    failureReason = "expected 302 explicitly unassigned map positions";
    return false;
  }

  MapElement first{};
  PhysicalLedRef resolved{};
  if (!elementAt(0, first) || first.id.value == 0 ||
      first.type != MapElementType::Unknown || first.assigned ||
      !resolve(first.id, resolved) ||
      resolved.globalIndex.value != first.physical.globalIndex.value) {
    failureReason = "unknown logical map elements must resolve their physical reference";
    return false;
  }
  bool led291RemainsMapElement = false;
  for (uint16_t index = 0; index < elementCount(); ++index) {
    MapElement element{};
    if (!elementAt(index, element)) {
      failureReason = "logical map inventory lookup failed";
      return false;
    }
    if (element.physical.designator.value == 291 &&
        element.physical.address.channel == led::PhysicalChannel::CH5 &&
        element.physical.address.index == 18 &&
        element.physical.globalIndex.value == 167 &&
        !element.assigned && element.type == MapElementType::Unknown) {
      led291RemainsMapElement = true;
      break;
    }
  }
  if (!led291RemainsMapElement) {
    failureReason = "LED291 must remain an unassigned addressable map element";
    return false;
  }
  MapElement invalid{};
  if (findElement({0}, invalid) || findElement({303}, invalid)) {
    failureReason = "logical map element bounds check failed";
    return false;
  }

  led::map::PhysicalLed physicalA{};
  led::map::PhysicalLed physicalB{};
  if (!led::map::physicalLed({0}, physicalA) ||
      !led::map::physicalLed({1}, physicalB) ||
      physicalA.role != led::LedRole::Map ||
      physicalB.role != led::LedRole::Map) {
    failureReason = "shared-station invariant test could not select map LEDs";
    return false;
  }
  MapElement routeStationFixture[2]{};
  routeStationFixture[0] = {
      {1001},
      MapElementType::Station,
      {1},
      {42},
      {0},
      {0},
      Direction::Unknown,
      {physicalA.address, physicalA.globalIndex, physicalA.designator},
      true};
  routeStationFixture[1] = {
      {1002},
      MapElementType::Station,
      {2},
      {42},
      {0},
      {0},
      Direction::Unknown,
      {physicalB.address, physicalB.globalIndex, physicalB.designator},
      true};
  const char *fixtureFailure = nullptr;
  if (!validateAssignedElements(routeStationFixture, 2, fixtureFailure)) {
    failureReason = "route-specific shared-station fixture was rejected";
    return false;
  }
  routeStationFixture[1].physical = routeStationFixture[0].physical;
  fixtureFailure = nullptr;
  if (validateAssignedElements(routeStationFixture, 2, fixtureFailure)) {
    failureReason = "duplicate physical LED assignment was not rejected";
    return false;
  }

  failureReason = nullptr;
  return true;
}

}  // namespace brooklyn::subway
