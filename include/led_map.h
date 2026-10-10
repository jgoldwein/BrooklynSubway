// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_types.h"

#include <stdint.h>

namespace brooklyn::led::map {

constexpr uint8_t CHANNEL_COUNT = 8;
constexpr uint16_t TOTAL_LED_COUNT = 306;

struct PhysicalLed {
  PhysicalAddress address;
  GlobalPhysicalIndex globalIndex;
  BoardLedDesignator designator;
  LedRole role;
};

uint8_t channelLedCount(PhysicalChannel channel);
bool toGlobalIndex(PhysicalAddress address, GlobalPhysicalIndex &result);
bool toPhysicalAddress(GlobalPhysicalIndex index, PhysicalAddress &result);
bool physicalLed(GlobalPhysicalIndex index, PhysicalLed &result);
bool statusLedAddress(StatusLed status, PhysicalAddress &result);
bool statusLedDesignator(StatusLed status, BoardLedDesignator &result);

}  // namespace brooklyn::led::map
