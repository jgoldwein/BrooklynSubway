// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "train_frame.h"

#include "led_map.h"
#include "semantic_api.h"

#include <string.h>

namespace brooklyn::semantic {

static_assert(kFrameLedCount == led::map::TOTAL_LED_COUNT,
              "frame must cover exactly the canonical global LED space");

namespace {

bool identityValid(const ResolveResult &r) {
  if (r.globalIndex >= kFrameLedCount || r.semantic == nullptr) return false;

  led::map::PhysicalLed phys{};
  if (!led::map::physicalLed(led::GlobalPhysicalIndex{r.globalIndex}, phys)) return false;
  if (phys.role != led::LedRole::Map) return false;  // status LEDs never occupied

  const MapLed *canonical = semanticForLed(r.globalIndex);
  if (canonical == nullptr || r.semantic != canonical) return false;
  if (canonical->globalOrder != r.globalIndex) return false;
  if (canonical->channel != r.channel || canonical->channelIndex != r.channelIndex) return false;
  if (static_cast<uint8_t>(phys.address.channel) != r.channel ||
      phys.address.index != r.channelIndex) {
    return false;
  }
  if (canonical->designator != phys.designator.value) return false;
  return true;
}

}  // namespace

void clearFrame(TrainFrame &frame) { memset(&frame, 0, sizeof frame); }

namespace {

FrameAdd countLed(TrainFrame &frame, uint16_t globalIndex) {
  frame.approachingOnly[globalIndex / 8] &= ~(uint8_t{1} << (globalIndex % 8));
  frame.uncertainOnly[globalIndex / 8] &= ~(uint8_t{1} << (globalIndex % 8));
  uint8_t &count = frame.occupancy[globalIndex];
  if (frame.resolvedTrains < 0xFFFFFFFFu) ++frame.resolvedTrains;
  if (count == 0) {
    ++frame.occupiedLeds;
  } else {
    if (frame.excessTrains < 0xFFFFFFFFu) ++frame.excessTrains;
  }
  if (count == kOccupancyMax) {
    if (frame.saturatedTrains < 0xFFFF) ++frame.saturatedTrains;
    return FrameAdd::Added;
  }
  ++count;
  if (count == 2) ++frame.collisionLeds;
  if (count > frame.maxOccupancy) frame.maxOccupancy = count;
  return FrameAdd::Added;
}

}  // namespace

FrameAdd addResolvedTrain(TrainFrame &frame, const ResolveResult &resolved) {
  if (resolved.status != ResolveStatus::Match) {
    if (frame.ignoredTrains < 0xFFFF) ++frame.ignoredTrains;
    return FrameAdd::Ignored;
  }
  if (!identityValid(resolved)) {
    if (frame.rejectedTrains < 0xFFFF) ++frame.rejectedTrains;
    return FrameAdd::Rejected;
  }
  return countLed(frame, resolved.globalIndex);
}

FrameAdd addGtfsResult(TrainFrame &frame, const GtfsMapResult &result) {
  if (result.status != GtfsMapStatus::Match) {
    if (frame.ignoredTrains < 0xFFFF) ++frame.ignoredTrains;
    return FrameAdd::Ignored;
  }
  return addResolvedTrain(frame, result.resolved);
}

bool validatedGlobalIndex(const GtfsMapResult &result, uint16_t &globalIndex) {
  if (result.status != GtfsMapStatus::Match || result.resolved.status != ResolveStatus::Match ||
      !identityValid(result.resolved)) {
    return false;
  }
  globalIndex = result.resolved.globalIndex;
  return true;
}

bool addValidatedLed(TrainFrame &frame, uint16_t globalIndex, bool approaching) {
  return addIndicatedLed(frame, globalIndex, approaching ? StationIndication::Approaching : StationIndication::Stopped);
}

bool addIndicatedLed(TrainFrame &frame, uint16_t globalIndex, StationIndication indication) {
  if (globalIndex >= kFrameLedCount || semanticForLed(globalIndex) == nullptr) return false;
  led::map::PhysicalLed phys{};
  if (!led::map::physicalLed(led::GlobalPhysicalIndex{globalIndex}, phys)) return false;
  if (phys.role != led::LedRole::Map) return false;
  // Stronger evidence wins, regardless of contribution order: stopped > approach > uncertain.
  StationIndication chosen = indication;
  if (frame.occupancy[globalIndex]) {
    const auto existing = isUncertainOnly(frame, globalIndex) ? StationIndication::Uncertain :
        isApproachingOnly(frame, globalIndex) ? StationIndication::Approaching : StationIndication::Stopped;
    if (existing == StationIndication::Stopped || indication == StationIndication::Stopped) chosen = StationIndication::Stopped;
    else if (existing == StationIndication::Approaching || indication == StationIndication::Approaching) chosen = StationIndication::Approaching;
  }
  countLed(frame, globalIndex);
  const uint8_t bit = uint8_t{1} << (globalIndex % 8);
  if (chosen == StationIndication::Approaching) frame.approachingOnly[globalIndex / 8] |= bit;
  if (chosen == StationIndication::Uncertain) frame.uncertainOnly[globalIndex / 8] |= bit;
  if (indication == StationIndication::Approaching && frame.approachingTrains != UINT32_MAX) ++frame.approachingTrains;
  if (indication == StationIndication::Uncertain && frame.uncertainTrains != UINT32_MAX) ++frame.uncertainTrains;
  return true;
}

bool isUncertainOnly(const TrainFrame &frame, uint16_t globalIndex) {
  return globalIndex < kFrameLedCount &&
      (frame.uncertainOnly[globalIndex / 8] & (uint8_t{1} << (globalIndex % 8))) != 0;
}

bool isApproachingOnly(const TrainFrame &frame, uint16_t globalIndex) {
  return globalIndex < kFrameLedCount &&
      (frame.approachingOnly[globalIndex / 8] & (uint8_t{1} << (globalIndex % 8))) != 0;
}

size_t composeTrainFrame(TrainFrame &frame, const GtfsMapResult *results, size_t count) {
  clearFrame(frame);
  size_t added = 0;
  for (size_t i = 0; i < count; ++i) {
    if (addGtfsResult(frame, results[i]) == FrameAdd::Added) ++added;
  }
  return added;
}

uint8_t frameOccupancy(const TrainFrame &frame, uint16_t globalIndex) {
  return globalIndex < kFrameLedCount ? frame.occupancy[globalIndex] : 0;
}

bool frameOccupied(const TrainFrame &frame, uint16_t globalIndex) {
  return frameOccupancy(frame, globalIndex) != 0;
}

}  // namespace brooklyn::semantic
