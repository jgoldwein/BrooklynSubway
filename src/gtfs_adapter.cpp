// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "gtfs_adapter.h"

#include "generated/gtfs_stop_table.h"

#include <string.h>

namespace brooklyn::semantic {
namespace {

struct RouteIdEntry {
  const char *id;
  Route route;
};

// GTFS route_id values for the 18 mapped routes. "S" is the map's shuttle
// route; the static feed publishes the Brooklyn/Rockaway shuttles as "FS" and
// "H". "GS" (42 St shuttle) is not on this map and stays unsupported.
constexpr RouteIdEntry kRouteIds[] = {
    {"2", Route::Route2}, {"3", Route::Route3}, {"4", Route::Route4},
    {"5", Route::Route5}, {"A", Route::RouteA}, {"B", Route::RouteB},
    {"C", Route::RouteC}, {"D", Route::RouteD}, {"F", Route::RouteF},
    {"G", Route::RouteG}, {"J", Route::RouteJ}, {"L", Route::RouteL},
    {"M", Route::RouteM}, {"N", Route::RouteN}, {"Q", Route::RouteQ},
    {"R", Route::RouteR}, {"S", Route::RouteS}, {"FS", Route::RouteS},
    {"H", Route::RouteS}, {"Z", Route::RouteZ},
};

bool isIdChar(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'); }

bool nonEmpty(const char *text) { return text != nullptr && text[0] != '\0'; }

const GtfsStop *findParent(const char *id3) {
  uint16_t lo = 0;
  uint16_t hi = kGtfsStopCount;
  while (lo < hi) {
    const uint16_t mid = static_cast<uint16_t>((lo + hi) / 2);
    const int cmp = strncmp(kGtfsStops[mid].stopId, id3, 3);
    if (cmp == 0) {
      return &kGtfsStops[mid];
    }
    if (cmp < 0) {
      lo = static_cast<uint16_t>(mid + 1);
    } else {
      hi = mid;
    }
  }
  return nullptr;
}

constexpr ResolveResult kNotQueried{ResolveStatus::Invalid, kNoGlobalIndex, 0, 0,
                                    nullptr};
constexpr TrainMapPosition kNoPosition{Route::None, PositionKind::Invalid, 0, 0,
                                       0, kAnySegmentSlot};

GtfsMapResult failure(GtfsMapStatus status) {
  return GtfsMapResult{status, kNoPosition, kNotQueried};
}

GtfsMapResult fromResolve(const TrainMapPosition &position,
                          const ResolveResult &resolved) {
  GtfsMapStatus status = GtfsMapStatus::Invalid;
  switch (resolved.status) {
    case ResolveStatus::Match:
      status = GtfsMapStatus::Match;
      break;
    case ResolveStatus::NoMatch:
      status = GtfsMapStatus::Unrepresented;
      break;
    case ResolveStatus::Ambiguous:
      status = GtfsMapStatus::InsufficientPosition;
      break;
    case ResolveStatus::Invalid:
      return GtfsMapResult{GtfsMapStatus::Invalid, kNoPosition, resolved};
  }
  return GtfsMapResult{status, position, resolved};
}

}  // namespace

StopLookup normalizeGtfsStop(const char *stopId) {
  constexpr StopLookup kMalformed{StopLookupStatus::Malformed, 0,
                                  PlatformDirection::None};
  if (stopId == nullptr) {
    return kMalformed;
  }
  const size_t length = strlen(stopId);
  if (length != 3 && length != 4) {
    return kMalformed;
  }
  for (size_t i = 0; i < length; ++i) {
    if (!isIdChar(stopId[i])) {
      return kMalformed;
    }
  }

  PlatformDirection platform = PlatformDirection::None;
  if (length == 4) {
    if (stopId[3] == 'N') {
      platform = PlatformDirection::North;
    } else if (stopId[3] == 'S') {
      platform = PlatformDirection::South;
    } else {
      return kMalformed;
    }
  }

  const GtfsStop *parent = findParent(stopId);
  if (parent == nullptr) {
    return StopLookup{StopLookupStatus::Unknown, 0, platform};
  }
  return StopLookup{StopLookupStatus::Ok, parent->stationId, platform};
}

Route routeFromGtfsRouteId(const char *routeId) {
  if (!nonEmpty(routeId)) {
    return Route::None;
  }
  for (const RouteIdEntry &entry : kRouteIds) {
    if (strcmp(entry.id, routeId) == 0) {
      return entry.route;
    }
  }
  return Route::None;
}

uint8_t slotForProgress(uint16_t progress, uint8_t pixelCount) {
  if (pixelCount == 0) {
    return 0;
  }
  const uint32_t slot = static_cast<uint32_t>(progress) * pixelCount / kProgressMax;
  return slot >= pixelCount ? static_cast<uint8_t>(pixelCount - 1)
                            : static_cast<uint8_t>(slot);
}

GtfsMapResult mapGtfsTrain(const GtfsTrainState &state) {
  // Required fields first: missing state is Invalid, never "unsupported".
  if (!nonEmpty(state.routeId) || !nonEmpty(state.stopId)) {
    return failure(GtfsMapStatus::Invalid);
  }
  const bool stopped = state.status == GtfsVehicleStatus::StoppedAt;
  const bool moving = state.status == GtfsVehicleStatus::IncomingAt ||
                      state.status == GtfsVehicleStatus::InTransitTo;
  if (!stopped && !moving) {
    return failure(GtfsMapStatus::Invalid);
  }
  if (moving && !nonEmpty(state.previousStopId)) {
    return failure(GtfsMapStatus::Invalid);
  }
  if (moving && state.progress != kProgressUnknown &&
      state.progress > kProgressMax) {
    return failure(GtfsMapStatus::Invalid);
  }

  const Route route = routeFromGtfsRouteId(state.routeId);
  if (route == Route::None) {
    return failure(GtfsMapStatus::UnsupportedRoute);
  }

  const StopLookup current = normalizeGtfsStop(state.stopId);
  if (current.status == StopLookupStatus::Malformed) {
    return failure(GtfsMapStatus::Invalid);
  }
  if (stopped) {
    if (current.status == StopLookupStatus::Unknown) {
      return failure(GtfsMapStatus::UnknownStop);
    }
    const TrainMapPosition position = stationPosition(route, current.stationId);
    return fromResolve(position, resolveTrainPosition(position));
  }

  const StopLookup previous = normalizeGtfsStop(state.previousStopId);
  if (previous.status == StopLookupStatus::Malformed) {
    return failure(GtfsMapStatus::Invalid);
  }
  if (current.status == StopLookupStatus::Unknown ||
      previous.status == StopLookupStatus::Unknown) {
    return failure(GtfsMapStatus::UnknownStop);
  }
  if (previous.stationId == current.stationId) {
    return failure(GtfsMapStatus::Invalid);
  }

  // Endpoint A = stop just departed, B = stop being approached, so slot
  // numbering follows the direction of travel (Phase 4C reverses as needed).
  const uint8_t pixels =
      segmentPixelCount(route, previous.stationId, current.stationId);
  const TrainMapPosition unslotted =
      segmentPosition(route, previous.stationId, current.stationId);
  if (pixels == 0) {
    return fromResolve(unslotted, resolveTrainPosition(unslotted));
  }
  uint8_t slot = 0;
  if (pixels > 1) {
    if (state.progress == kProgressUnknown) {
      return fromResolve(unslotted, resolveTrainPosition(unslotted));
    }
    slot = slotForProgress(state.progress, pixels);
  }
  const TrainMapPosition position =
      segmentPosition(route, previous.stationId, current.stationId, slot);
  return fromResolve(position, resolveTrainPosition(position));
}

}  // namespace brooklyn::semantic
