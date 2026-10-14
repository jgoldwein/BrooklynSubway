// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "gtfs_rt_decoder.h"
#include "rgb_renderer.h"
#include "train_frame.h"

#include <stddef.h>
#include <stdint.h>

// Phase 4I: end-to-end, heap-free train pipeline.
//
//   raw GTFS-RT buffer(s) -> streaming VehiclePosition decode -> state builder
//     (static topology previous stop) -> TrainFrame -> RgbFrame (memory only)
//
// Pure software: no networking, no heap, no host static GTFS, no GPIO, no
// LED_ENABLE, no LED driver or setter/show call. The caller owns the raw feed
// buffers; this module never copies or retains them.
//
// Cycle API (one cycle == one consistent set of feeds):
//   beginCycle(nowUnix, config)   full reset of all cycle state
//   processFeed(data, length)     once per feed, any order
//   finishCycle()                 renders the RgbFrame and freezes counters
//
// Trip identity and duplicate contract
//   Each supported-route trip is identified by a 64-bit pair of independent
//   32-bit hashes of its trip_id (tripHashPrimary / tripHashSecondary). Realtime
//   trip_ids observed in the audit are 11-17 characters (typical 13.5), but an
//   exact table would cost 32 B/trip (kTripIdMax) and the string itself is not
//   retained, so identity is a 64-bit hash plus a collision report:
//     * Different trip_ids whose PRIMARY hashes match but SECONDARY hashes
//       differ are tracked as distinct trips and counted in tripIdentityCollision.
//       They are never treated as duplicates.
//     * A full 64-bit match (~2^-64 per pair) is treated as the same trip. If
//       the two records disagree the trip is rejected as a conflict (fail
//       safe), so such a collision can never draw a wrong train. If they
//       agree they are merged as one duplicate (a train-count undercount at
//       that same map result). Identity is therefore not mathematically exact.
//   A trip's "state" is its resolved outcome (category plus, for resolved
//   trains, the map LED).
//     * Same trip_id, identical outcome: counted once; extra records are
//       duplicateIdentical.
//     * Same trip_id, differing outcomes: the trip is rejected for the whole
//       cycle (conflictedTrips); none of its records (duplicateConflict) is
//       drawn. This is independent of feed and record order.
//   Train contributions are applied to the TrainFrame in finishCycle(), so the
//   result and the counters do not depend on feed order while the trip table
//   is not full. Once kMaxTrackedTrips distinct trips exist, further new trips
//   are dropped (tripCapacityOverflow); which ones are dropped then depends on
//   arrival order.

