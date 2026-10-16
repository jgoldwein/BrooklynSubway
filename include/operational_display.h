// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "rgb_renderer.h"

#include <stdint.h>

namespace brooklyn::display {

constexpr uint32_t kPartialGraceMs = 90000;
constexpr uint32_t kHardStaleLimitMs = 300000;

enum class State : uint8_t {
  WaitingForData,
  Fresh,
  DegradedHold,
  DegradedPartial,
  StaleHold,
  NoData,
};

enum class CycleQuality : uint8_t {
  Complete,
  Partial,
  NoFeeds,
};

enum class Event : uint8_t {
  None,
  PartialHeld,
  PartialCommitted,
  NoFeeds,
  AgedStale,
  HardStale,
};

enum class WifiIndicator : uint8_t {
  Disabled,
  Unavailable,
  Connecting,
  Associated,
  TimeSyncing,
};

struct StatusIndicators {
  bool systemHealthy;
  WifiIndicator wifi;
  bool provisioning;
  bool feedAttempted;
  State displayState;
  uint8_t brightness;
};

class Controller {
 public:
  Controller();

  // Called only after a whole poll cycle finishes; the candidate is copied
  // atomically from the pipeline's independent working frame.
  Event completeCycle(const semantic::TrainFrame &candidate, CycleQuality quality,
                     uint32_t nowMs);

  // Advances stale-state aging without changing state during an active cycle.
  Event update(uint32_t nowMs);

  State state() const { return state_; }
  bool hasFrame() const { return hasFrame_; }
  bool feedAttempted() const { return feedAttempted_; }
  uint32_t frameRevision() const { return frameRevision_; }
  uint32_t dataAgeMs(uint32_t nowMs) const;
  uint32_t completeAgeMs(uint32_t nowMs) const;
  const semantic::TrainFrame &frame() const { return displayed_; }

 private:
  void commit(const semantic::TrainFrame &candidate, uint32_t nowMs);
  bool transition(State next);
  Event advanceStaleness(uint32_t nowMs);

  semantic::TrainFrame displayed_{};
  uint32_t lastCommitMs_ = 0;
  uint32_t lastCompleteMs_ = 0;
  uint32_t frameRevision_ = 0;
  State state_ = State::WaitingForData;
  bool hasFrame_ = false;
  bool hasComplete_ = false;
  bool feedAttempted_ = false;
};

const char *stateName(State state);

// Renders static status colors into the four canonical status LED positions.
// Map pixels are not read or modified; no physical LED API is involved.
bool applyStatusOverlay(semantic::RgbFrame &frame, const StatusIndicators &status);

}  // namespace brooklyn::display
