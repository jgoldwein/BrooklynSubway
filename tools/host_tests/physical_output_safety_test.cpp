// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "physical_output_safety.h"

#include "led_power.h"

#include <stdint.h>
#include <stdio.h>

namespace led = brooklyn::led;
namespace safety = brooklyn::led::safety;

namespace {

int checks = 0;
int failures = 0;

#define CHECK(condition)                                                  \
  do {                                                                     \
    ++checks;                                                              \
    if (!(condition)) {                                                    \
      ++failures;                                                          \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
    }                                                                      \
  } while (0)

enum class Event : uint8_t {
  EnableLow,
  BackendInitialize,
  DataLow,
  PrepareBlack,
  EnableHigh,
  TransmitBlack,
  BlackComplete,
  Armed,
  TransmitColor,
};

struct FakeTransport {
  Event events[128]{};
  uint8_t eventCount = 0;
  bool failInitialize = false;
  bool failPrepareBlack = false;
  bool failBlackTransmit = false;
  bool failColorTransmit = false;
  bool failEnableHigh = false;
  bool retainedColor = false;
  safety::Controller *controller = nullptr;
  const safety::Operations *operations = nullptr;
};

void record(FakeTransport &fake, Event event) {
  if (fake.eventCount < sizeof(fake.events) / sizeof(fake.events[0])) {
    fake.events[fake.eventCount++] = event;
  } else {
    ++failures;
  }
}

void clearEvents(FakeTransport &fake) {
  fake.eventCount = 0;
}

bool enable(void *context, bool value) {
  auto &fake = *static_cast<FakeTransport *>(context);
  if (value) {
    record(fake, Event::EnableHigh);
    return !fake.failEnableHigh;
  }
  record(fake, Event::EnableLow);
  return true;
}

bool initializeBackend(void *context) {
  auto &fake = *static_cast<FakeTransport *>(context);
  record(fake, Event::BackendInitialize);
  return !fake.failInitialize;
}

bool dataLow(void *context) {
  record(*static_cast<FakeTransport *>(context), Event::DataLow);
  return true;
}

bool prepareBlack(void *context, const led::DisplayState *black,
                  led::DisplayState *limited, uint16_t count) {
  auto &fake = *static_cast<FakeTransport *>(context);
  record(fake, Event::PrepareBlack);
  CHECK(safety::isBlackFrame(black, count));
  led::power::FramePowerReport report{};
  led::power::limitFrameToBudget(black, count, 255,
                                 led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP,
                                 limited, report);
  CHECK(report.effectiveBrightness ==
        led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP);
  CHECK(report.limitedCurrentMa == 0);
  CHECK(safety::isBlackFrame(limited, count));
  return !fake.failPrepareBlack;
}

bool transmit(void *context, const led::DisplayState *frame, uint16_t count) {
  auto &fake = *static_cast<FakeTransport *>(context);
  if (safety::isBlackFrame(frame, count)) {
    record(fake, Event::TransmitBlack);
    CHECK(count == led::map::TOTAL_LED_COUNT);
    for (uint8_t channel = 1; channel <= led::map::CHANNEL_COUNT; ++channel) {
      const uint8_t channelCount =
          led::map::channelLedCount(static_cast<led::PhysicalChannel>(channel));
      CHECK(channelCount > 0);
      led::PhysicalAddress first{};
      led::PhysicalAddress last{};
      uint16_t start = 0;
      for (uint8_t prior = 1; prior < channel; ++prior) {
        start = static_cast<uint16_t>(
            start + led::map::channelLedCount(
                        static_cast<led::PhysicalChannel>(prior)));
      }
      CHECK(led::map::toPhysicalAddress({start}, first));
      CHECK(led::map::toPhysicalAddress(
          {static_cast<uint16_t>(start + channelCount - 1)}, last));
      CHECK(first.channel == static_cast<led::PhysicalChannel>(channel));
      CHECK(last.channel == static_cast<led::PhysicalChannel>(channel));
      for (uint16_t index = start; index < start + channelCount; ++index) {
        const led::Rgb color = frame[index].color;
        CHECK(color.red == 0 && color.green == 0 && color.blue == 0);
      }
    }
    if (fake.controller != nullptr && fake.operations != nullptr) {
      led::DisplayState color[led::map::TOTAL_LED_COUNT]{};
      color[0].color.red = 1;
      CHECK(fake.controller->state() == safety::State::ClearingBlack);
      CHECK(!fake.controller->transmit(*fake.operations, true, color,
                                       led::map::TOTAL_LED_COUNT));
    }
    if (fake.failBlackTransmit) {
      return false;
    }
    fake.retainedColor = false;
    record(fake, Event::BlackComplete);
    return true;
  }

  record(fake, Event::TransmitColor);
  if (fake.failColorTransmit) {
    return false;
  }
  fake.retainedColor = true;
  return true;
}

safety::Operations operationsFor(FakeTransport &fake) {
  return {&fake, enable, initializeBackend, dataLow, prepareBlack, transmit};
}

bool containsEvent(const FakeTransport &fake, Event event) {
  for (uint8_t index = 0; index < fake.eventCount; ++index) {
    if (fake.events[index] == event) {
      return true;
    }
  }
  return false;
}

void checkExactStartSequence(const FakeTransport &fake) {
  const Event expected[] = {
      Event::EnableLow,       Event::BackendInitialize, Event::DataLow,
      Event::PrepareBlack,    Event::EnableHigh,        Event::TransmitBlack,
      Event::BlackComplete,   Event::Armed,
  };
  CHECK(fake.eventCount == sizeof(expected) / sizeof(expected[0]));
  if (fake.eventCount == sizeof(expected) / sizeof(expected[0])) {
    for (uint8_t index = 0; index < fake.eventCount; ++index) {
      CHECK(fake.events[index] == expected[index]);
    }
  }
}

void testColdStartAndPreArmRejections() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  led::DisplayState color[led::map::TOTAL_LED_COUNT]{};
  color[0].color.red = 20;

