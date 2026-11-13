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

uint16_t approachPeriodMs(const StationTiming &timing, uint32_t elapsedSeconds) {
  if (!timing.arrivalSecondsPlusOne || timing.arrivalSecondsPlusOne == UINT16_MAX ||
      timing.movementAgeSeconds + elapsedSeconds > 90) return 1000;
  const uint32_t eta = timing.arrivalSecondsPlusOne - 1;
  const uint32_t remaining = eta > elapsedSeconds ? eta - elapsedSeconds : 0;
  return remaining <= 15 ? 334 : remaining <= 30 ? 500 : remaining <= 60 ? 1000 : 2000;
}

uint16_t departurePeriodMs(const StationTiming &timing, uint32_t elapsedSeconds) {
  if (!timing.departureAgePlusOne) return 0;
  const uint32_t age = timing.departureAgePlusOne - 1 + elapsedSeconds;
  return age < 10 ? 500 : age < 20 ? 1000 : age < 30 ? 2000 : 0;
}

Rgb stationMotionColor(StationMotion motion, const StationTiming &timing,
                       uint32_t elapsedSeconds) {
  constexpr Rgb orange{255, 96, 0};
  constexpr Rgb blue{0, 0, 255};
  uint32_t blueWeight = 255;
  if (motion == StationMotion::Approaching) {
    // Unknown/stalled predictions stay orange instead of inventing progress.
    if (!timing.arrivalSecondsPlusOne || timing.arrivalSecondsPlusOne == UINT16_MAX ||
        timing.movementAgeSeconds + elapsedSeconds > 90) return orange;
    const uint32_t eta = timing.arrivalSecondsPlusOne - 1;
    uint32_t remaining = eta > elapsedSeconds ? eta - elapsedSeconds : 0;
    if (remaining > 120) remaining = 120;
    blueWeight = (120 - remaining) * 255 / 120;
  } else if (motion == StationMotion::Departing) {
    uint32_t age = timing.departureAgePlusOne ? timing.departureAgePlusOne - 1 + elapsedSeconds : 0;
    if (age > 30) age = 30;
    blueWeight = (30 - age) * 255 / 30;
  }
  const uint32_t orangeWeight = 255 - blueWeight;
  return Rgb{static_cast<uint8_t>((orange.red * orangeWeight + blue.red * blueWeight + 127) / 255),
             static_cast<uint8_t>((orange.green * orangeWeight + blue.green * blueWeight + 127) / 255),
             static_cast<uint8_t>((orange.blue * orangeWeight + blue.blue * blueWeight + 127) / 255)};
}

RenderResult renderTrainFrame(const TrainFrame &input, RgbFrame &output,
                              const RenderConfig &config) {
  memset(&output, 0, sizeof output);
  RenderResult result{};
  const bool timed = config.animateTraffic && input.animationEpochUnix &&
                     config.nowUnix >= input.animationEpochUnix;
  const uint32_t elapsed = timed ? config.nowUnix - input.animationEpochUnix : 0;

  for (uint16_t i = 0; i < kFrameLedCount; ++i) {
    const bool occupied = input.occupancy[i] != 0;
    const auto &timing = input.timing[i];
    const bool approachOverlay = timed && !occupied && timing.arrivalSecondsPlusOne;
    const uint16_t departure = timed && !occupied ? departurePeriodMs(timing, elapsed) : 0;
    if (!occupied && !approachOverlay && !departure) continue;

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
    if (!routeVisible(config.routeVisibilityMask, semantic->route)) continue;
    if (isApproachingOnly(input, i) || approachOverlay) {
      const uint16_t period = approachPeriodMs(timing, elapsed);
      const bool lit = timed ? config.animationTimeMs % period < period / 2 : config.approachingLit;
      if (!lit) continue;
    } else if (!occupied && departure && config.animationTimeMs % departure >= departure / 2) continue;

    Rgb color = nominalRouteColor(semantic->route);
    if (config.animateTraffic && semantic->role == Role::Station) {
      const auto motion = isApproachingOnly(input, i) || approachOverlay
          ? StationMotion::Approaching : !occupied && departure
          ? StationMotion::Departing : StationMotion::Stopped;
      color = stationMotionColor(motion, timing, elapsed);
    }
    output.pixels[i] = scaleBrightness(color, config.brightness);
    ++result.renderedLeds;
    if (semantic->role == Role::Station) ++result.stationLeds;
    else ++result.segmentLeds;
  }
  return result;
}

}  // namespace brooklyn::semantic
