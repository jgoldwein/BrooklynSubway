// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "train_pipeline.h"

#include "gtfs_rt_state.h"
#include "semantic_api.h"

#include <string.h>

namespace brooklyn::pipeline {

using gtfsrt::Category;
using gtfsrt::Reason;

namespace {

void bump(uint32_t &counter) {
  if (counter != 0xFFFFFFFFu) ++counter;
}

void add(uint32_t &counter, uint32_t n) {
  counter = (0xFFFFFFFFu - counter < n) ? 0xFFFFFFFFu : counter + n;
}

enum Bucket : uint8_t {
  kStation,
  kSegment,
  kUnrepresented,
  kInsufficient,
  kUnsupported,
  kUnknownStop,
  kInvalid,
  kAmbiguous,
  kOperational,
  kStale,
  kNotStarted,
  kSkipped,
  kFrameRejected,
};

constexpr uint16_t kConflictBit = 0x8000;
constexpr uint16_t kOutcomeMask = 0x7FFF;
constexpr uint16_t kMaxRecords = 0xFFFF;
static_assert(semantic::kFrameLedCount <= 512, "map LED index must fit 9 bits");

uint16_t packOutcome(Bucket b, uint16_t led) { return static_cast<uint16_t>((b << 9) | led); }

void visitor(void *context, const gtfsrt::RtTrain &train, uint32_t headerTimestamp) {
  static_cast<TrainPipeline *>(context)->onVehicle(train, headerTimestamp);
}

}  // namespace

void TrainPipeline::beginCycle(uint32_t nowUnix, const semantic::RenderConfig &config) {
  memset(&frame_, 0, sizeof frame_);
  memset(&rgb_, 0, sizeof rgb_);
  memset(trips_, 0, sizeof trips_);
  memset(&counters_, 0, sizeof counters_);
  semantic::clearFrame(frame_);
  nowUnix_ = nowUnix;
  tracked_ = 0;
  config_ = config;
  state_ = CycleState::Collecting;
}

uint32_t tripHashPrimary(const char *s) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (; *s != '\0'; ++s) {
    h ^= static_cast<uint8_t>(*s);
    h *= 16777619u;
  }
  return h != 0 ? h : 1u;  // 0 marks an empty slot
}

uint32_t tripHashSecondary(const char *s) {
  uint32_t h = 0x9E3779B9u;
  uint32_t length = 0;
  for (; *s != '\0'; ++s, ++length) {
    h = (h ^ static_cast<uint8_t>(*s)) * 0x85EBCA6Bu;
    h ^= h >> 13;
  }
  h ^= length * 0xC2B2AE35u;
  h ^= h >> 16;
  return h;
}

void TrainPipeline::record(const char *tripId, uint16_t outcome) {
  const uint32_t h1 = tripHashPrimary(tripId);
  const uint32_t h2 = tripHashSecondary(tripId);
  uint16_t slot = static_cast<uint16_t>(h1 & (kTripTableSlots - 1));
  bool collided = false;
  for (uint16_t probes = 0; probes < kTripTableSlots; ++probes) {
    TripEntry &e = trips_[slot];
    if (e.h1 == 0) {
      if (tracked_ >= kMaxTrackedTrips) {
        bump(counters_.tripCapacityOverflow);
        return;
      }
      e.h1 = h1;
      e.h2 = h2;
      e.outcome = outcome;
      e.records = 1;
      ++tracked_;
      if (collided) bump(counters_.tripIdentityCollision);
      return;
    }
    if (e.h1 == h1) {
      if (e.h2 == h2) {
        if (e.records < kMaxRecords) ++e.records;
        if ((e.outcome & kOutcomeMask) != outcome) e.outcome |= kConflictBit;
        return;
      }
      collided = true;
    }
    slot = static_cast<uint16_t>((slot + 1) & (kTripTableSlots - 1));
  }
  bump(counters_.tripCapacityOverflow);  // unreachable while kMaxTrackedTrips < kTripTableSlots
}

