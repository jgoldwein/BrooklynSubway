// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_power.h"

namespace brooklyn::led::power {
namespace {

constexpr uint64_t BRIGHTNESS_DENOMINATOR = 255ULL * 255ULL;

uint8_t applyScale(uint8_t value, uint16_t scale) {
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale + 127) /
                              255);
}

uint8_t combineBrightness(uint8_t brightness, uint8_t scale) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(brightness) * scale + 127) / 255);
}

}  // namespace

uint32_t estimateFrameCurrentMa(const DisplayState *frame, uint16_t count,
                                uint8_t brightness) {
  if (frame == nullptr || brightness == 0) {
    return 0;
  }

  uint64_t componentSum = 0;
  for (uint16_t index = 0; index < count; ++index) {
    componentSum += frame[index].color.red;
    componentSum += frame[index].color.green;
    componentSum += frame[index].color.blue;
  }

  const uint64_t numerator =
      componentSum * ESTIMATED_MA_PER_COLOR_CHANNEL_AT_FULL * brightness;
  return static_cast<uint32_t>((numerator + BRIGHTNESS_DENOMINATOR - 1) /
                               BRIGHTNESS_DENOMINATOR);
}

uint8_t brightnessScaleForBudget(const DisplayState *frame, uint16_t count,
                                 uint8_t brightness) {
  const uint32_t estimate =
      estimateFrameCurrentMa(frame, count, brightness);
  if (estimate <= FIRMWARE_LED_CURRENT_BUDGET_MA) {
    return 255;
  }

  uint16_t scale = static_cast<uint16_t>(
      (static_cast<uint32_t>(FIRMWARE_LED_CURRENT_BUDGET_MA) * 255) /
      estimate);
  while (scale > 0 &&
         estimateFrameCurrentMa(
             frame, count, combineBrightness(
                               brightness, static_cast<uint8_t>(scale))) >
             FIRMWARE_LED_CURRENT_BUDGET_MA) {
    --scale;
  }
  return static_cast<uint8_t>(scale);
}

void limitFrameToBudget(const DisplayState *source, uint16_t count,
                        uint8_t requestedBrightness, uint8_t brightnessCap,
                        DisplayState *limited, FramePowerReport &report) {
  report = {0, 0, 255, 0};
  if (source == nullptr || limited == nullptr) {
    return;
  }

  const uint8_t effectiveBrightness =
      requestedBrightness < brightnessCap ? requestedBrightness : brightnessCap;
  report.effectiveBrightness = effectiveBrightness;
  report.estimatedCurrentMa =
      estimateFrameCurrentMa(source, count, effectiveBrightness);
  report.appliedScale =
      brightnessScaleForBudget(source, count, effectiveBrightness);
  const uint8_t finalScale =
      combineBrightness(effectiveBrightness, report.appliedScale);

  for (uint16_t index = 0; index < count; ++index) {
    limited[index].color.red = applyScale(source[index].color.red, finalScale);
    limited[index].color.green =
        applyScale(source[index].color.green, finalScale);
    limited[index].color.blue =
        applyScale(source[index].color.blue, finalScale);
  }
  report.limitedCurrentMa =
      estimateFrameCurrentMa(limited, count, 255);
}

}  // namespace brooklyn::led::power
