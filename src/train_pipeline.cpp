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
  kApproaching,
};

constexpr uint16_t kConflictBit = 0x8000;
constexpr uint16_t kOutcomeMask = 0x7FFF;
constexpr uint16_t kMaxRecords = 0xFFFF;
static_assert(semantic::kFrameLedCount <= 512, "map LED index must fit 9 bits");

uint16_t packOutcome(Bucket b, uint16_t led) { return static_cast<uint16_t>((b << 9) | led); }

void visitor(void *context, const gtfsrt::RtTrain &train, uint32_t headerTimestamp) {
  static_cast<TrainPipeline *>(context)->onVehicle(train, headerTimestamp);
}
void predictionVisitor(void *context, const gtfsrt::RtTrain &train, uint32_t timestamp) {
  static_cast<TrainPipeline *>(context)->onPrediction(train, timestamp);
}

}  // namespace

void TrainPipeline::beginCycle(uint32_t nowUnix, const semantic::RenderConfig &config) {
  for (auto &location : locations_) location.seen = false;
  blueHistoryCount_ = blueHistoryDropped_ = 0;
  if (finishedForBlueHistory_ && nowUnix > nowUnix_ && nowUnix - nowUnix_ <= 120) {
    for (uint16_t i = 0; i < kTripTableSlots; ++i) {
      const auto &e = trips_[i];
      if (!e.h1 || (e.outcome & kConflictBit) || (e.outcome >> 9) != kStation) continue;
      if (blueHistoryCount_ == kBlueHistoryCapacity) { ++blueHistoryDropped_; continue; }
      auto &saved = blueHistory_[blueHistoryCount_++];
      saved.h1=e.h1; saved.h2=e.h2; saved.led=e.outcome & 0x1FF;
    }
  }
  finishedForBlueHistory_ = false;
  memset(&frame_, 0, sizeof frame_);
  memset(&rgb_, 0, sizeof rgb_);
  memset(trips_, 0, sizeof trips_);
  memset(blueEvidence_, 0, sizeof blueEvidence_);
  memset(&blueReport_, 0, sizeof blueReport_);
  memset(predictions_, 0, sizeof predictions_);
  memset(&counters_, 0, sizeof counters_);
  semantic::clearFrame(frame_);
  frame_.animationEpochUnix = nowUnix;
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

void TrainPipeline::onPrediction(const gtfsrt::RtTrain &train, uint32_t feedTimestamp) {
  const auto route = semantic::routeFromGtfsRouteId(train.routeId);
  const uint32_t arrival = train.nextStopArrival ? train.nextStopArrival : train.nextStopDeparture;
  if (route == semantic::Route::None || !arrival || strlen(train.nextStopId) != 4 || train.fieldTruncated) return;
  const uint32_t h1 = tripHashPrimary(train.tripId), h2 = tripHashSecondary(train.tripId);
  uint16_t slot = h1 & (kTripTableSlots - 1);
  for (uint16_t probe = 0; probe < kTripTableSlots; ++probe) {
    auto &entry = predictions_[slot];
    if (!entry.h1) {
      entry.h1 = h1; entry.h2 = h2; entry.arrival = arrival;
      entry.timestamp = train.tripUpdateTimestamp ? train.tripUpdateTimestamp : feedTimestamp;
      entry.hasArrival = train.nextStopArrival != 0;
      memcpy(entry.stop, train.nextStopId, 4); entry.route = route;
      return;
    }
    if (entry.h1 == h1 && entry.h2 == h2) {
      if (entry.arrival != arrival || entry.route != route || entry.hasArrival != (train.nextStopArrival != 0) || memcmp(entry.stop, train.nextStopId, 4)) entry.conflict = true;
      return;
    }
    slot = (slot + 1) & (kTripTableSlots - 1);
  }
  bump(counters_.predictionCapacityOverflow);
}

void TrainPipeline::record(const char *tripId, uint16_t outcome, const MotionEvidence &motion, const BlueEvidence &blue) {
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
      e.motion = motion;
      blueEvidence_[slot] = blue;
      if ((outcome >> 9) == kStation) {
        for (uint16_t i = 0; i < blueHistoryCount_; ++i) {
          const auto &previous = blueHistory_[i];
          if (previous.h1 == h1 && previous.h2 == h2 && previous.led != (outcome & 0x1FF)) {
            blueEvidence_[slot].flags |= StopChanged; break;
          }
        }
      }
      ++tracked_;
      if (collided) bump(counters_.tripIdentityCollision);
      return;
    }
    if (e.h1 == h1) {
      if (e.h2 == h2) {
        if (e.records < kMaxRecords) ++e.records;
        if ((e.outcome & kOutcomeMask) != outcome) e.outcome |= kConflictBit;
        if (memcmp(&e.motion, &motion, sizeof motion) != 0) e.motion = {};
        auto &saved = blueEvidence_[slot];
        if (saved.stopKey != blue.stopKey) saved.stopKey = 0;
        const bool recent = (saved.flags & RecentMatchingArrival) && (blue.flags & RecentMatchingArrival);
        saved.flags |= blue.flags;
        if (!recent) saved.flags &= ~RecentMatchingArrival;
        if (blue.age > saved.age) saved.age = blue.age;
        if (memcmp(saved.nextStop, blue.nextStop, 4)) memset(saved.nextStop, 0, 4);
        return;
      }
      collided = true;
    }
    slot = static_cast<uint16_t>((slot + 1) & (kTripTableSlots - 1));
  }
  bump(counters_.tripCapacityOverflow);  // unreachable while kMaxTrackedTrips < kTripTableSlots
}

