// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_power.h"

#include <string.h>

namespace brooklyn::led::power {

uint32_t estimateFrameCurrentMa(const DisplayState *, uint16_t, uint8_t) {
  return 0;
}

uint8_t brightnessScaleForBudget(const DisplayState *, uint16_t, uint8_t) {
  return 255;
}

void limitFrameToBudget(const DisplayState *source, uint16_t count,
                        uint8_t requestedBrightness, uint8_t,
                        DisplayState *limited, FramePowerReport &report) {
  report = {0, 0, 255, requestedBrightness};
  if (source != nullptr && limited != nullptr) {
    memcpy(limited, source, sizeof(DisplayState) * count);
  }
}

}  // namespace brooklyn::led::power
