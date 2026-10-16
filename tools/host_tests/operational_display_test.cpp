// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_map.h"
#include "operational_display.h"
#include "semantic_api.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace display = brooklyn::display;
namespace led = brooklyn::led;
namespace semantic = brooklyn::semantic;

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

bool equalRgb(semantic::Rgb a, semantic::Rgb b) {
  return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

semantic::TrainFrame makeFrame(uint8_t index, uint8_t occupancy) {
  semantic::TrainFrame frame;
  semantic::clearFrame(frame);
  if (occupancy != 0) {
    frame.occupancy[index] = occupancy;
    frame.resolvedTrains = occupancy;
    frame.occupiedLeds = 1;
    frame.excessTrains = static_cast<uint32_t>(occupancy - 1);
    frame.collisionLeds = occupancy > 1 ? 1 : 0;
    frame.maxOccupancy = occupancy;
  }
  return frame;
}

bool blank(const semantic::TrainFrame &frame) {
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    if (frame.occupancy[i] != 0) return false;
  }
  return frame.resolvedTrains == 0 && frame.excessTrains == 0 &&
         frame.occupiedLeds == 0 && frame.collisionLeds == 0 &&
         frame.saturatedTrains == 0 && frame.ignoredTrains == 0 &&
         frame.rejectedTrains == 0 && frame.maxOccupancy == 0;
}

uint16_t statusIndex(led::StatusLed status) {
  led::PhysicalAddress address{};
  led::GlobalPhysicalIndex index{};
  CHECK(led::map::statusLedAddress(status, address));
  CHECK(led::map::toGlobalIndex(address, index));
  led::map::PhysicalLed physical{};
  CHECK(led::map::physicalLed(index, physical));
  CHECK(physical.role == led::LedRole::Status);
  return index.value;
}

void testStartupAndAtomicCommit() {
  display::Controller controller;
  semantic::TrainFrame a = makeFrame(0, 2);
  semantic::TrainFrame b = makeFrame(1, 3);

  CHECK(controller.state() == display::State::WaitingForData);
  CHECK(!controller.hasFrame() && !controller.feedAttempted());
  CHECK(controller.completeCycle(a, display::CycleQuality::Complete, 1000) ==
        display::Event::None);
  CHECK(controller.state() == display::State::Fresh && controller.hasFrame());
  CHECK(memcmp(&controller.frame(), &a, sizeof(a)) == 0);

  const semantic::TrainFrame shown = controller.frame();
  b.occupancy[1] = 4;  // Candidate work during polling is not visible to the controller.
  CHECK(memcmp(&controller.frame(), &shown, sizeof(shown)) == 0);
  CHECK(controller.completeCycle(b, display::CycleQuality::Complete, 2000) ==
        display::Event::None);
  CHECK(controller.state() == display::State::Fresh);
  CHECK(memcmp(&controller.frame(), &b, sizeof(b)) == 0);
  CHECK(controller.frameRevision() == 2);
}

void testStartupQualityAndNoFeeds() {
  semantic::TrainFrame partial = makeFrame(5, 1);
  semantic::TrainFrame blankCandidate = makeFrame(0, 0);
  display::Controller partialController;
  CHECK(partialController.completeCycle(partial, display::CycleQuality::Partial, 42) ==
        display::Event::PartialCommitted);
  CHECK(partialController.state() == display::State::DegradedPartial);
  CHECK(memcmp(&partialController.frame(), &partial, sizeof(partial)) == 0);

  display::Controller emptyController;
  CHECK(emptyController.completeCycle(blankCandidate, display::CycleQuality::NoFeeds, 42) ==
        display::Event::NoFeeds);
  CHECK(emptyController.feedAttempted());
  CHECK(emptyController.state() == display::State::NoData);
  CHECK(!emptyController.hasFrame() && blank(emptyController.frame()));
  CHECK(emptyController.update(1000000) == display::Event::None);
  CHECK(emptyController.state() == display::State::NoData);
}

