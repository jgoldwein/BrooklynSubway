#include "live_display_policy.h"
#include "led_power.h"

namespace brooklyn::display::live {

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
  dirty_ = true;
  return true;
}

void Controller::disable(const Operations &operations) {
  enabled_ = false;
  if (armed_ && operations.stop) operations.stop(operations.context);
  armed_ = false;
}

void Controller::service(uint32_t nowMs, bool powerPressed, bool recovery,
                         const Operations &operations) {
  if (!enabled_) return;
  if (powerPressed || recovery || !safe(nowMs) || !operations.arm ||
      !operations.transmit || !operations.stop) {
    disable(operations);
    return;
  }
  if (!armed_) {
    if (!operations.arm(operations.context)) {
      operations.stop(operations.context);
      enabled_ = false;
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
}

}  // namespace brooklyn::display::live
