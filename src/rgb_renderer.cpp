// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "rgb_renderer.h"

#include "led_map.h"
#include "semantic_api.h"

#include <string.h>

namespace brooklyn::semantic {

namespace {

constexpr Rgb kBlack{0, 0, 0};

bool mapIdentityValid(uint16_t index, const MapLed *&semantic) {
  semantic = semanticForLed(index);
  if (semantic == nullptr) return false;
  led::map::PhysicalLed phys{};
  if (!led::map::physicalLed(led::GlobalPhysicalIndex{index}, phys)) return false;
  if (phys.role != led::LedRole::Map) return false;
  if (semantic->globalOrder != index) return false;
  if (semantic->channel != static_cast<uint8_t>(phys.address.channel) ||
      semantic->channelIndex != phys.address.index) {
    return false;
  }
  return semantic->designator == phys.designator.value;
}

}  // namespace

Rgb nominalRouteColor(Route route) {
  switch (route) {
    case Route::Route2:
    case Route::Route3:
      return Rgb{0xEE, 0x35, 0x2E};
    case Route::Route4:
    case Route::Route5:
      return Rgb{0x00, 0x93, 0x3C};
    case Route::RouteA:
    case Route::RouteC:
      return Rgb{0x00, 0x39, 0xA6};
    case Route::RouteB:
    case Route::RouteD:
    case Route::RouteF:
    case Route::RouteM:
      return Rgb{0xFF, 0x63, 0x19};
    case Route::RouteG:
      return Rgb{0x6C, 0xBE, 0x45};
    case Route::RouteJ:
    case Route::RouteZ:
      return Rgb{0x99, 0x66, 0x33};
    case Route::RouteL:
      return Rgb{0xA7, 0xA9, 0xAC};
    case Route::RouteN:
    case Route::RouteQ:
    case Route::RouteR:
      return Rgb{0xFC, 0xCC, 0x0A};
    case Route::RouteS:
      return Rgb{0x80, 0x81, 0x83};
    case Route::None:
    default:
      return kBlack;
  }
}

Rgb scaleBrightness(Rgb color, uint8_t brightness) {
  const uint16_t b = brightness;
  return Rgb{static_cast<uint8_t>((color.red * b + 127) / 255),
             static_cast<uint8_t>((color.green * b + 127) / 255),
             static_cast<uint8_t>((color.blue * b + 127) / 255)};
}

RenderResult renderTrainFrame(const TrainFrame &input, RgbFrame &output,
                              const RenderConfig &config) {
  memset(&output, 0, sizeof output);
  RenderResult result{};

  for (uint16_t i = 0; i < kFrameLedCount; ++i) {
    if (input.occupancy[i] == 0) continue;

    const MapLed *semantic = nullptr;
    if (!mapIdentityValid(i, semantic)) {
      ++result.rejectedLeds;
      if (semantic == nullptr) ++result.occupiedStatusLeds;  // status LEDs have no record
      continue;
    }
    if (semantic->route == Route::None) {
      ++result.rejectedLeds;
      continue;
    }

    output.pixels[i] = scaleBrightness(nominalRouteColor(semantic->route), config.brightness);
    ++result.renderedLeds;
    if (semantic->role == Role::Station) ++result.stationLeds;
    else ++result.segmentLeds;
  }
  return result;
}

}  // namespace brooklyn::semantic
