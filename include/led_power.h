// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_types.h"

#include <stdint.h>

namespace brooklyn::led::power {

constexpr uint16_t NOMINAL_SUPPLY_VOLTAGE_MV = 5000;
constexpr uint16_t NOMINAL_SUPPLY_CURRENT_LIMIT_MA = 5000;
constexpr uint16_t FIRMWARE_LED_CURRENT_BUDGET_MA = 4000;

// Configurable LED-load estimate only: 20 mA per full-scale RGB component is
// not measured for this board and excludes MCU/peripheral current.
constexpr uint8_t ESTIMATED_MA_PER_COLOR_CHANNEL_AT_FULL = 20;

// Separate physical-output bring-up cap; does not change logical brightness.
constexpr uint8_t PRODUCTION_BRINGUP_BRIGHTNESS_CAP = 16;

struct FramePowerReport {
  uint32_t estimatedCurrentMa;
  uint32_t limitedCurrentMa;
  uint8_t appliedScale;
  uint8_t effectiveBrightness;
};

uint32_t estimateFrameCurrentMa(const DisplayState *frame, uint16_t count,
                                uint8_t brightness);
uint8_t brightnessScaleForBudget(const DisplayState *frame, uint16_t count,
                                 uint8_t brightness);
void limitFrameToBudget(const DisplayState *source, uint16_t count,
                        uint8_t requestedBrightness, uint8_t brightnessCap,
                        DisplayState *limited, FramePowerReport &report);

}  // namespace brooklyn::led::power