  CHECK(controller.state() == safety::State::Disabled);
  CHECK(!controller.transmit(operations, true, color,
                             led::map::TOTAL_LED_COUNT));
  CHECK(!containsEvent(fake, Event::TransmitColor));
  CHECK(controller.initialize(operations, true));
  CHECK(controller.state() == safety::State::BackendReady);
  CHECK(!controller.transmit(operations, true, color,
                             led::map::TOTAL_LED_COUNT));
  CHECK(!controller.arm(operations, false));
  CHECK(controller.state() == safety::State::BackendReady);
  CHECK(!containsEvent(fake, Event::TransmitColor));
}

void testBlackFirstOrderingAndNormalFrames() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  fake.controller = &controller;
  fake.operations = &operations;

  CHECK(controller.initialize(operations, true));
  CHECK(controller.arm(operations, true));
  CHECK(controller.state() == safety::State::Armed);
  record(fake, Event::Armed);
  checkExactStartSequence(fake);

  led::DisplayState firstColor[led::map::TOTAL_LED_COUNT]{};
  firstColor[0].color = {20, 10, 5};
  CHECK(controller.transmit(operations, true, firstColor,
                            led::map::TOTAL_LED_COUNT));
  CHECK(fake.events[fake.eventCount - 1] == Event::TransmitColor);
  led::DisplayState secondColor[led::map::TOTAL_LED_COUNT]{};
  secondColor[305].color.blue = 30;
  CHECK(controller.transmit(operations, true, secondColor,
                            led::map::TOTAL_LED_COUNT));
  CHECK(fake.events[fake.eventCount - 1] == Event::TransmitColor);
}

