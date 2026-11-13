// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "led_types.h"
#include <string.h>

namespace brooklyn::led::diagnostic {
enum class Kind : uint8_t { FirstLed, ChannelWalk, AllChannels, AllRoutes };
struct Selection {
  Kind kind = Kind::FirstLed;
  PhysicalChannel channel = PhysicalChannel::CH1;
};
inline bool parseSelection(const char *value, Selection &result) {
  if (value == nullptr) return false;
  if (strcmp(value, "routes") == 0) {
    result.kind = Kind::AllRoutes;
    return true;
  }
  if (strcmp(value, "first") == 0) {
    result.kind = Kind::FirstLed;
    result.channel = PhysicalChannel::CH1;
    return true;
  }
  if (strcmp(value, "all") == 0) {
    result.kind = Kind::AllChannels;
    result.channel = PhysicalChannel::CH1;
    return true;
  }
  if (strlen(value) != 3 || value[0] != 'c' || value[1] != 'h' ||
      value[2] < '1' || value[2] > '8') return false;
  result.kind = Kind::ChannelWalk;
  result.channel = static_cast<PhysicalChannel>(value[2] - '0');
  return true;
}
}
