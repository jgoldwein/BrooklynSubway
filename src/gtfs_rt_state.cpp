// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "gtfs_rt_state.h"

#include <string.h>

namespace brooklyn::gtfsrt {

using semantic::GtfsMapStatus;
using semantic::GtfsTrainState;
using semantic::GtfsVehicleStatus;
using semantic::PositionKind;

namespace {

constexpr uint8_t kCanceled = 3;  // TripDescriptor.ScheduleRelationship.CANCELED

void skip(Observation &o, Reason r) {
  o.category = Category::Skipped;
  o.reason = r;
}

void invalid(Observation &o, Reason r) {
  o.category = Category::Invalid;
  o.reason = r;
}

}  // namespace

bool isRealtimeOnlyOperationalStop(const char *routeId, const char *stopId) {
  if (routeId == nullptr || stopId == nullptr || strcmp(routeId, "H") != 0) return false;
  return strcmp(stopId, "H19") == 0 || strcmp(stopId, "H19N") == 0 || strcmp(stopId, "H19S") == 0;
}

void evaluateTrain(const RtTrain &t, uint32_t feedTimestamp,
                   const BuilderConfig &config, Observation &o) {
  memset(&o, 0, sizeof o);
  o.category = Category::Invalid;
  o.reason = Reason::None;
  o.gtfsStatus = GtfsVehicleStatus::Unknown;
  o.result.status = GtfsMapStatus::Invalid;

  if (t.routeId[0] == '\0') return invalid(o, Reason::MissingRoute);
  if (semantic::routeFromGtfsRouteId(t.routeId) == semantic::Route::None) {
    o.category = Category::UnsupportedRoute;
    return;
  }
  if (t.scheduleRelationship == kCanceled) return skip(o, Reason::Canceled);
  if (!t.hasVehicle) return skip(o, Reason::NoVehicle);
  if (t.status == VehicleStatus::Absent) return skip(o, Reason::NotStarted);
  if (t.vehicleStopId[0] == '\0') return invalid(o, Reason::MissingStop);

  if (isRealtimeOnlyOperationalStop(t.routeId, t.vehicleStopId)) {
    o.category = Category::OperationalStop;
    return;
  }

  const uint32_t now = config.now != 0 ? config.now : feedTimestamp;
  if (config.maxVehicleAgeSec != 0 && now != 0 && t.vehicleTimestamp != 0 &&
      now > t.vehicleTimestamp && now - t.vehicleTimestamp > config.maxVehicleAgeSec)
    return skip(o, Reason::Stale);

  GtfsTrainState s{};
  s.routeId = t.routeId;
  s.stopId = t.vehicleStopId;
  s.previousStopId = nullptr;
  s.progress = semantic::kProgressUnknown;

  if (t.status == VehicleStatus::StoppedAt) {
    s.status = GtfsVehicleStatus::StoppedAt;
  } else {
    s.status = t.status == VehicleStatus::IncomingAt ? GtfsVehicleStatus::IncomingAt
                                                    : GtfsVehicleStatus::InTransitTo;
    if (config.previousStop == nullptr ||
        !config.previousStop(config.previousStopContext, t, t.vehicleStopId, o.previous) ||
        o.previous.stopId[0] == '\0')
      return skip(o, Reason::NoPreviousStop);
    s.previousStopId = o.previous.stopId;
  }
  o.gtfsStatus = s.status;

  o.result = semantic::mapGtfsTrain(s);
  o.adapted = true;
  switch (o.result.status) {
    case GtfsMapStatus::Match:
      o.category = o.result.position.kind == PositionKind::Segment ? Category::ResolvedSegment
                                                                   : Category::ResolvedStation;
      break;
    case GtfsMapStatus::Unrepresented: o.category = Category::Unrepresented; break;
    case GtfsMapStatus::InsufficientPosition: o.category = Category::InsufficientPosition; break;
    case GtfsMapStatus::UnsupportedRoute: o.category = Category::UnsupportedRoute; break;
    case GtfsMapStatus::UnknownStop: o.category = Category::UnknownStop; break;
    case GtfsMapStatus::Invalid: invalid(o, Reason::AdapterInvalid); break;
  }
}

const char *categoryName(Category c) {
  switch (c) {
    case Category::ResolvedStation: return "ResolvedStation";
    case Category::ResolvedSegment: return "ResolvedSegment";
    case Category::Unrepresented: return "Unrepresented";
    case Category::InsufficientPosition: return "InsufficientPosition";
    case Category::UnsupportedRoute: return "UnsupportedRoute";
    case Category::UnknownStop: return "UnknownStop";
    case Category::Invalid: return "Invalid";
    case Category::Skipped: return "Skipped";
    case Category::OperationalStop: return "OperationalStop";
  }
  return "?";
}

const char *reasonName(Reason r) {
  switch (r) {
    case Reason::None: return "";
    case Reason::MissingRoute: return "MissingRoute";
    case Reason::MissingStop: return "MissingStop";
    case Reason::AdapterInvalid: return "AdapterInvalid";
    case Reason::Canceled: return "Canceled";
    case Reason::NoVehicle: return "NoVehicle";
    case Reason::NotStarted: return "NotStarted";
    case Reason::Stale: return "Stale";
    case Reason::NoPreviousStop: return "NoPreviousStop";
  }
  return "?";
}

}  // namespace brooklyn::gtfsrt
