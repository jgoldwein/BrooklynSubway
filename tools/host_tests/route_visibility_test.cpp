// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_map.h"
#include "operational_display.h"
#include "rgb_renderer.h"
#include "semantic_api.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace semantic = brooklyn::semantic;
namespace display = brooklyn::display;

namespace {

int checks = 0;
int failures = 0;

#define CHECK(condition)                                                   \
  do {                                                                      \
    ++checks;                                                               \
    if (!(condition)) {                                                     \
      ++failures;                                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);           \
    }                                                                       \
  } while (0)

bool isBlack(semantic::Rgb color) {
  return color.red == 0 && color.green == 0 && color.blue == 0;
}

semantic::RgbFrame render(const semantic::TrainFrame &frame, uint8_t brightness,
                          uint32_t mask) {
  semantic::RgbFrame output{};
  semantic::RenderConfig config{};
  config.brightness = brightness;
  config.routeVisibilityMask = mask;
  semantic::renderTrainFrame(frame, output, config);
  return output;
}

semantic::TrainFrame fullyOccupiedFrame() {
  semantic::TrainFrame frame{};
  semantic::clearFrame(frame);
  for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
    const uint16_t global = semantic::kMapLeds[i].globalOrder;
    frame.occupancy[global] = static_cast<uint8_t>(1 + i % 4);
  }
  frame.occupiedLeds = semantic::kMapLedCount;
  frame.resolvedTrains = semantic::kMapLedCount;
  return frame;
}

bool sameRgb(semantic::Rgb left, semantic::Rgb right) {
  return left.red == right.red && left.green == right.green &&
         left.blue == right.blue;
}

void testCanonicalRouteBitMapping() {
  CHECK(semantic::kRouteCount == 18);
  CHECK(semantic::kAllRoutesVisibleMask == 0x3ffff);
  uint32_t seenBits = 0;
  for (uint8_t value = 1; value <= semantic::kRouteCount; ++value) {
    const semantic::Route route = static_cast<semantic::Route>(value);
    const uint32_t bit = semantic::routeVisibilityBit(route);
    CHECK(bit == (uint32_t{1} << (value - 1)));
    CHECK((seenBits & bit) == 0);
    CHECK(semantic::routeVisible(bit, route));
    CHECK(!semantic::routeVisible(semantic::kAllRoutesVisibleMask & ~bit,
                                  route));
    CHECK(semantic::routeName(route) == semantic::kRouteNames[value]);
    CHECK(semantic::kRouteNames[value][0] != '\0');
    seenBits |= bit;
  }
  CHECK(seenBits == semantic::kAllRoutesVisibleMask);
  CHECK(semantic::routeVisibilityBit(semantic::Route::None) == 0);
  CHECK(semantic::routeVisibilityBit(static_cast<semantic::Route>(255)) == 0);
}

void testEveryMapLedHasCanonicalRouteAndRole() {
  uint16_t stations = 0;
  uint16_t segments = 0;
  for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
    const semantic::MapLed &record = semantic::kMapLeds[i];
    const semantic::MapLed *lookup = semantic::semanticForLed(record.globalOrder);
    CHECK(lookup == &record);
    CHECK(semantic::routeVisibilityBit(record.route) != 0);
    CHECK(semantic::routeName(record.route)[0] != '\0');
    if (record.role == semantic::Role::Station) {
      ++stations;
      CHECK(semantic::isStationLed(record.globalOrder));
    } else if (record.role == semantic::Role::Segment) {
      ++segments;
      CHECK(semantic::isSegmentLed(record.globalOrder));
    } else {
      CHECK(false);
    }
  }
  CHECK(stations == semantic::kStationLedCount);
  CHECK(segments == semantic::kSegmentLedCount);
  CHECK(stations == 276);
  CHECK(segments == 26);
  CHECK(semantic::kMapLedCount == 302);
  CHECK(semantic::kPhysicalLedCount == 306);
  CHECK(semantic::kFrameLedCount == semantic::kPhysicalLedCount);
  CHECK(semantic::kFrameLedCount - semantic::kMapLedCount == 4);
  // U23 is a separate GPIO indicator, not part of the addressable RGB frame.
}

