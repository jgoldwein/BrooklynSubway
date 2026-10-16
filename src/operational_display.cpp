// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "operational_display.h"

#include "led_map.h"

#include <string.h>

namespace brooklyn::display {
namespace {

constexpr semantic::Rgb kBlack{0, 0, 0};
constexpr semantic::Rgb kGreen{0, 255, 0};
constexpr semantic::Rgb kBlue{0, 0, 255};
constexpr semantic::Rgb kRed{255, 0, 0};
constexpr semantic::Rgb kAmber{255, 128, 0};
constexpr semantic::Rgb kCyan{0, 255, 255};
constexpr semantic::Rgb kWhite{255, 255, 255};

semantic::Rgb networkColor(WifiIndicator wifi, bool provisioning) {
  if (provisioning) return kCyan;
  switch (wifi) {
    case WifiIndicator::Disabled: return kBlack;
    case WifiIndicator::Unavailable: return kRed;
    case WifiIndicator::Connecting: return kRed;
    case WifiIndicator::TimeSyncing: return kAmber;
    case WifiIndicator::Associated: return kBlue;
  }
  return kRed;
}

semantic::Rgb feedColor(State state, bool attempted) {
  if (!attempted) return kBlack;
  switch (state) {
    case State::Fresh: return kGreen;
    case State::DegradedHold:
    case State::DegradedPartial:
    case State::StaleHold: return kAmber;
    case State::WaitingForData:
    case State::NoData: return kRed;
  }
  return kRed;
}

}  // namespace

Controller::Controller() { semantic::clearFrame(displayed_); }

bool Controller::transition(State next) {
  if (state_ == next) return false;
  state_ = next;
  return true;
}

void Controller::commit(const semantic::TrainFrame &candidate, uint32_t nowMs) {
  memcpy(&displayed_, &candidate, sizeof displayed_);
  hasFrame_ = true;
  lastCommitMs_ = nowMs;
  ++frameRevision_;
}

Event Controller::completeCycle(const semantic::TrainFrame &candidate, CycleQuality quality,
                                uint32_t nowMs) {
  feedAttempted_ = true;
  if (quality == CycleQuality::Complete) {
    commit(candidate, nowMs);
    hasComplete_ = true;
    lastCompleteMs_ = nowMs;
    transition(State::Fresh);
    return Event::None;
  }

  if (quality == CycleQuality::Partial) {
    if (hasFrame_ && hasComplete_ &&
        static_cast<uint32_t>(nowMs - lastCompleteMs_) <= kPartialGraceMs &&
        static_cast<uint32_t>(nowMs - lastCommitMs_) < kHardStaleLimitMs) {
      transition(State::DegradedHold);
      return Event::PartialHeld;
    }
    commit(candidate, nowMs);
    transition(State::DegradedPartial);
    return Event::PartialCommitted;
  }

  if (hasFrame_ &&
      static_cast<uint32_t>(nowMs - lastCommitMs_) >= kHardStaleLimitMs) {
    semantic::clearFrame(displayed_);
    hasFrame_ = false;
    ++frameRevision_;
    transition(State::NoData);
    return Event::HardStale;
  }
  return transition(hasFrame_ ? State::StaleHold : State::NoData)
             ? Event::NoFeeds
             : Event::None;
}

Event Controller::advanceStaleness(uint32_t nowMs) {
  if (!hasFrame_) return Event::None;
  const uint32_t age = static_cast<uint32_t>(nowMs - lastCommitMs_);
  if (age >= kHardStaleLimitMs) {
    semantic::clearFrame(displayed_);
    hasFrame_ = false;
    ++frameRevision_;
    transition(State::NoData);
    return Event::HardStale;
  }
  if (age > kPartialGraceMs && state_ != State::StaleHold) {
    transition(State::StaleHold);
    return Event::AgedStale;
  }
  return Event::None;
}

Event Controller::update(uint32_t nowMs) { return advanceStaleness(nowMs); }

uint32_t Controller::dataAgeMs(uint32_t nowMs) const {
  return hasFrame_ ? static_cast<uint32_t>(nowMs - lastCommitMs_) : 0;
}

uint32_t Controller::completeAgeMs(uint32_t nowMs) const {
  return hasComplete_ ? static_cast<uint32_t>(nowMs - lastCompleteMs_) : 0;
}

const char *stateName(State state) {
  switch (state) {
    case State::WaitingForData: return "WaitingForData";
    case State::Fresh: return "Fresh";
    case State::DegradedHold: return "DegradedHold";
    case State::DegradedPartial: return "DegradedPartial";
    case State::StaleHold: return "StaleHold";
    case State::NoData: return "NoData";
  }
  return "Unknown";
}

bool applyStatusOverlay(semantic::RgbFrame &frame, const StatusIndicators &status) {
  constexpr led::StatusLed kStatuses[] = {
      led::StatusLed::System, led::StatusLed::Network, led::StatusLed::MtaFeed,
      led::StatusLed::Mode};
  semantic::Rgb colors[] = {
      status.systemHealthy ? kGreen : kRed,
      networkColor(status.wifi, status.provisioning),
      feedColor(status.displayState, status.feedAttempted),
      status.provisioning ? kWhite : kBlack,
  };

  uint16_t indices[4]{};
  for (uint8_t i = 0; i < 4; ++i) {
    led::PhysicalAddress address{};
    led::GlobalPhysicalIndex global{};
    led::map::PhysicalLed physical{};
    if (!led::map::statusLedAddress(kStatuses[i], address) ||
        !led::map::toGlobalIndex(address, global) ||
        !led::map::physicalLed(global, physical) || physical.role != led::LedRole::Status) {
      return false;
    }
    indices[i] = global.value;
  }

  for (uint8_t i = 0; i < 4; ++i) {
    frame.pixels[indices[i]] = semantic::scaleBrightness(colors[i], status.brightness);
  }
  return true;
}

}  // namespace brooklyn::display