void testPartialGraceAndBoundaries() {
  semantic::TrainFrame complete = makeFrame(2, 2);
  semantic::TrainFrame partialB = makeFrame(3, 1);
  semantic::TrainFrame partialC = makeFrame(4, 1);
  semantic::TrainFrame partialD = makeFrame(5, 3);
  display::Controller controller;
  controller.completeCycle(complete, display::CycleQuality::Complete, 1000);

  CHECK(controller.completeCycle(partialB, display::CycleQuality::Partial, 31000) ==
        display::Event::PartialHeld);
  CHECK(controller.state() == display::State::DegradedHold);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);
  CHECK(controller.completeCycle(partialC, display::CycleQuality::Partial, 61000) ==
        display::Event::PartialHeld);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);

  CHECK(controller.completeAgeMs(91000) == display::kPartialGraceMs);
  CHECK(controller.completeCycle(partialD, display::CycleQuality::Partial, 90999) ==
        display::Event::PartialHeld);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);
  CHECK(controller.completeCycle(partialD, display::CycleQuality::Partial, 91000) ==
        display::Event::PartialHeld);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);
  CHECK(controller.completeCycle(partialD, display::CycleQuality::Partial, 91001) ==
        display::Event::PartialCommitted);
  CHECK(controller.state() == display::State::DegradedPartial);
  CHECK(memcmp(&controller.frame(), &partialD, sizeof(partialD)) == 0);

  const semantic::TrainFrame partialE = makeFrame(6, 2);
  CHECK(controller.completeCycle(partialE, display::CycleQuality::Partial, 101001) ==
        display::Event::PartialCommitted);
  CHECK(controller.state() == display::State::DegradedPartial);
  CHECK(memcmp(&controller.frame(), &partialE, sizeof(partialE)) == 0);
  CHECK(controller.update(101001 + display::kPartialGraceMs + 1) ==
        display::Event::AgedStale);
}

void testNoFeedsStalenessAndRecovery() {
  semantic::TrainFrame complete = makeFrame(6, 2);
  semantic::TrainFrame candidate = makeFrame(7, 1);
  display::Controller controller;
  controller.completeCycle(complete, display::CycleQuality::Complete, 5000);

  CHECK(controller.completeCycle(candidate, display::CycleQuality::NoFeeds, 35000) ==
        display::Event::NoFeeds);
  CHECK(controller.state() == display::State::StaleHold);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);
  CHECK(controller.update(5000 + display::kHardStaleLimitMs - 1) ==
        display::Event::None);
  CHECK(controller.state() == display::State::StaleHold);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);
  CHECK(controller.update(5000 + display::kHardStaleLimitMs) ==
        display::Event::HardStale);
  CHECK(controller.state() == display::State::NoData);
  CHECK(!controller.hasFrame() && blank(controller.frame()));
  semantic::RgbFrame output{};
  semantic::renderTrainFrame(controller.frame(), output, semantic::RenderConfig{255});
  display::StatusIndicators failed{true, display::WifiIndicator::Unavailable, false,
                                   controller.feedAttempted(), controller.state(), 255};
  CHECK(display::applyStatusOverlay(output, failed));
  CHECK(equalRgb(output.pixels[statusIndex(led::StatusLed::MtaFeed)], {255, 0, 0}));
  CHECK(equalRgb(output.pixels[statusIndex(led::StatusLed::Network)], {255, 0, 0}));
  for (uint16_t i = 0; i < led::map::TOTAL_LED_COUNT; ++i) {
    led::map::PhysicalLed physical{};
    led::map::physicalLed(led::GlobalPhysicalIndex{i}, physical);
    if (physical.role == led::LedRole::Map) CHECK(equalRgb(output.pixels[i], {0, 0, 0}));
  }

  CHECK(controller.completeCycle(candidate, display::CycleQuality::Partial,
                                 5000 + display::kHardStaleLimitMs + 1) ==
        display::Event::PartialCommitted);
  CHECK(controller.state() == display::State::DegradedPartial);
  CHECK(memcmp(&controller.frame(), &candidate, sizeof(candidate)) == 0);
  CHECK(controller.completeCycle(complete, display::CycleQuality::Complete,
                                 5000 + display::kHardStaleLimitMs + 2) ==
        display::Event::None);
  CHECK(controller.state() == display::State::Fresh);
  CHECK(memcmp(&controller.frame(), &complete, sizeof(complete)) == 0);
}