void TrainPipeline::onVehicle(const gtfsrt::RtTrain &t, uint32_t feedTimestamp) {
  bump(counters_.vehiclesSeen);
  const bool supported = semantic::routeFromGtfsRouteId(t.routeId) != semantic::Route::None;
  if (supported) bump(counters_.supportedVehicles);

  gtfsrt::BuilderConfig cfg{};
  cfg.now = nowUnix_;
  cfg.maxVehicleAgeSec = gtfsrt::kDefaultMaxVehicleAgeSec;
  cfg.previousStop = gtfsrt::topologyPreviousStop;
  cfg.previousStopContext = nullptr;

  gtfsrt::Observation o;
  gtfsrt::evaluateTrain(t, feedTimestamp, cfg, o);

  Bucket bucket = kSkipped;
  uint16_t led = 0;
  switch (o.category) {
    case Category::ResolvedStation:
    case Category::ResolvedSegment:
      if (semantic::validatedGlobalIndex(o.result, led))
        bucket = o.category == Category::ResolvedStation ? kStation : kSegment;
      else
        bucket = kFrameRejected;
      break;
    case Category::Unrepresented: bucket = kUnrepresented; break;
    case Category::InsufficientPosition: bucket = kInsufficient; break;
    case Category::UnsupportedRoute: bucket = kUnsupported; break;
    case Category::UnknownStop: bucket = kUnknownStop; break;
    case Category::Invalid: bucket = kInvalid; break;
    case Category::OperationalStop: bucket = kOperational; break;
    case Category::Skipped:
      switch (o.reason) {
        case Reason::PreviousStopAmbiguous: bucket = kAmbiguous; break;
        case Reason::Stale: bucket = kStale; break;
        case Reason::NotStarted: bucket = kNotStarted; break;
        default: bucket = kSkipped; break;
      }
      break;
  }

  if (supported) {
    record(t.tripId, packOutcome(bucket, led));
  } else {
    bumpBucket(bucket);
  }
}

void TrainPipeline::bumpBucket(uint16_t bucket) {
  switch (bucket) {
    case kStation: bump(counters_.resolvedStation); break;
    case kSegment: bump(counters_.resolvedSegment); break;
    case kUnrepresented: bump(counters_.unrepresented); break;
    case kInsufficient: bump(counters_.insufficientPosition); break;
    case kUnsupported: bump(counters_.unsupportedRoute); break;
    case kUnknownStop: bump(counters_.unknownStop); break;
    case kInvalid: bump(counters_.invalid); break;
    case kAmbiguous: bump(counters_.previousStopAmbiguous); break;
    case kOperational: bump(counters_.operationalStop); break;
    case kStale: bump(counters_.stale); break;
    case kNotStarted: bump(counters_.notStarted); break;
    case kFrameRejected: bump(counters_.frameRejected); break;
    default: bump(counters_.skipped); break;
  }
}

gtfsrt::DecodeStatus TrainPipeline::processFeed(const uint8_t *data, size_t length) {
  if (state_ != CycleState::Collecting) return gtfsrt::DecodeStatus::Empty;
  gtfsrt::FeedStats stats;
  const gtfsrt::DecodeStatus status = gtfsrt::visitVehicles(data, length, visitor, this, stats);
  bump(counters_.feedsProcessed);
  if (status != gtfsrt::DecodeStatus::Ok) bump(counters_.feedDecodeFailures);
  add(counters_.entitiesSeen, stats.entities);
  add(counters_.malformedEntities, stats.malformedEntities);
  add(counters_.tripUpdatesIgnored, stats.tripUpdates);
  return status;
}

void TrainPipeline::finishCycle() {
  if (state_ != CycleState::Collecting) return;
  for (uint16_t i = 0; i < kTripTableSlots; ++i) {
    const TripEntry &e = trips_[i];
    if (e.h1 == 0) continue;
    if ((e.outcome & kConflictBit) != 0) {
      bump(counters_.conflictedTrips);
      add(counters_.duplicateConflict, e.records);
      continue;
    }
    add(counters_.duplicateIdentical, static_cast<uint32_t>(e.records) - 1u);
    const uint16_t bucket = static_cast<uint16_t>(e.outcome >> 9);
    if ((bucket == kStation || bucket == kSegment) &&
        !semantic::addValidatedLed(frame_, static_cast<uint16_t>(e.outcome & 0x1FF))) {
      bump(counters_.frameRejected);
      continue;
    }
    bumpBucket(bucket);
  }
  const semantic::RenderResult r = semantic::renderTrainFrame(frame_, rgb_, config_);
  counters_.resolvedTrains = frame_.resolvedTrains;
  counters_.excessTrains = frame_.excessTrains;
  counters_.occupiedLeds = frame_.occupiedLeds;
  counters_.collisionLeds = frame_.collisionLeds;
  counters_.maxOccupancy = frame_.maxOccupancy;
  counters_.rendererRejected = r.rejectedLeds;
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    const bool lit = rgb_.pixels[i].red || rgb_.pixels[i].green || rgb_.pixels[i].blue;
    if (!lit) continue;
    ++counters_.nonBlackPixels;
    if (semantic::semanticForLed(i) == nullptr) ++counters_.statusNonBlackPixels;
  }
  state_ = CycleState::Finished;
}

bool selfCheckEmptyCycle(TrainPipeline &p) {
  p.beginCycle(0);
  const gtfsrt::DecodeStatus s = p.processFeed(nullptr, 0);
  p.finishCycle();
  const Counters &c = p.counters();
  return s == gtfsrt::DecodeStatus::Empty && c.feedDecodeFailures == 1 && c.vehiclesSeen == 0 &&
         c.occupiedLeds == 0 && c.nonBlackPixels == 0 && p.state() == CycleState::Finished;
}

}  // namespace brooklyn::pipeline
