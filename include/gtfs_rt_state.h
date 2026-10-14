// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "gtfs_adapter.h"
#include "gtfs_rt_decoder.h"

#include <stdint.h>

// Phase 4E state builder: RtTrain (decoded GTFS-RT) -> GtfsTrainState ->
// Phase 4D adapter -> Phase 4C resolver. Pure software: no networking, no
// GPIO, no LED output.
//
// Policy (direct GTFS facts only; nothing is estimated):
//   STOPPED_AT                -> StoppedAt at VehiclePosition.stop_id
//   INCOMING_AT/IN_TRANSIT_TO -> segment previous->VehiclePosition.stop_id,
//                                progress = kProgressUnknown
// The realtime feed carries no past stops, so the previous stop comes from a
// caller-supplied PreviousStopProvider (static schedule data). If it cannot
// supply one the train is Skipped(NoPreviousStop); it is never guessed.
namespace brooklyn::gtfsrt {

enum class Category : uint8_t {
  ResolvedStation,
  ResolvedSegment,
  Unrepresented,
  InsufficientPosition,
  UnsupportedRoute,
  UnknownStop,
  Invalid,
  Skipped,
  // Known realtime-only stop that is deliberately not on the map (route H, H19).
  OperationalStop,
};

// True for route-specific realtime-only operational stops (H19, H19N, H19S on
// route H). They are absent from static GTFS and never mapped to an LED.
bool isRealtimeOnlyOperationalStop(const char *routeId, const char *stopId);

enum class Reason : uint8_t {
  None,
  MissingRoute,       // Invalid
  MissingStop,        // Invalid: vehicle has a status but no stop_id
  AdapterInvalid,     // Invalid: adapter rejected the state
  Canceled,           // Skipped: schedule_relationship == CANCELED
  NoVehicle,          // Skipped: TripUpdate only, no VehiclePosition
  NotStarted,         // Skipped: VehiclePosition without current_status
  Stale,              // Skipped: vehicle timestamp too old
  NoPreviousStop,     // Skipped: provider could not supply a previous stop
  PreviousStopAmbiguous,  // Skipped: several static predecessors with different outcomes
};

enum class PreviousSource : uint8_t {
  None,
  ExactTrip,         // host static pattern index, exact trip shape (host only)
  RouteFallback,     // host static pattern index, route/direction (host only)
  Realtime,          // earlier stop from the TripUpdate (Phase 4F)
  StaticTopology,    // unique predecessor from the compiled topology table (Phase 4F)
  StaticEquivalent,  // several predecessors, all with the identical adapter outcome
};

enum class PreviousFailure : uint8_t {
  None,
  NoPrevious,          // no previous stop available (default for a false return)
  Ambiguous,           // `candidates` holds the static predecessors
  UnknownDestination,  // destination stop is not in the compiled topology table
};

constexpr uint8_t kPrevStopMax = 8;
constexpr uint8_t kPrevCandidatesMax = 4;

struct PreviousStop {
  char stopId[kPrevStopMax];
  PreviousSource source;
  PreviousFailure failure;  // set by the provider when it returns false
  uint8_t candidateCount;
  char candidates[kPrevCandidatesMax][4];
};

// Returns true and fills `out` when a unique previous stop is known for
// `train` heading to `stopId` (platform or parent id as sent by the feed).
using PreviousStopProvider = bool (*)(void *context, const RtTrain &train,
                                      const char *stopId, PreviousStop &out);

// Phase 4F firmware-capable provider: realtime earlier stop if the TripUpdate
// shows one, else the compiled static topology table. Needs no context.
bool topologyPreviousStop(void *context, const RtTrain &train, const char *stopId,
                          PreviousStop &out);

struct BuilderConfig {
  uint32_t now;                 // reference time; 0 = use the feed header timestamp
  uint32_t maxVehicleAgeSec;    // older vehicle timestamps are Stale; 0 disables
  PreviousStopProvider previousStop;  // may be null (between-station trains are then skipped)
  void *previousStopContext;
};

constexpr uint32_t kDefaultMaxVehicleAgeSec = 300;

struct Observation {
  Category category;
  Reason reason;
  semantic::GtfsVehicleStatus gtfsStatus;  // normalized status fed to the adapter
  bool adapted;                      // mapGtfsTrain was called
  semantic::GtfsMapResult result;    // valid when `adapted`
  PreviousStop previous;
};

// Evaluates one decoded train. `feedTimestamp` is FeedHeader.timestamp.
void evaluateTrain(const RtTrain &train, uint32_t feedTimestamp,
                   const BuilderConfig &config, Observation &out);

const char *categoryName(Category c);
const char *reasonName(Reason r);

}  // namespace brooklyn::gtfsrt
