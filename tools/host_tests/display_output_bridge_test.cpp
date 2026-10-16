// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "display_output.h"

#include <Arduino.h>

#include "led_backend.h"
#include "led_engine.h"
#include "led_map.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace display = brooklyn::display;
namespace led = brooklyn::led;
namespace semantic = brooklyn::semantic;

HostSerial Serial;

namespace {

int checks = 0;
int failures = 0;
int backendCalls = 0;

#define CHECK(condition)                                                  \
  do {                                                                     \
    ++checks;                                                              \
    if (!(condition)) {                                                    \
      ++failures;                                                          \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
    }                                                                      \
  } while (0)

bool equalRgb(led::Rgb actual, semantic::Rgb expected) {
  return actual.red == expected.red && actual.green == expected.green &&
         actual.blue == expected.blue;
}

semantic::Rgb pattern(uint16_t index, uint8_t salt) {
  return {static_cast<uint8_t>((index * 17U + salt) & 0xffU),
          static_cast<uint8_t>((index * 29U + salt * 3U) & 0xffU),
          static_cast<uint8_t>((index * 43U + salt * 7U) & 0xffU)};
}

void fillPattern(semantic::RgbFrame &frame, uint8_t salt) {
  for (uint16_t index = 0; index < led::map::TOTAL_LED_COUNT; ++index) {
    frame.pixels[index] = pattern(index, salt);
  }
}

void snapshot(semantic::RgbFrame &frame) {
  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    led::Rgb color{};
    CHECK(led::getGlobalLed({global}, color));
    frame.pixels[global] = {color.red, color.green, color.blue};
  }
}

void checkFrame(const semantic::RgbFrame &expected) {
  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    led::Rgb color{};
    CHECK(led::getGlobalLed({global}, color));
    CHECK(equalRgb(color, expected.pixels[global]));
  }
}

void testCanonicalMapAccounting() {
  CHECK(led::map::TOTAL_LED_COUNT == 306);
  uint16_t mapCount = 0;
  uint16_t statusCount = 0;
  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    const led::GlobalPhysicalIndex index{global};
    led::PhysicalAddress address{};
    led::GlobalPhysicalIndex roundTrip{};
    led::map::PhysicalLed physical{};
    CHECK(led::map::toPhysicalAddress(index, address));
    CHECK(led::map::toGlobalIndex(address, roundTrip));
    CHECK(roundTrip.value == global);
    CHECK(led::map::physicalLed(index, physical));
    CHECK(physical.globalIndex.value == global);
    CHECK(physical.address.channel == address.channel);
    CHECK(physical.address.index == address.index);
    if (physical.role == led::LedRole::Map) {
      ++mapCount;
    } else {
      ++statusCount;
    }
  }
  CHECK(mapCount == 302);
  CHECK(statusCount == 4);
  for (uint8_t status = 0; status < 4; ++status) {
    led::PhysicalAddress address{};
    led::GlobalPhysicalIndex index{};
    led::map::PhysicalLed physical{};
    CHECK(led::map::statusLedAddress(static_cast<led::StatusLed>(status),
                                     address));
    CHECK(led::map::toGlobalIndex(address, index));
    CHECK(led::map::physicalLed(index, physical));
    CHECK(physical.role == led::LedRole::Status);
  }
}

void testIsolatedPixelTransfersAndNeighbors() {
  for (uint16_t target = 0; target < led::map::TOTAL_LED_COUNT; ++target) {
    semantic::RgbFrame frame{};
    frame.pixels[target] = pattern(target, 79);
    CHECK(display::output::publish(&frame));

    for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
      led::PhysicalAddress address{};
      led::GlobalPhysicalIndex roundTrip{};
      led::Rgb actual{};
      CHECK(led::map::toPhysicalAddress({global}, address));
      CHECK(led::map::toGlobalIndex(address, roundTrip));
      CHECK(roundTrip.value == global);
      CHECK(led::getLed(address, actual));
      const semantic::Rgb expected =
          global == target ? frame.pixels[target] : semantic::Rgb{0, 0, 0};
      CHECK(equalRgb(actual, expected));
    }
  }
}

