// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "gtfs_rt_state.h"

#include "previous_stop.h"

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

namespace {

bool sameParent(const char *a, const char *b) {
  return a[0] != '\0' && b[0] != '\0' && strncmp(a, b, 3) == 0;
}

// The TripUpdate shows a stop the train already left only when its first
// stop_time_update is an event in the past and the second is the stop the
// vehicle is heading to. Never observed in the live MTA feeds (Phase 4F audit);
// kept as a strictly-guarded direct-fact path.
const char *realtimeEarlierStop(const RtTrain &t, const char *stopId) {
  if (!t.hasTripUpdate || t.stopTimeUpdateCount < 2) return nullptr;
  if (!sameParent(t.secondStopId, stopId) || sameParent(t.nextStopId, stopId)) return nullptr;
  const uint32_t event = t.nextStopDeparture != 0 ? t.nextStopDeparture : t.nextStopArrival;
  const uint32_t ref = t.tripUpdateTimestamp != 0 ? t.tripUpdateTimestamp : t.vehicleTimestamp;
  if (event == 0 || ref == 0 || event > ref) return nullptr;
  return t.nextStopId;
}

}  // namespace

bool topologyPreviousStop(void *, const RtTrain &train, const char *stopId, PreviousStop &out) {
  using gtfs_topology::PrevStatus;
  memset(&out, 0, sizeof out);
  out.failure = PreviousFailure::NoPrevious;
  const gtfs_topology::PreviousStopResult r = gtfs_topology::previousStopForRealtimeState(
      train.routeId, stopId, travelDirection(train), realtimeEarlierStop(train, stopId));
  switch (r.status) {
    case PrevStatus::Match:
      memcpy(out.stopId, r.candidates[0], 4);
      out.source = r.source == gtfs_topology::PrevSource::Realtime ? PreviousSource::Realtime
                                                                    : PreviousSource::StaticTopology;
      out.failure = PreviousFailure::None;
      return true;
    case PrevStatus::Ambiguous:
      out.failure = PreviousFailure::Ambiguous;
      out.candidateCount = r.count < kPrevCandidatesMax ? r.count : kPrevCandidatesMax;
      for (uint8_t i = 0; i < out.candidateCount; ++i) memcpy(out.candidates[i], r.candidates[i], 4);
      return false;
    case PrevStatus::UnknownStop: out.failure = PreviousFailure::UnknownDestination; return false;
    default: return false;
  }
}

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
    if (config.previousStop == nullptr) return skip(o, Reason::NoPreviousStop);
    const bool have =
        config.previousStop(config.previousStopContext, t, t.vehicleStopId, o.previous) &&
        o.previous.stopId[0] != '\0';
    if (!have) {
      if (o.previous.failure == PreviousFailure::UnknownDestination) {
        o.category = Category::UnknownStop;
        return;
      }
      if (o.previous.failure != PreviousFailure::Ambiguous || o.previous.candidateCount < 2)
        return skip(o, Reason::NoPreviousStop);
      // Ambiguous: accept only if every candidate yields the identical outcome.
      semantic::GtfsMapResult first{};
      bool same = true;
      for (uint8_t i = 0; i < o.previous.candidateCount && same; ++i) {
        s.previousStopId = o.previous.candidates[i];
        s.status = t.status == VehicleStatus::IncomingAt ? GtfsVehicleStatus::IncomingAt
                                                        : GtfsVehicleStatus::InTransitTo;
        const semantic::GtfsMapResult r = semantic::mapGtfsTrain(s);
        if (i == 0) {
          first = r;
        } else if (r.status != first.status ||
                   (r.status == GtfsMapStatus::Match &&
                    r.resolved.globalIndex != first.resolved.globalIndex)) {
          same = false;
        }
      }
      if (!same) return skip(o, Reason::PreviousStopAmbiguous);
      memcpy(o.previous.stopId, o.previous.candidates[0], 4);
      o.previous.source = PreviousSource::StaticEquivalent;
    }
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
    case Reason::PreviousStopAmbiguous: return "PreviousStopAmbiguous";
  }
  return "?";
}

}  // namespace brooklyn::gtfsrt