void TrainPipeline::onVehicle(const gtfsrt::RtTrain &vehicle, uint32_t feedTimestamp) {
  gtfsrt::RtTrain t = vehicle;
  BlueEvidence blue{}; blue.flags = NoPrediction;
  if (strlen(vehicle.vehicleStopId) == 4) memcpy(&blue.stopKey, vehicle.vehicleStopId, 4);
  const uint32_t diagnosticTime = feedTimestamp > nowUnix_ ? feedTimestamp : nowUnix_;
  const uint32_t predictionH1 = tripHashPrimary(t.tripId), predictionH2 = tripHashSecondary(t.tripId);
  uint16_t predictionSlot = predictionH1 & (kTripTableSlots - 1);
  for (uint16_t probe = 0; probe < kTripTableSlots; ++probe) {
    const auto &prediction = predictions_[predictionSlot];
    if (!prediction.h1) break;
    if (prediction.h1 == predictionH1 && prediction.h2 == predictionH2) {
      if (!prediction.conflict && prediction.route == semantic::routeFromGtfsRouteId(t.routeId)) {
        memcpy(blue.nextStop, prediction.stop, 4); blue.flags = 0;
        if (memcmp(prediction.stop, vehicle.vehicleStopId, 4)) blue.flags |= NextDifferent;
        else if (diagnosticTime && prediction.arrival > diagnosticTime) blue.flags |= FutureArrival;
        // A matching, recent arrival supports the reported stop; it does not
        // prove dwell. Contradictory/missing/old evidence must not look blue.
        if (prediction.hasArrival && !(blue.flags & NextDifferent) && t.status == gtfsrt::VehicleStatus::StoppedAt &&
            t.vehicleTimestamp && diagnosticTime >= t.vehicleTimestamp &&
            diagnosticTime - t.vehicleTimestamp <= 90 && prediction.timestamp &&
            diagnosticTime >= prediction.timestamp && diagnosticTime - prediction.timestamp <= 60 &&
            prediction.arrival <= diagnosticTime && diagnosticTime - prediction.arrival <= 30 &&
            feedTimestamp && diagnosticTime >= feedTimestamp && diagnosticTime - feedTimestamp <= 60)
          blue.flags |= RecentMatchingArrival;
        char next[5]{}; memcpy(next, prediction.stop, 4);
        const bool inferred = gtfsrt::applyVerifiedTripDeparture(t, next, prediction.arrival,
            prediction.timestamp, feedTimestamp, nowUnix_);
        if (inferred) bump(counters_.verifiedTripDepartures);
        if (strlen(t.vehicleStopId) == 4 && memcmp(prediction.stop, t.vehicleStopId, 4) == 0)
          t.nextStopArrival = prediction.arrival;
      }
      break;
    }
    predictionSlot = (predictionSlot + 1) & (kTripTableSlots - 1);
  }
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
      if (bucket == kStation && o.approachingStation) bucket = kApproaching;
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
    MotionEvidence motion{};
    if (!frame_.animationEpochUnix) frame_.animationEpochUnix = feedTimestamp;
    const uint32_t reference = frame_.animationEpochUnix;
    const uint32_t age = t.vehicleTimestamp && reference >= t.vehicleTimestamp
        ? reference - t.vehicleTimestamp : UINT32_MAX;
    motion.movementAgeSeconds = age < 255 ? age : 255;
    const uint32_t diagnosticAge = t.vehicleTimestamp && diagnosticTime >= t.vehicleTimestamp
        ? diagnosticTime - t.vehicleTimestamp : UINT32_MAX;
    blue.age = diagnosticAge < 255 ? diagnosticAge : 255;
    if (diagnosticAge == UINT32_MAX) blue.flags |= UnknownAge;
    if ((bucket == kApproaching || bucket == kSegment) && t.status != gtfsrt::VehicleStatus::StoppedAt) {
      semantic::GtfsTrainState next{};
      next.routeId = t.routeId; next.stopId = t.vehicleStopId;
      next.status = semantic::GtfsVehicleStatus::StoppedAt;
      uint16_t nextLed = 0;
      if (semantic::validatedGlobalIndex(semantic::mapGtfsTrain(next), nextLed)) {
        motion.approachLedPlusOne = nextLed + 1;
        motion.arrivalSecondsPlusOne = UINT16_MAX;  // moving, ETA unavailable
        if (age <= 90 && t.nextStopArrival >= reference && reference &&
            t.nextStopArrival - reference <= 600)
          motion.arrivalSecondsPlusOne = t.nextStopArrival - reference + 1;
      }
    }
    if (t.status != gtfsrt::VehicleStatus::StoppedAt && age < 30 &&
        o.previous.source != gtfsrt::PreviousSource::None &&
        o.previous.source != gtfsrt::PreviousSource::StaticEquivalent) {
      semantic::GtfsTrainState previous{};
      previous.routeId = t.routeId; previous.stopId = o.previous.stopId;
      previous.status = semantic::GtfsVehicleStatus::StoppedAt;
      uint16_t previousLed = 0;
      if (semantic::validatedGlobalIndex(semantic::mapGtfsTrain(previous), previousLed)) {
        motion.departureLedPlusOne = previousLed + 1;
        motion.departureAgePlusOne = age + 1;
      }
    }
    if (strlen(t.vehicleStopId)==4) memcpy(&blue.stopKey,t.vehicleStopId,4);
    record(t.tripId, packOutcome(bucket, led), motion, blue);
  } else {
    bumpBucket(bucket);
  }
}

