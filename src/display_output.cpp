// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "display_output.h"

#include "led_engine.h"
#include "led_map.h"

namespace brooklyn::display::output {
namespace {

static_assert(semantic::kFrameLedCount == led::map::TOTAL_LED_COUNT,
              "RGB and physical addressable frames must have the same size");

constexpr uint16_t kExpectedMapPixels = 302;
constexpr uint16_t kExpectedStatusPixels = 4;

}  // namespace

bool publish(const semantic::RgbFrame *frame) {
  if (frame == nullptr) {
    return false;
  }

  led::DisplayState staged[led::map::TOTAL_LED_COUNT]{};
  uint16_t mapPixels = 0;
  uint16_t statusPixels = 0;
  bool statusSeen[4] = {};

  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    const led::GlobalPhysicalIndex index{global};
    led::PhysicalAddress address{};
    led::GlobalPhysicalIndex roundTrip{};
    led::map::PhysicalLed physical{};
    if (!led::map::toPhysicalAddress(index, address) ||
        !led::map::toGlobalIndex(address, roundTrip) ||
        roundTrip.value != global ||
        !led::map::physicalLed(index, physical) ||
        physical.globalIndex.value != global ||
        physical.address.channel != address.channel ||
        physical.address.index != address.index) {
      return false;
    }

    if (physical.role == led::LedRole::Map) {
      ++mapPixels;
    } else if (physical.role == led::LedRole::Status) {
      ++statusPixels;
      bool matchedStatus = false;
      for (uint8_t status = 0; status < 4; ++status) {
        led::PhysicalAddress expected{};
        led::GlobalPhysicalIndex expectedGlobal{};
        if (led::map::statusLedAddress(static_cast<led::StatusLed>(status),
                                       expected) &&
            led::map::toGlobalIndex(expected, expectedGlobal) &&
            expectedGlobal.value == global) {
          if (statusSeen[status]) {
            return false;
          }
          statusSeen[status] = true;
          matchedStatus = true;
          break;
        }
      }
      if (!matchedStatus) {
        return false;
      }
    } else {
      return false;
    }

    const semantic::Rgb &pixel = frame->pixels[global];
    staged[global].color = {pixel.red, pixel.green, pixel.blue};
  }

  if (mapPixels != kExpectedMapPixels ||
      statusPixels != kExpectedStatusPixels) {
    return false;
  }
  for (bool seen : statusSeen) {
    if (!seen) {
      return false;
    }
  }

  return led::publishFrame(staged, led::map::TOTAL_LED_COUNT);
}

}  // namespace brooklyn::display::output