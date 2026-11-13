#include "live_display_policy.h"
#include "led_power.h"
#include <string.h>

namespace brooklyn::display::live {

void Controller::publish(const Snapshot &snapshot) {
  if ((memcmp(snapshot_.frame.occupancy, snapshot.frame.occupancy,
              sizeof(snapshot.frame.occupancy)) != 0 ||
       memcmp(snapshot_.frame.approachingOnly, snapshot.frame.approachingOnly,
              sizeof(snapshot.frame.approachingOnly)) != 0) &&
      stats_.mapChanges != UINT32_MAX) ++stats_.mapChanges;
  snapshot_ = snapshot;
  dirty_ = true;
}

OutputStats Controller::stats() const {
  auto result = stats_;
  result.powerCanResume = powerCanResume_ && !enabled_;
  if (!enabled_) result.blinkingLeds = 0;
  return result;
}

bool Controller::powerPress(uint32_t nowMs, bool pressed) {
  if (!buttonInitialized_) {
    buttonInitialized_ = true;
    buttonReading_ = buttonStable_ = pressed;
    return false;  // A button held at boot must be released first.
  }
  if (pressed != buttonReading_) {
    buttonReading_ = pressed;
    buttonChangedAtMs_ = nowMs;
  }
  if (buttonStable_ != buttonReading_ &&
      static_cast<uint32_t>(nowMs - buttonChangedAtMs_) >= 30) {
    buttonStable_ = buttonReading_;
    return buttonStable_;
  }
  return false;
}

bool Controller::safe(uint32_t nowMs) const {
  return snapshot_.hasFrame && snapshot_.status.systemHealthy &&
      !snapshot_.status.provisioning &&
      snapshot_.status.displayState != State::WaitingForData &&
      snapshot_.status.displayState != State::NoData &&
      static_cast<uint32_t>(nowMs - snapshot_.frameAtMs) < kHardStaleLimitMs;
}

bool Controller::enable(uint32_t nowMs) {
  // An old held frame cannot be used to start a new session.
  if (!safe(nowMs) || static_cast<uint32_t>(nowMs - snapshot_.frameAtMs) > kPartialGraceMs ||
      (snapshot_.status.displayState != State::Fresh &&
                       snapshot_.status.displayState != State::DegradedPartial)) return false;
  enabled_ = true;
  powerCanResume_ = true;
  dirty_ = true;
  return true;
}

void Controller::disable(const Operations &operations) {
  enabled_ = false;
  powerCanResume_ = false;
  if (armed_ && operations.stop) operations.stop(operations.context);
  armed_ = false;
}

void Controller::service(uint32_t nowMs, bool powerPressed, bool recovery,
                         const Operations &operations) {
  const bool toggle = powerPress(nowMs, powerPressed);
  if (recovery || !safe(nowMs) || !operations.arm ||
      !operations.transmit || !operations.stop) {
    disable(operations);
    return;
  }
  if (toggle && powerCanResume_) {
    if (enabled_) {
      disable(operations);
      powerCanResume_ = true;  // A button pause preserves this boot's approval.
    } else {
      (void)enable(nowMs);
    }
  }
  if (!enabled_) return;
  if (!armed_) {
    if (!operations.arm(operations.context)) {
      operations.stop(operations.context);
      enabled_ = false;
      powerCanResume_ = false;
      return;
    }
    armed_ = true;
    dirty_ = true;
  }
  const bool phase = semantic::approachingPhaseLit(nowMs);
  if (!dirty_ && phase == phase_) return;
  semantic::RenderConfig config{};
  config.brightness = snapshot_.brightness < led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP
      ? snapshot_.brightness : led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP;
  config.routeVisibilityMask = snapshot_.routeMask;
  config.approachingLit = phase;
  semantic::RgbFrame rgb{};
  const auto result = semantic::renderTrainFrame(snapshot_.frame, rgb, config);
  auto status = snapshot_.status;
  status.brightness = config.brightness;
  if (result.rejectedLeds != 0 || !applyStatusOverlay(rgb, status)) {
    disable(operations);
    return;
  }
  led::DisplayState source[semantic::kFrameLedCount]{};
  led::DisplayState limited[semantic::kFrameLedCount]{};
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) source[i].color = rgb.pixels[i];
  led::power::FramePowerReport report{};
  // RGB is already capped; apply only the existing total-current estimator.
  led::power::limitFrameToBudget(source, semantic::kFrameLedCount, 255, 255,
                                limited, report);
  if (!operations.transmit(operations.context, limited, semantic::kFrameLedCount)) {
    disable(operations);
    return;
  }
  phase_ = phase;
  dirty_ = false;
  if (stats_.framesWritten != UINT32_MAX) ++stats_.framesWritten;
  stats_.lastWriteAtMs = nowMs;
  stats_.blinkingLeds = 0;
  if (config.brightness != 0) {
    for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
      const auto *mapped = semantic::semanticForLed(i);
      if (mapped && snapshot_.frame.occupancy[i] != 0 &&
          semantic::isApproachingOnly(snapshot_.frame, i) &&
          semantic::routeVisible(config.routeVisibilityMask, mapped->route)) ++stats_.blinkingLeds;
    }
  }
}

}  // namespace brooklyn::display::live
