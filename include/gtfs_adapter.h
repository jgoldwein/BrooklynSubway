// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "train_position_resolver.h"

#include <stdint.h>

// Pure software GTFS interpretation layer:
//   GtfsTrainState -> TrainMapPosition -> resolver -> physical map LED identity.
// It has no networking/protobuf dependency and never touches GPIO, LED buffers,
// or LED output routines. A later transport layer fills GtfsTrainState.
namespace brooklyn::semantic {

// ---- Stop / route normalization -----------------------------------------

enum class PlatformDirection : uint8_t {
  None = 0,   // parent stop id given (no platform suffix)
  North = 1,  // platform id "<parent>N"
  South = 2,  // platform id "<parent>S"
};

enum class StopLookupStatus : uint8_t {
  Ok,         // maps to a registry station
  Unknown,    // well-formed, but not in the GTFS stop table
  Malformed,  // null/empty/wrong shape
};

struct StopLookup {
  StopLookupStatus status;
  uint16_t stationId;  // 0 unless Ok
  PlatformDirection platform;
};

// Accepts a 3-char parent stop id ("A41") or a 4-char platform id ("A41N",
// "A41S"). Parent ids are looked up in the generated kGtfsStops table; a
// platform id is accepted only when its first three characters are a known
// parent and the fourth is exactly 'N' or 'S'.
StopLookup normalizeGtfsStop(const char *stopId);

// GTFS route_id -> semantic Route. Route::None for null, empty, or any
// unsupported id (including "GS", "W", "6", "7", "5X", "SI"). Case-sensitive.
Route routeFromGtfsRouteId(const char *routeId);

// ---- Adapter input -------------------------------------------------------

// Mirrors GTFS-RT VehiclePosition.current_status.
enum class GtfsVehicleStatus : uint8_t {
  Unknown = 0,
  IncomingAt = 1,   // approaching stopId
  StoppedAt = 2,    // standing at stopId
  InTransitTo = 3,  // moving toward stopId
};

constexpr uint16_t kProgressUnknown = 0xFFFF;
constexpr uint16_t kProgressMax = 1000;  // permille

struct GtfsTrainState {
  const char *routeId;         // required, e.g. "B", "FS"
  GtfsVehicleStatus status;    // required
  const char *stopId;          // StoppedAt: the stop it is at;
                               // otherwise the stop it is heading to
  const char *previousStopId;  // required unless StoppedAt: stop just departed
  // Optional fraction (0..kProgressMax permille) of the way from previousStopId
  // to stopId. kProgressUnknown when the feed cannot supply it.
  uint16_t progress;
};

// ---- Adapter result ------------------------------------------------------

enum class GtfsMapStatus : uint8_t {
  Match,                 // resolved to exactly one physical map LED
  Unrepresented,         // valid position; the map has no LED for it
  InsufficientPosition,  // multi-pixel segment, no progress to pick a pixel
  UnsupportedRoute,      // route_id is not one of the 18 mapped routes
  UnknownStop,           // well-formed stop id not in the GTFS stop table
  Invalid,               // missing/malformed/contradictory input
};

struct GtfsMapResult {
  GtfsMapStatus status;
  // The normalized query. Meaningful for Match, Unrepresented and
  // InsufficientPosition; kind == Invalid otherwise.
  TrainMapPosition position;
  // Resolver outcome for `position`; ResolveStatus::Invalid when never queried.
  ResolveResult resolved;
};

// For a multi-pixel segment: slot = min(progress * pixels / kProgressMax,
// pixels - 1), counted from previousStopId toward stopId.
uint8_t slotForProgress(uint16_t progress, uint8_t pixelCount);

GtfsMapResult mapGtfsTrain(const GtfsTrainState &state);

}  // namespace brooklyn::semantic