void testTransientDnsReplayAndCollisionPreservation() {
  semantic::TrainFrame completeA = makeFrame(8, 3);
  semantic::TrainFrame partialB = makeFrame(9, 1);
  semantic::TrainFrame partialC = makeFrame(10, 2);
  semantic::TrainFrame partialD = makeFrame(11, 4);
  semantic::TrainFrame completeE = makeFrame(12, 1);
  display::Controller replay;
  uint32_t now = 10000;

  replay.completeCycle(completeA, display::CycleQuality::Complete, now);
  CHECK(memcmp(&replay.frame(), &completeA, sizeof(completeA)) == 0);
  replay.completeCycle(partialB, display::CycleQuality::Partial, now += 30000);
  CHECK(replay.state() == display::State::DegradedHold);
  CHECK(memcmp(&replay.frame(), &completeA, sizeof(completeA)) == 0);
  replay.completeCycle(partialC, display::CycleQuality::Partial, now += 30000);
  CHECK(memcmp(&replay.frame(), &completeA, sizeof(completeA)) == 0);
  replay.completeCycle(partialD, display::CycleQuality::Partial, now += 31001);
  CHECK(replay.state() == display::State::DegradedPartial);
  CHECK(memcmp(&replay.frame(), &partialD, sizeof(partialD)) == 0);
  replay.completeCycle(partialB, display::CycleQuality::NoFeeds, now += 30000);
  CHECK(replay.state() == display::State::StaleHold);
  CHECK(memcmp(&replay.frame(), &partialD, sizeof(partialD)) == 0);
  CHECK(replay.update(now + display::kHardStaleLimitMs) == display::Event::HardStale);
  CHECK(replay.state() == display::State::NoData && blank(replay.frame()));
  replay.completeCycle(completeE, display::CycleQuality::Complete,
                       now += display::kHardStaleLimitMs + 1);
  CHECK(replay.state() == display::State::Fresh);
  CHECK(memcmp(&replay.frame(), &completeE, sizeof(completeE)) == 0);

  display::Controller outage;
  outage.completeCycle(completeA, display::CycleQuality::Complete, 1);
  CHECK(outage.completeCycle(partialB, display::CycleQuality::NoFeeds, 30001) ==
        display::Event::NoFeeds);
  CHECK(memcmp(&outage.frame(), &completeA, sizeof(completeA)) == 0);
  outage.completeCycle(completeE, display::CycleQuality::Complete, 60001);
  CHECK(outage.state() == display::State::Fresh);
  CHECK(memcmp(&outage.frame(), &completeE, sizeof(completeE)) == 0);
}

void testMillisRolloverAndRepeatedCycles() {
  semantic::TrainFrame a = makeFrame(20, 2);
  semantic::TrainFrame b = makeFrame(21, 1);
  display::Controller controller;
  const uint32_t start = UINT32_MAX - 45000u;
  controller.completeCycle(a, display::CycleQuality::Complete, start);
  CHECK(controller.completeAgeMs(start + display::kPartialGraceMs) ==
        display::kPartialGraceMs);
  CHECK(controller.completeCycle(b, display::CycleQuality::Partial,
                                 start + display::kPartialGraceMs) ==
        display::Event::PartialHeld);
  CHECK(memcmp(&controller.frame(), &a, sizeof(a)) == 0);
  CHECK(controller.completeCycle(b, display::CycleQuality::Partial,
                                 start + display::kPartialGraceMs + 1) ==
        display::Event::PartialCommitted);
  CHECK(memcmp(&controller.frame(), &b, sizeof(b)) == 0);
  const uint32_t commitAt = start + display::kPartialGraceMs + 1;
  CHECK(controller.update(commitAt + display::kHardStaleLimitMs - 1) ==
        display::Event::AgedStale);
  CHECK(memcmp(&controller.frame(), &b, sizeof(b)) == 0);
  CHECK(controller.update(commitAt + display::kHardStaleLimitMs) ==
        display::Event::HardStale);
  CHECK(blank(controller.frame()));

  const uint32_t revision = controller.frameRevision();
  for (uint32_t i = 0; i < 100; ++i) {
    controller.completeCycle(a, display::CycleQuality::Complete, commitAt + i);
    CHECK(memcmp(&controller.frame(), &a, sizeof(a)) == 0);
  }
  CHECK(controller.frameRevision() == revision + 100);
}

