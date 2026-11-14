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
                       uint32_t elapsedSeconds, const TrainColors &colors) {
  if (motion==StationMotion::Approaching) return colors.approaching;
  if (motion==StationMotion::Stopped) return colors.stopped;
  constexpr uint32_t duration=30;
  uint32_t age=timing.departureAgePlusOne ? timing.departureAgePlusOne-1+elapsedSeconds : 0;
  if (age>duration) age=duration;
  return scaleBrightness(colors.leaving,static_cast<uint8_t>((duration-age)*255/duration));
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
    const bool lastMode=config.showUnknownTiming && config.unknownTimingMode==UnknownTimingMode::LastReported;
    const uint32_t hintStart=input.lastReportedFadeStartUnix[i];
    const bool hintOverlay=timed && lastMode && !occupied && !approachOverlay && !departure && hintStart && config.nowUnix>=hintStart;
    if (!occupied && !approachOverlay && !departure && !hintOverlay) continue;

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
    // Arrival/departure indication is continuous color only: no off phase.

    Rgb color = nominalRouteColor(semantic->route);
    if (config.animateTraffic && semantic->role == Role::Station) {
      const auto motion = isApproachingOnly(input, i) || approachOverlay
          ? StationMotion::Approaching : !occupied && departure
          ? StationMotion::Departing : StationMotion::Stopped;
      color = stationMotionColor(motion, timing, elapsed, config.colors);
    }
    if (config.animateTraffic && semantic->role==Role::Segment) color=config.colors.approaching;
    const bool uncertain = isUncertainOnly(input, i) ||
        (occupied && !isApproachingOnly(input, i) && input.blueSecondsRemaining[i] &&
         elapsed >= input.blueSecondsRemaining[i]);
    const bool unknownApproach = (isApproachingOnly(input, i) || approachOverlay) &&
        (!timing.arrivalSecondsPlusOne || timing.arrivalSecondsPlusOne == UINT16_MAX ||
         timing.movementAgeSeconds + elapsed > 90);
    const bool staleSegment=lastMode && timed && occupied && semantic->role==Role::Segment &&
        (hintStart ? config.nowUnix>=hintStart : elapsed>90);
    const bool unknown = hintOverlay || staleSegment || (config.animateTraffic && semantic->role == Role::Station &&
        (uncertain || unknownApproach));
    if (unknown && (!config.showUnknownTiming || config.unknownTimingMode==UnknownTimingMode::Hidden)) continue;
    if (unknown) color=config.colors.uncertain;
    if (config.animateTraffic && uncertain) color=scaleBrightness(config.colors.uncertain,128);
    const uint8_t brightness = unknown && config.unknownTimingBrightness < config.brightness
        ? config.unknownTimingBrightness : config.brightness;
    if (unknown && lastMode) {
      if (!timed || !hintStart || config.nowUnix<hintStart || !config.lastReportedFadeSeconds) continue;
      const uint32_t age=config.nowUnix-hintStart;
      if (age>=config.lastReportedFadeSeconds) continue;
      const uint8_t limit=brightness<2 ? brightness : 2; // Faint neutral marker, still subject to all output caps.
      const uint32_t remaining=config.lastReportedFadeSeconds-age;
      const uint8_t level=(uint32_t(limit)*remaining+config.lastReportedFadeSeconds-1)/config.lastReportedFadeSeconds;
      output.pixels[i]=scaleBrightness(config.colors.uncertain,level);
    } else output.pixels[i] = scaleBrightness(color, brightness);
    ++result.renderedLeds;
    if (semantic->role == Role::Station) ++result.stationLeds;
    else ++result.segmentLeds;
  }
  return result;
}

}  // namespace brooklyn::semantic