void testRendererVisibilityAndBrightness() {
  const semantic::TrainFrame frame = fullyOccupiedFrame();
  const semantic::TrainFrame original = frame;
  semantic::RgbFrame legacyDefaultOutput{};
  semantic::renderTrainFrame(frame, legacyDefaultOutput,
                             semantic::RenderConfig{});
  const semantic::RgbFrame defaultOutput =
      render(frame, semantic::kDefaultRenderBrightness,
             semantic::kAllRoutesVisibleMask);
  CHECK(semantic::RenderConfig{}.routeVisibilityMask ==
        semantic::kAllRoutesVisibleMask);
  CHECK(memcmp(&defaultOutput, &legacyDefaultOutput,
               sizeof(defaultOutput)) == 0);

  uint16_t routeStationPixels[semantic::kRouteCount + 1] = {};
  uint16_t routeSegmentPixels[semantic::kRouteCount + 1] = {};
  for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
    const semantic::MapLed &record = semantic::kMapLeds[i];
    const uint8_t route = static_cast<uint8_t>(record.route);
    if (record.role == semantic::Role::Station) {
      ++routeStationPixels[route];
    } else {
      ++routeSegmentPixels[route];
    }
  }

  for (uint8_t value = 1; value <= semantic::kRouteCount; ++value) {
    const semantic::Route route = static_cast<semantic::Route>(value);
    const uint32_t bit = semantic::routeVisibilityBit(route);
    const uint32_t onlyThisRoute = bit;
    const semantic::RgbFrame onlyRouteOutput = render(frame, 64, onlyThisRoute);
    CHECK(routeStationPixels[value] + routeSegmentPixels[value] > 0);

    uint16_t visibleOnlyCount = 0;
    for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
      const semantic::MapLed &record = semantic::kMapLeds[i];
      const bool belongs = record.route == route;
      const semantic::Rgb expected =
          belongs ? defaultOutput.pixels[record.globalOrder]
                  : semantic::Rgb{0, 0, 0};
      CHECK(sameRgb(onlyRouteOutput.pixels[record.globalOrder], expected));
      if (belongs) {
        ++visibleOnlyCount;
        CHECK(!isBlack(onlyRouteOutput.pixels[record.globalOrder]));
      }
    }
    CHECK(visibleOnlyCount ==
          routeStationPixels[value] + routeSegmentPixels[value]);

    const uint32_t hideThisRoute =
        semantic::kAllRoutesVisibleMask & ~bit;
    const semantic::RgbFrame hiddenOutput = render(frame, 64, hideThisRoute);
    for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
      const semantic::MapLed &record = semantic::kMapLeds[i];
      if (record.route == route) {
        CHECK(isBlack(hiddenOutput.pixels[record.globalOrder]));
      } else {
        CHECK(sameRgb(hiddenOutput.pixels[record.globalOrder],
                      defaultOutput.pixels[record.globalOrder]));
      }
    }
    const semantic::RgbFrame reenabledOutput =
        render(frame, 64, hideThisRoute | bit);
    CHECK(memcmp(&reenabledOutput, &defaultOutput, sizeof(defaultOutput)) == 0);

    const semantic::RgbFrame hiddenAtDefaultBrightness =
        render(frame, 64, hideThisRoute);
    const semantic::RgbFrame visibleAtZeroBrightness =
        render(frame, 0, semantic::kAllRoutesVisibleMask);
    uint16_t firstRouteLed = UINT16_MAX;
    for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
      if (semantic::kMapLeds[i].route == route) {
        firstRouteLed = semantic::kMapLeds[i].globalOrder;
        break;
      }
    }
    CHECK(firstRouteLed != UINT16_MAX);
    CHECK(isBlack(hiddenAtDefaultBrightness.pixels[firstRouteLed]));
    CHECK(sameRgb(onlyRouteOutput.pixels[firstRouteLed],
                  defaultOutput.pixels[firstRouteLed]));
    CHECK(isBlack(visibleAtZeroBrightness.pixels[
        firstRouteLed]));
  }

  const semantic::RgbFrame allHidden = render(frame, 64, 0);
  for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
    CHECK(isBlack(allHidden.pixels[semantic::kMapLeds[i].globalOrder]));
  }
  const semantic::RgbFrame multiHidden = render(
      frame, 64, semantic::kAllRoutesVisibleMask &
                     ~semantic::routeVisibilityBit(semantic::Route::Route2) &
                     ~semantic::routeVisibilityBit(semantic::Route::RouteL));
  for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
    const semantic::MapLed &record = semantic::kMapLeds[i];
    const bool hidden = record.route == semantic::Route::Route2 ||
                        record.route == semantic::Route::RouteL;
    CHECK(hidden ? isBlack(multiHidden.pixels[record.globalOrder])
                 : sameRgb(multiHidden.pixels[record.globalOrder],
                           defaultOutput.pixels[record.globalOrder]));
  }
  CHECK(memcmp(&frame, &original, sizeof(frame)) == 0);
}