void testProvisioningDoesNotClearMap() {
  const semantic::TrainFrame train = makeFrame(25, 2);
  display::Controller controller;
  controller.completeCycle(train, display::CycleQuality::Complete, 500);

  semantic::RgbFrame output{};
  semantic::renderTrainFrame(controller.frame(), output, semantic::RenderConfig{255});
  const semantic::Rgb mapPixel = output.pixels[25];
  const semantic::TrainFrame snapshot = controller.frame();
  display::StatusIndicators provisioning{true, display::WifiIndicator::Unavailable, true,
                                         true, controller.state(), 255};
  CHECK(display::applyStatusOverlay(output, provisioning));
  CHECK(equalRgb(output.pixels[25], mapPixel));
  CHECK(memcmp(&controller.frame(), &snapshot, sizeof(snapshot)) == 0);
  CHECK(controller.update(500 + display::kPartialGraceMs) == display::Event::None);
  CHECK(memcmp(&controller.frame(), &snapshot, sizeof(snapshot)) == 0);
  CHECK(controller.update(501 + display::kPartialGraceMs) == display::Event::AgedStale);
  CHECK(controller.state() == display::State::StaleHold);
  CHECK(memcmp(&controller.frame(), &snapshot, sizeof(snapshot)) == 0);
}

void testStatusOverlayAndRenderingBoundary() {
  static_assert(semantic::kFrameLedCount == led::map::TOTAL_LED_COUNT,
                "logical frame remains the canonical physical LED space");
  CHECK(sizeof(semantic::RgbFrame) == 918);

  uint16_t mapCount = 0;
  uint16_t statusCount = 0;
  for (uint16_t i = 0; i < led::map::TOTAL_LED_COUNT; ++i) {
    led::map::PhysicalLed physical{};
    CHECK(led::map::physicalLed(led::GlobalPhysicalIndex{i}, physical));
    if (physical.role == led::LedRole::Map) ++mapCount;
    else ++statusCount;
  }
  CHECK(mapCount == 302 && statusCount == 4);

  semantic::RgbFrame frame{};
  for (semantic::Rgb &pixel : frame.pixels) pixel = semantic::Rgb{17, 31, 47};
  const semantic::RgbFrame before = frame;
  display::StatusIndicators normal{true, display::WifiIndicator::Associated, false,
                                   true, display::State::Fresh, 255};
  CHECK(display::applyStatusOverlay(frame, normal));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::System)], {0, 255, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {0, 0, 255}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::MtaFeed)], {0, 255, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Mode)], {0, 0, 0}));
  uint16_t changed = 0;
  for (uint16_t i = 0; i < led::map::TOTAL_LED_COUNT; ++i) {
    led::map::PhysicalLed physical{};
    led::map::physicalLed(led::GlobalPhysicalIndex{i}, physical);
    if (physical.role == led::LedRole::Map) {
      CHECK(equalRgb(frame.pixels[i], before.pixels[i]));
    } else if (!equalRgb(frame.pixels[i], before.pixels[i])) {
      ++changed;
    }
  }
  CHECK(changed == 4);

  display::StatusIndicators setup{false, display::WifiIndicator::Unavailable, true,
                                  true, display::State::NoData, 64};
  CHECK(display::applyStatusOverlay(frame, setup));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::System)], {64, 0, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {0, 64, 64}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::MtaFeed)], {64, 0, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Mode)], {64, 64, 64}));

  display::StatusIndicators waiting{true, display::WifiIndicator::Disabled, false,
                                    false, display::State::WaitingForData, 255};
  CHECK(display::applyStatusOverlay(frame, waiting));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {0, 0, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::MtaFeed)], {0, 0, 0}));

  display::StatusIndicators stationDown{true, display::WifiIndicator::Unavailable, false,
                                        true, display::State::NoData, 255};
  CHECK(display::applyStatusOverlay(frame, stationDown));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {255, 0, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::MtaFeed)], {255, 0, 0}));
  stationDown.wifi = display::WifiIndicator::Connecting;
  CHECK(display::applyStatusOverlay(frame, stationDown));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {255, 0, 0}));
  stationDown.wifi = display::WifiIndicator::TimeSyncing;
  CHECK(display::applyStatusOverlay(frame, stationDown));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {255, 128, 0}));
  stationDown.displayState = display::State::StaleHold;
  CHECK(display::applyStatusOverlay(frame, stationDown));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::Network)], {255, 128, 0}));
  CHECK(equalRgb(frame.pixels[statusIndex(led::StatusLed::MtaFeed)], {255, 128, 0}));

  semantic::TrainFrame train = makeFrame(0, 1);
  semantic::RgbFrame rendered{};
  semantic::renderTrainFrame(train, rendered, semantic::RenderConfig{255});
  const semantic::Rgb mapColor = rendered.pixels[0];
  const semantic::RgbFrame mapOnly = rendered;
  display::StatusIndicators indicators{true, display::WifiIndicator::TimeSyncing, true,
                                       true, display::State::DegradedPartial, 255};
  CHECK(display::applyStatusOverlay(rendered, indicators));
  CHECK(equalRgb(rendered.pixels[0], mapColor));
  for (uint16_t i = 0; i < led::map::TOTAL_LED_COUNT; ++i) {
    led::map::PhysicalLed physical{};
    led::map::physicalLed(led::GlobalPhysicalIndex{i}, physical);
    if (physical.role == led::LedRole::Map) {
      CHECK(equalRgb(rendered.pixels[i], mapOnly.pixels[i]));
    }
  }
  constexpr led::StatusLed statuses[] = {
      led::StatusLed::System, led::StatusLed::Network,
      led::StatusLed::MtaFeed, led::StatusLed::Mode};
  for (led::StatusLed status : statuses) {
    const uint16_t index = statusIndex(status);
    CHECK(semantic::semanticForLed(index) == nullptr);
    led::map::PhysicalLed physical{};
    CHECK(led::map::physicalLed(led::GlobalPhysicalIndex{index}, physical));
    CHECK(physical.role == led::LedRole::Status);
    CHECK(rendered.pixels[index].red != 0 || rendered.pixels[index].green != 0 ||
          rendered.pixels[index].blue != 0);
  }
}

}  // namespace

int main() {
  printf("sizeof(OperationalDisplay::Controller)=%zu sizeof(TrainFrame)=%zu "
         "sizeof(RgbFrame)=%zu\n",
         sizeof(display::Controller), sizeof(semantic::TrainFrame),
         sizeof(semantic::RgbFrame));
  CHECK(display::kPartialGraceMs == 90000);
  CHECK(display::kHardStaleLimitMs == 300000);
  testStartupAndAtomicCommit();
  testStartupQualityAndNoFeeds();
  testPartialGraceAndBoundaries();
  testNoFeedsStalenessAndRecovery();
  testTransientDnsReplayAndCollisionPreservation();
  testMillisRolloverAndRepeatedCycles();
  testProvisioningDoesNotClearMap();
  testStatusOverlayAndRenderingBoundary();
  printf("Operational display tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
