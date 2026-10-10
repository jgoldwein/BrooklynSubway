// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_types.h"

#include <stdint.h>

namespace brooklyn::led {

void begin();
void clear();
bool setLed(PhysicalAddress address, Rgb color);
bool setGlobalLed(GlobalPhysicalIndex index, Rgb color);
bool getLed(PhysicalAddress address, Rgb &color);
bool getGlobalLed(GlobalPhysicalIndex index, Rgb &color);
bool setStatusLed(StatusLed status, Rgb color);
bool fillChannel(PhysicalChannel channel, Rgb color);
void setGlobalBrightness(uint16_t brightness);
uint8_t globalBrightness();
void render();
bool runSelfTest();
void runXiaoDemonstration();

}  // namespace brooklyn::led