void testBlackFailureNeverArms() {
  const bool failureModes[] = {true, false};
  for (bool failPreparation : failureModes) {
    safety::Controller controller;
    FakeTransport fake{};
    safety::Operations operations = operationsFor(fake);
    CHECK(controller.initialize(operations, true));
    fake.failPrepareBlack = failPreparation;
    fake.failBlackTransmit = !failPreparation;
    CHECK(!controller.arm(operations, true));
    CHECK(controller.state() == safety::State::BackendReady);
    CHECK(!controller.transmit(operations, true, nullptr, 0));
    CHECK(!containsEvent(fake, Event::TransmitColor));
    CHECK(fake.events[fake.eventCount - 1] == Event::DataLow);
  }
}

void testColorFailureDisarmsAndRequiresFreshBlack() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  CHECK(controller.initialize(operations, true));
  CHECK(controller.arm(operations, true));
  CHECK(controller.state() == safety::State::Armed);

  led::DisplayState color[led::map::TOTAL_LED_COUNT]{};
  color[0].color.red = 1;
  fake.failColorTransmit = true;
  CHECK(!controller.transmit(operations, true, color,
                             led::map::TOTAL_LED_COUNT));
  CHECK(controller.state() == safety::State::BackendReady);
  CHECK(fake.events[fake.eventCount - 2] == Event::EnableLow);
  CHECK(fake.events[fake.eventCount - 1] == Event::DataLow);

  const uint8_t transmissionsBeforeRetry = fake.eventCount;
  CHECK(!controller.transmit(operations, true, color,
                             led::map::TOTAL_LED_COUNT));
  CHECK(fake.eventCount == transmissionsBeforeRetry);
  fake.failColorTransmit = false;
  CHECK(controller.arm(operations, true));
  CHECK(controller.state() == safety::State::Armed);
  CHECK(fake.events[fake.eventCount - 2] == Event::TransmitBlack);
  CHECK(fake.events[fake.eventCount - 1] == Event::BlackComplete);
}

void testEnableFailureNeverArms() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  CHECK(controller.initialize(operations, true));
  fake.failEnableHigh = true;
  CHECK(!controller.arm(operations, true));
  CHECK(controller.state() == safety::State::BackendReady);
  CHECK(!containsEvent(fake, Event::TransmitBlack));
  CHECK(fake.events[fake.eventCount - 2] == Event::EnableLow);
  CHECK(fake.events[fake.eventCount - 1] == Event::DataLow);
  CHECK(!containsEvent(fake, Event::TransmitColor));

  fake.failEnableHigh = false;
  CHECK(controller.arm(operations, true));
  CHECK(controller.state() == safety::State::Armed);
  CHECK(fake.events[fake.eventCount - 2] == Event::TransmitBlack);
  CHECK(fake.events[fake.eventCount - 1] == Event::BlackComplete);
}

void testDisarmAndRearm() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  CHECK(controller.initialize(operations, true));
  CHECK(controller.arm(operations, true));
  CHECK(controller.state() == safety::State::Armed);

  clearEvents(fake);
  controller.disarm(operations);
  CHECK(controller.state() == safety::State::BackendReady);
  CHECK(fake.events[0] == Event::EnableLow);
  CHECK(fake.events[1] == Event::DataLow);

  led::DisplayState color[led::map::TOTAL_LED_COUNT]{};
  color[2].color.green = 1;
  CHECK(!controller.transmit(operations, true, color,
                             led::map::TOTAL_LED_COUNT));
  CHECK(!containsEvent(fake, Event::TransmitColor));
  CHECK(controller.arm(operations, true));
  CHECK(controller.state() == safety::State::Armed);
  CHECK(containsEvent(fake, Event::TransmitBlack));
}