namespace brooklyn::pipeline {

// Maximum distinct supported-route trips tracked per cycle. The live feeds hold
// roughly 560-600 supported vehicles at once (Phase 4I audit, maximum 581).
constexpr uint16_t kTripTableSlots = 1024;                   // power of two
constexpr uint16_t kMaxTrackedTrips = 896;                   // 7/8 load limit
static_assert((kTripTableSlots & (kTripTableSlots - 1)) == 0, "slots must be a power of two");
static_assert(kMaxTrackedTrips < kTripTableSlots, "table must never fill completely");

// Trip identity hashes (exposed for tests). Both are never used alone for
// equality; primary is FNV-1a, secondary a different multiply/xorshift hash
// seeded with the string length.
uint32_t tripHashPrimary(const char *tripId);
uint32_t tripHashSecondary(const char *tripId);

// One tracked trip: 12 B. h1 == 0 marks an empty slot.
struct TripEntry {
  uint32_t h1;
  uint32_t h2;
  uint16_t outcome;  // packed Outcome + map LED; top bit = conflict
  uint16_t records;  // VehiclePosition records seen for this trip (saturating)
};
static_assert(sizeof(TripEntry) == 12, "TripEntry layout");

// All counters saturate and never wrap. After finishCycle() every vehicle
// record delivered to the pipeline (`vehiclesSeen`) lands in exactly one
// bucket: duplicateIdentical, duplicateConflict, tripCapacityOverflow,
// frameRejected, or one category counter (resolvedStation .. skipped), so those
// buckets sum to vehiclesSeen. Supported-trip category counters are filled by
// finishCycle(); unsupportedRoute is counted as vehicles arrive.
struct Counters {
  // Input
  uint32_t feedsProcessed;
  uint32_t feedDecodeFailures;   // Empty/Truncated/Malformed feed (partial results kept)
  uint32_t entitiesSeen;
  uint32_t vehiclesSeen;         // VehiclePosition records with a trip_id
  uint32_t supportedVehicles;    // subset of vehiclesSeen whose route is on the map
  uint32_t malformedEntities;    // damaged entities / vehicles without trip_id
  uint32_t tripUpdatesIgnored;   // TripUpdate entities (not decoded)
  // Categories
  uint32_t resolvedStation;
  uint32_t resolvedSegment;
  uint32_t unrepresented;
  uint32_t insufficientPosition;
  uint32_t unsupportedRoute;
  uint32_t unknownStop;
  uint32_t invalid;
  uint32_t previousStopAmbiguous;
  uint32_t operationalStop;
  uint32_t stale;
  uint32_t notStarted;
  uint32_t skipped;              // canceled, no previous stop, no vehicle
  // Capacity / dedup
  uint32_t duplicateIdentical;   // extra records of a trip with an identical outcome
  uint32_t duplicateConflict;    // all records of trips whose records disagreed
  uint32_t conflictedTrips;      // trips rejected because their records disagreed
  uint32_t tripIdentityCollision;  // distinct trip_ids sharing a primary hash
  uint32_t tripCapacityOverflow;
  uint32_t frameRejected;        // resolved result refused by the TrainFrame
  // Frame (copied from the TrainFrame by finishCycle)
  uint32_t resolvedTrains;
  uint32_t excessTrains;
  uint16_t occupiedLeds;
  uint16_t collisionLeds;
  uint8_t maxOccupancy;
  // Renderer (set by finishCycle)
  uint16_t rendererRejected;
  uint16_t nonBlackPixels;
  uint16_t statusNonBlackPixels;
};

enum class CycleState : uint8_t { Idle, Collecting, Finished };

class TrainPipeline {
 public:
  // Resets everything (frame, RGB, trip table, counters). nowUnix is a Unix
  // timestamp used for staleness; 0 falls back to each feed's header timestamp.
  void beginCycle(uint32_t nowUnix, const semantic::RenderConfig &config = semantic::RenderConfig{});

  // Decodes one raw feed incrementally. Returns the decoder status; a non-Ok
  // status is counted and whatever was validly decoded before it is kept.
  // Returns Empty without effect if no cycle is collecting.
  gtfsrt::DecodeStatus processFeed(const uint8_t *data, size_t length);

  // Applies the deduplicated trips to the frame and renders it. Safe to call more than once; later calls change nothing.
  void finishCycle();

  CycleState state() const { return state_; }
  const Counters &counters() const { return counters_; }
  const semantic::TrainFrame &frame() const { return frame_; }
  // Non-black pixels only after finishCycle(); all black before.
  const semantic::RgbFrame &rgb() const { return rgb_; }

  // Internal: one decoded VehiclePosition.
  void onVehicle(const gtfsrt::RtTrain &train, uint32_t feedTimestamp);

 private:
  void record(const char *tripId, uint16_t outcome);
  void bumpBucket(uint16_t bucket);

  semantic::TrainFrame frame_;
  semantic::RgbFrame rgb_;
  TripEntry trips_[kTripTableSlots];
  Counters counters_;
  uint32_t nowUnix_;
  uint16_t tracked_;
  semantic::RenderConfig config_;
  CycleState state_;
};

// Startup self-check used by the firmware: runs an empty cycle (no feed data)
// and verifies the pipeline reports an empty frame. Touches no hardware.
bool selfCheckEmptyCycle(TrainPipeline &pipeline);

}  // namespace brooklyn::pipeline