void TrainPipeline::bumpBucket(uint16_t bucket) {
  switch (bucket) {
    case kStation: bump(counters_.resolvedStation); break;
    case kApproaching:
      bump(counters_.resolvedStation);
      bump(counters_.approachingStation);
      break;
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
  memset(predictions_, 0, sizeof predictions_);
  gtfsrt::FeedStats predictionStats{};
  (void)gtfsrt::visitTripUpdates(data, length, predictionVisitor, this, predictionStats);
  const gtfsrt::DecodeStatus status = gtfsrt::visitVehicles(data, length, visitor, this, stats);
  bump(counters_.feedsProcessed);
  if (status != gtfsrt::DecodeStatus::Ok) bump(counters_.feedDecodeFailures);
  add(counters_.entitiesSeen, stats.entities);
  add(counters_.malformedEntities, stats.malformedEntities);
  add(counters_.tripUpdatesIgnored, stats.tripUpdates);
  return status;
}

uint16_t TrainPipeline::blueRemaining(uint32_t h1, uint32_t h2, uint32_t stopKey, uint32_t now) {
  if (!now || !stopKey) return 0;
  // Retain visits through missing polls. Reclaim only entries unseen for 30 min.
  StopVisit *empty = nullptr;
  for (auto &visit : stopVisits_) {
    if (visit.h1 && now >= visit.lastSeen && now - visit.lastSeen > 1800) visit = {};
    if (!visit.h1) { if (!empty) empty = &visit; continue; }
    if (visit.h1 != h1 || visit.h2 != h2) continue;
    if (visit.stopKey != stopKey) { visit.stopKey = stopKey; visit.firstSeen = now; }
    visit.lastSeen = now;
    if (now < visit.firstSeen) return 0; // clock rollback never grants extra blue
    const uint32_t age = now - visit.firstSeen;
    return age < config_.blueHoldSeconds ? config_.blueHoldSeconds - age : 0;
  }
  if (!empty) return 0; // capacity uncertainty must not restart an existing timer
  *empty = {h1, h2, stopKey, now, now};
  return config_.blueHoldSeconds;
}

void TrainPipeline::forgetLocation(uint32_t h1,uint32_t h2) {
  for (auto &location : locations_) if (location.h1==h1 && location.h2==h2) location={};
}
void TrainPipeline::rememberLocation(const TripEntry &trip,uint16_t led,bool unknown,uint32_t nextUnknownAt) {
  const uint32_t now=frame_.animationEpochUnix;
  if (!now) return;
  const auto &evidence=blueEvidence_[&trip-trips_];
  if (!evidence.stopKey) { forgetLocation(trip.h1,trip.h2); return; }
  if (evidence.age>90 || (evidence.flags & UnknownAge)) return;
  LocationHistory *empty=nullptr;
  for (auto &location : locations_) {
    if (location.h1 && now>=location.lastSeen && now-location.lastSeen>1800) location={};
    if (!location.h1) { if (!empty) empty=&location; continue; }
    if (location.h1!=trip.h1 || location.h2!=trip.h2) continue;
    if (now<location.lastSeen) { location={}; return; }
    if (location.stopKey!=evidence.stopKey || location.led!=led) {
      location.stopKey=evidence.stopKey; location.led=led; location.unknownSince=0; location.fadeStart=now;
    }
    if (unknown && !location.unknownSince)
      location.unknownSince=location.fadeStart && location.fadeStart<now ? location.fadeStart : now;
    if (!unknown) location.unknownSince=0;
    location.lastSeen=now; location.seen=true;
    location.fadeStart=unknown ? location.unknownSince : nextUnknownAt;
    return;
  }
  if (empty) *empty={trip.h1,trip.h2,evidence.stopKey,now,unknown?now:0,unknown?now:nextUnknownAt,led,true};
}

void TrainPipeline::finishCycle() {
  if (state_ != CycleState::Collecting) return;
  BlueEvidence perLed[semantic::kFrameLedCount]{};
  bool hasBlue[semantic::kFrameLedCount]{};
  blueReport_.timestamp = nowUnix_ ? nowUnix_ : frame_.animationEpochUnix;
  blueReport_.comparedTrips = blueHistoryCount_;
  blueReport_.historyDropped = blueHistoryDropped_;
  for (uint16_t i = 0; i < kTripTableSlots; ++i) {
    const TripEntry &e = trips_[i];
    if (e.h1 == 0) continue;
    if ((e.outcome & kConflictBit) != 0) {
      forgetLocation(e.h1,e.h2);
      bump(counters_.conflictedTrips);
      add(counters_.duplicateConflict, e.records);
      continue;
    }
    add(counters_.duplicateIdentical, static_cast<uint32_t>(e.records) - 1u);
    const uint16_t bucket = static_cast<uint16_t>(e.outcome >> 9);
    const uint16_t remaining = bucket == kStation ? blueRemaining(e.h1, e.h2,
        blueEvidence_[i].stopKey, frame_.animationEpochUnix) : 0;
    if (bucket == kApproaching || bucket == kSegment) {
      for (auto &visit : stopVisits_) if (visit.h1 == e.h1 && visit.h2 == e.h2) visit = {};
    }
    const bool uncertain = bucket == kStation &&
        (!(blueEvidence_[i].flags & RecentMatchingArrival) || !remaining);
    const uint16_t index = e.outcome & 0x1FF;
    if ((bucket == kStation || bucket == kSegment || bucket == kApproaching) &&
        !(uncertain ? semantic::addUncertainLed(frame_, index) :
          semantic::addValidatedLed(frame_, index, bucket == kApproaching))) {
      bump(counters_.frameRejected);
      continue;
    }
    if (bucket==kStation || bucket==kApproaching || bucket==kSegment) {
      const bool movingUnknown=bucket==kApproaching &&
          (!e.motion.arrivalSecondsPlusOne || e.motion.arrivalSecondsPlusOne==UINT16_MAX || e.motion.movementAgeSeconds>90);
      const uint32_t now=frame_.animationEpochUnix;
      const uint32_t nextUnknownAt=bucket==kStation ? now+remaining :
          bucket==kApproaching ? now+(e.motion.movementAgeSeconds<=90 ? 91-e.motion.movementAgeSeconds : 0) :
          now+(blueEvidence_[i].age<=90 ? 91-blueEvidence_[i].age : 0);
      rememberLocation(e,index,uncertain||movingUnknown,nextUnknownAt);
    } else if (bucket!=kStale && bucket!=kInvalid && bucket!=kInsufficient) {
      // Explicit removal/conflict or a changed unmapped stop must not leave a ghost.
      forgetLocation(e.h1,e.h2);
    }
    if (bucket == kStation && !uncertain && remaining > frame_.blueSecondsRemaining[index])
      frame_.blueSecondsRemaining[index] = remaining;
    bumpBucket(bucket);
    if (bucket == kStation) {
      const uint16_t led = e.outcome & 0x1FF;
      const auto *mapped = semantic::semanticForLed(led);
      if (mapped && semantic::routeVisible(config_.routeVisibilityMask, mapped->route)) {
        auto &r = blueReport_.routes[static_cast<uint8_t>(mapped->route)];
        const auto &b = blueEvidence_[i];
        ++r.trains;
        if (!(b.flags & UnknownAge) && b.age > 90) ++r.old;
        if (b.flags & NextDifferent) ++r.different;
        if (b.flags & FutureArrival) ++r.future;
        if (b.flags & NoPrediction) ++r.missing;
        if (b.flags & StopChanged) ++r.changed;
        auto &saved = perLed[led];
        if (!hasBlue[led]) saved = b;
        else {
          saved.flags |= b.flags;
          if (b.age > saved.age) saved.age = b.age;
          if (memcmp(saved.nextStop, b.nextStop, 4)) memset(saved.nextStop, 0, 4);
        }
        hasBlue[led] = true;
      }
    }
    if (e.motion.approachLedPlusOne) {
      auto &timing = frame_.timing[e.motion.approachLedPlusOne - 1];
      if (e.motion.arrivalSecondsPlusOne && (!timing.arrivalSecondsPlusOne ||
          e.motion.arrivalSecondsPlusOne < timing.arrivalSecondsPlusOne)) {
        timing.arrivalSecondsPlusOne = e.motion.arrivalSecondsPlusOne;
        timing.movementAgeSeconds = e.motion.movementAgeSeconds;
      }
    }
    if ((bucket == kStation || bucket == kSegment || bucket == kApproaching) && e.motion.departureLedPlusOne) {
      auto &timing = frame_.timing[e.motion.departureLedPlusOne - 1];
      if (!timing.departureAgePlusOne || e.motion.departureAgePlusOne < timing.departureAgePlusOne)
        timing.departureAgePlusOne = e.motion.departureAgePlusOne;
    }
  }
  for (uint16_t led = 0; led < semantic::kFrameLedCount; ++led) {
    if (!hasBlue[led]) continue;
    const auto *mapped = semantic::semanticForLed(led);
    auto &r = blueReport_.routes[static_cast<uint8_t>(mapped->route)];
    ++r.leds;
    if (semantic::isUncertainOnly(frame_, led)) ++blueReport_.uncertainLeds;
    else if (!semantic::isApproachingOnly(frame_, led)) ++blueReport_.steadyLeds;
    if (frame_.occupancy[led] > 1) ++r.shared;
    const auto &b = perLed[led];
    if (blueReport_.sampleCount < kBlueSamples && ((b.flags & ~RecentMatchingArrival) || b.age > 90)) {
      auto &sample = blueReport_.samples[blueReport_.sampleCount++];
      sample.led = led; sample.age = b.age; sample.flags = b.flags;
      sample.trains = frame_.occupancy[led];
      memcpy(sample.nextStop, b.nextStop, 4); sample.nextStop[4] = '\0';
    }
  }
  for (const auto &location : locations_) {
    if (!location.h1 || location.led>=semantic::kFrameLedCount) continue;
    const uint32_t start=location.seen ? location.fadeStart :
        location.unknownSince ? location.unknownSince : location.lastSeen;
    if (!start) continue;
    auto &saved=frame_.lastReportedFadeStartUnix[location.led];
    if (!saved || start>saved) saved=start;
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
  finishedForBlueHistory_ = true;
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
