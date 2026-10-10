// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

namespace brooklyn::led {

enum class PhysicalChannel : uint8_t {
  CH1 = 1,
  CH2,
  CH3,
  CH4,
  CH5,
  CH6,
  CH7,
  CH8,
};

struct PhysicalAddress {
  PhysicalChannel channel;
  uint8_t index;
};

struct GlobalPhysicalIndex {
  uint16_t value;
};

struct BoardLedDesignator {
  uint16_t value;
};

struct MapElementId {
  uint16_t value;
};

struct SubwayRouteId {
  uint16_t value;
};

enum class LedRole : uint8_t {
  Map,
  Status,
};

enum class StatusLed : uint8_t {
  System,
  Network,
  MtaFeed,
  Mode,
};

struct Rgb {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct DisplayState {
  Rgb color;
};

}  // namespace brooklyn::led