void testNoDataStatusDoesNotReplaceForcedBlack() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  CHECK(controller.initialize(operations, true));

  led::DisplayState noData[led::map::TOTAL_LED_COUNT]{};
  led::PhysicalAddress status{};
  led::GlobalPhysicalIndex statusGlobal{};
  CHECK(led::map::statusLedAddress(led::StatusLed::MtaFeed, status));
  CHECK(led::map::toGlobalIndex(status, statusGlobal));
  noData[statusGlobal.value].color = {255, 0, 0};
  CHECK(!safety::isBlackFrame(noData, led::map::TOTAL_LED_COUNT));

  CHECK(controller.arm(operations, true));
  CHECK(fake.events[fake.eventCount - 1] == Event::BlackComplete);
  CHECK(controller.state() == safety::State::Armed);
  CHECK(controller.transmit(operations, true, noData,
                            led::map::TOTAL_LED_COUNT));
  CHECK(fake.events[fake.eventCount - 1] == Event::TransmitColor);
}

void testResetRetainsPhysicalStateUntilBlackCompletes() {
  FakeTransport fake{};
  safety::Controller first{};
  safety::Operations operations = operationsFor(fake);
  CHECK(first.initialize(operations, true));
  CHECK(first.arm(operations, true));
  led::DisplayState previousColor[led::map::TOTAL_LED_COUNT]{};
  previousColor[10].color.red = 80;
  CHECK(first.transmit(operations, true, previousColor,
                       led::map::TOTAL_LED_COUNT));
  CHECK(fake.retainedColor);

  safety::Controller afterReset{};
  fake.controller = &afterReset;
  fake.operations = &operations;
  CHECK(afterReset.state() == safety::State::Disabled);
  CHECK(fake.retainedColor);
  CHECK(!afterReset.transmit(operations, true, previousColor,
                             led::map::TOTAL_LED_COUNT));
  CHECK(fake.retainedColor);

  CHECK(afterReset.initialize(operations, true));
  CHECK(fake.retainedColor);
  CHECK(afterReset.arm(operations, true));
  CHECK(!fake.retainedColor);
  CHECK(afterReset.state() == safety::State::Armed);
}

void testCompileTimeDisabledAndFailureStates() {
  safety::Controller controller;
  FakeTransport fake{};
  safety::Operations operations = operationsFor(fake);
  CHECK(!controller.initialize(operations, false));
  CHECK(controller.state() == safety::State::Disabled);
  CHECK(fake.eventCount == 0);
  CHECK(!controller.arm(operations, false));
  led::DisplayState color[led::map::TOTAL_LED_COUNT]{};
  color[0].color.red = 1;
  CHECK(!controller.transmit(operations, false, color,
                             led::map::TOTAL_LED_COUNT));
  CHECK(fake.eventCount == 0);

  fake.failInitialize = true;
  CHECK(!controller.initialize(operations, true));
  CHECK(controller.state() == safety::State::Disabled);
  CHECK(!containsEvent(fake, Event::EnableHigh));
  CHECK(!containsEvent(fake, Event::TransmitBlack));
}

void testInvalidFrameAndNoHeapState() {
  safety::Controller controller;
  CHECK(sizeof(controller) == sizeof(safety::State));
  CHECK(!safety::isBlackFrame(nullptr, led::map::TOTAL_LED_COUNT));
  CHECK(!safety::isBlackFrame(
      static_cast<const led::DisplayState *>(nullptr),
      led::map::TOTAL_LED_COUNT - 1));
  led::DisplayState invalid[led::map::TOTAL_LED_COUNT]{};
  CHECK(!safety::isBlackFrame(invalid, led::map::TOTAL_LED_COUNT - 1));
}

}  // namespace

int main() {
  testColdStartAndPreArmRejections();
  testBlackFirstOrderingAndNormalFrames();
  testBlackFailureNeverArms();
  testColorFailureDisarmsAndRequiresFreshBlack();
  testEnableFailureNeverArms();
  testDisarmAndRearm();
  testNoDataStatusDoesNotReplaceForcedBlack();
  testResetRetainsPhysicalStateUntilBlackCompletes();
  testCompileTimeDisabledAndFailureStates();
  testInvalidFrameAndNoHeapState();
  printf("Physical output safety tests: %d checks, %d failures\n", checks,
         failures);
  return failures == 0 ? 0 : 1;
}