void testDeterministicFullFrameAndChannelBoundaries() {
  semantic::RgbFrame frame{};
  fillPattern(frame, 11);
  const semantic::RgbFrame original = frame;
  CHECK(display::output::publish(&frame));
  checkFrame(original);
  CHECK(memcmp(&frame, &original, sizeof(frame)) == 0);
  CHECK(display::output::publish(&frame));
  checkFrame(original);

  uint16_t channelStart = 0;
  for (uint8_t channel = 1; channel <= led::map::CHANNEL_COUNT; ++channel) {
    const auto physicalChannel = static_cast<led::PhysicalChannel>(channel);
    const uint8_t count = led::map::channelLedCount(physicalChannel);
    CHECK(count != 0);
    const uint16_t boundaries[] = {
        channelStart, static_cast<uint16_t>(channelStart + count - 1)};
    for (uint16_t global : boundaries) {
      led::PhysicalAddress address{};
      led::GlobalPhysicalIndex roundTrip{};
      led::Rgb actual{};
      CHECK(led::map::toPhysicalAddress({global}, address));
      CHECK(address.channel == physicalChannel);
      CHECK(led::map::toGlobalIndex(address, roundTrip));
      CHECK(roundTrip.value == global);
      CHECK(led::getLed(address, actual));
      CHECK(equalRgb(actual, original.pixels[global]));
    }
    channelStart = static_cast<uint16_t>(channelStart + count);
  }
  CHECK(channelStart == led::map::TOTAL_LED_COUNT);
}

void testRepresentativeFramesAndPixelPreservation() {
  semantic::RgbFrame frames[4]{};
  fillPattern(frames[0], 1);  // Fresh.
  fillPattern(frames[1], 2);  // Degraded.
  fillPattern(frames[2], 3);  // NoData.
  fillPattern(frames[3], 4);  // Provisioning.
  for (uint16_t global = 0; global < led::map::TOTAL_LED_COUNT; ++global) {
    led::map::PhysicalLed physical{};
    CHECK(led::map::physicalLed({global}, physical));
    if (physical.role == led::LedRole::Status) {
      frames[0].pixels[global] = {0, 255, 0};
      frames[1].pixels[global] = {255, 128, 0};
      frames[2].pixels[global] = {255, 0, 0};
      frames[3].pixels[global] = {0, 255, 255};
    }
  }

  for (const semantic::RgbFrame &frame : frames) {
    const semantic::RgbFrame original = frame;
    CHECK(display::output::publish(&frame));
    checkFrame(original);
  }
}

void testInvalidInputDoesNotPublish() {
  semantic::RgbFrame before{};
  snapshot(before);
  CHECK(!display::output::publish(nullptr));
  checkFrame(before);

  led::DisplayState invalid[led::map::TOTAL_LED_COUNT]{};
  CHECK(!led::publishFrame(nullptr, led::map::TOTAL_LED_COUNT));
  CHECK(!led::publishFrame(invalid, led::map::TOTAL_LED_COUNT - 1));
  checkFrame(before);
}

}  // namespace

namespace brooklyn::led::backend {

bool initializePhysicalLedBackend() {
  ++backendCalls;
  return false;
}
bool armLedOutputs() {
  ++backendCalls;
  return false;
}
void disarmLedOutputs() { ++backendCalls; }
bool physicalLedOutputsArmed() {
  ++backendCalls;
  return false;
}
bool physicalLedBackendInitialized() {
  ++backendCalls;
  return false;
}
bool physicalLedOutputPermissionEnabled() {
  ++backendCalls;
  return false;
}
bool transmitFrame(const DisplayState *, uint16_t) {
  ++backendCalls;
  return false;
}

}  // namespace brooklyn::led::backend

int main() {
  led::begin();
  testCanonicalMapAccounting();
  testIsolatedPixelTransfersAndNeighbors();
  testDeterministicFullFrameAndChannelBoundaries();
  testRepresentativeFramesAndPixelPreservation();
  testInvalidInputDoesNotPublish();
  CHECK(backendCalls == 0);
  printf("Output bridge tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}