void checkOverlayWithAllRoutesHidden(const display::Controller &controller,
                                    uint32_t nowMs,
                                    display::State expectedState) {
  CHECK(controller.state() == expectedState);
  const uint32_t revisionBefore = controller.frameRevision();
  const uint32_t ageBefore = controller.dataAgeMs(nowMs);
  const semantic::RgbFrame visible = render(
      controller.frame(), 64, semantic::kAllRoutesVisibleMask);
  const semantic::RgbFrame noneVisible = render(controller.frame(), 64, 0);
  const display::StatusIndicators indicators{
      true, display::WifiIndicator::Associated, false, true,
      controller.state(), 64};
  semantic::RgbFrame visibleOverlay = visible;
  semantic::RgbFrame hiddenOverlay = noneVisible;
  CHECK(display::applyStatusOverlay(visibleOverlay, indicators));
  CHECK(display::applyStatusOverlay(hiddenOverlay, indicators));

  uint16_t statusCount = 0;
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    if (semantic::semanticForLed(global) != nullptr) continue;
    ++statusCount;
    CHECK(sameRgb(visibleOverlay.pixels[global],
                  hiddenOverlay.pixels[global]));
  }
  CHECK(statusCount == 4);
  for (uint16_t i = 0; i < semantic::kMapLedCount; ++i) {
    CHECK(isBlack(hiddenOverlay.pixels[semantic::kMapLeds[i].globalOrder]));
  }
  CHECK(controller.frameRevision() == revisionBefore);
  CHECK(controller.dataAgeMs(nowMs) == ageBefore);
}

void testStatusOverlayIsIndependentAcrossDisplayStates() {
  display::Controller controller;
  const semantic::TrainFrame fullFrame = fullyOccupiedFrame();
  semantic::TrainFrame partialFrame = fullFrame;
  partialFrame.occupancy[semantic::kMapLeds[0].globalOrder] = 2;

  CHECK(controller.completeCycle(fullFrame, display::CycleQuality::Complete,
                                 0) == display::Event::None);
  checkOverlayWithAllRoutesHidden(controller, 0, display::State::Fresh);
  CHECK(controller.completeCycle(partialFrame, display::CycleQuality::Partial,
                                 30000) == display::Event::PartialHeld);
  checkOverlayWithAllRoutesHidden(controller, 30000,
                                  display::State::DegradedHold);
  CHECK(controller.completeCycle(partialFrame, display::CycleQuality::Partial,
                                 90001) == display::Event::PartialCommitted);
  checkOverlayWithAllRoutesHidden(controller, 90001,
                                  display::State::DegradedPartial);
  CHECK(controller.completeCycle(partialFrame, display::CycleQuality::NoFeeds,
                                 200000) == display::Event::NoFeeds);
  checkOverlayWithAllRoutesHidden(controller, 200000,
                                  display::State::StaleHold);
  CHECK(controller.update(390001) == display::Event::HardStale);
  checkOverlayWithAllRoutesHidden(controller, 390001, display::State::NoData);
  CHECK(!controller.hasFrame());
  CHECK(controller.feedAttempted());
  CHECK(controller.frameRevision() == 3);
}

}  // namespace

int main() {
  CHECK(semantic::selfCheck());
  testCanonicalRouteBitMapping();
  testEveryMapLedHasCanonicalRouteAndRole();
  testRendererVisibilityAndBrightness();
  testStatusOverlayIsIndependentAcrossDisplayStates();
  printf("Phase 4S route visibility tests: %d checks, %d failures\n", checks,
         failures);
  return failures == 0 ? 0 : 1;
}
