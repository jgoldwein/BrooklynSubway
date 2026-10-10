// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_engine.h"

#include "led_backend.h"
#include "board_pins.h"
#include "led_map.h"
#include "led_power.h"

#include <Arduino.h>

namespace brooklyn::led {
namespace {

DisplayState framebuffer[map::TOTAL_LED_COUNT]{};
uint8_t brightness = 255;

constexpr Rgb BLACK = {0, 0, 0};
constexpr Rgb RED = {255, 0, 0};
constexpr Rgb GREEN = {0, 255, 0};
constexpr Rgb BLUE = {0, 0, 255};
constexpr Rgb WHITE = {255, 255, 255};

bool isLit(Rgb color) {
  return color.red != 0 || color.green != 0 || color.blue != 0;
}

const char *statusName(StatusLed status) {
  switch (status) {
    case StatusLed::System:
      return "SYSTEM";
    case StatusLed::Network:
      return "NETWORK";
    case StatusLed::MtaFeed:
      return "MTA FEED";
    case StatusLed::Mode:
      return "MODE";
  }
  return "UNKNOWN";
}

const char *statusColor(StatusLed status) {
  PhysicalAddress address{};
  Rgb color{};
  if (!map::statusLedAddress(status, address) || !getLed(address, color)) {
    return "invalid";
  }
  if (color.red == 0 && color.green == 0 && color.blue == 0) {
    return "off";
  }
  if (color.red == 0 && color.green == 255 && color.blue == 0) {
    return "green";
  }
  if (color.red == 0 && color.green == 0 && color.blue == 255) {
    return "blue";
  }
  if (color.red == 255 && color.green == 0 && color.blue == 0) {
    return "red";
  }
  if (color.red == 255 && color.green == 255 && color.blue == 255) {
    return "white";
  }
  return "custom";
}

bool failSelfTest(const char *reason) {
  Serial.printf("LED engine self-test: FAIL - %s\n", reason);
  clear();
  brightness = 255;
  return false;
}

}  // namespace

void begin() {
  clear();
  brightness = 255;
}

void clear() {
  for (DisplayState &state : framebuffer) {
    state.color = BLACK;
  }
}

bool setLed(PhysicalAddress address, Rgb color) {
  GlobalPhysicalIndex globalIndex{};
  if (!map::toGlobalIndex(address, globalIndex)) {
    return false;
  }
  framebuffer[globalIndex.value].color = color;
  return true;
}

bool setGlobalLed(GlobalPhysicalIndex index, Rgb color) {
  if (index.value >= map::TOTAL_LED_COUNT) {
    return false;
  }
  framebuffer[index.value].color = color;
  return true;
}

bool getLed(PhysicalAddress address, Rgb &color) {
  GlobalPhysicalIndex globalIndex{};
  if (!map::toGlobalIndex(address, globalIndex)) {
    return false;
  }
  color = framebuffer[globalIndex.value].color;
  return true;
}

bool getGlobalLed(GlobalPhysicalIndex index, Rgb &color) {
  if (index.value >= map::TOTAL_LED_COUNT) {
    return false;
  }
  color = framebuffer[index.value].color;
  return true;
}

bool setStatusLed(StatusLed status, Rgb color) {
  PhysicalAddress address{};
  return map::statusLedAddress(status, address) && setLed(address, color);
}

bool fillChannel(PhysicalChannel channel, Rgb color) {
  const uint8_t count = map::channelLedCount(channel);
  if (count == 0) {
    return false;
  }
  for (uint8_t index = 0; index < count; ++index) {
    if (!setLed({channel, index}, color)) {
      return false;
    }
  }
  return true;
}

void setGlobalBrightness(uint16_t value) {
  brightness = value > 255 ? 255 : static_cast<uint8_t>(value);
}

uint8_t globalBrightness() {
  return brightness;
}

void render() {
#if defined(BROOKLYN_TARGET_XIAO)
  uint16_t litCount = 0;
  uint8_t activeChannels = 0;
  DisplayState limitedFrame[map::TOTAL_LED_COUNT]{};
  power::FramePowerReport powerReport{};
  power::limitFrameToBudget(framebuffer, map::TOTAL_LED_COUNT, brightness, 255,
                            limitedFrame, powerReport);
  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    const PhysicalChannel physicalChannel =
        static_cast<PhysicalChannel>(channel + 1);
    bool channelLit = false;
    const uint8_t count = map::channelLedCount(physicalChannel);
    for (uint8_t index = 0; index < count; ++index) {
      Rgb color{};
      getLed({physicalChannel, index}, color);
      if (isLit(color)) {
        ++litCount;
        channelLit = true;
      }
    }
    if (channelLit) {
      activeChannels |= static_cast<uint8_t>(1U << channel);
    }
  }

  Serial.printf("LED simulation: %u lit, channels", litCount);
  if (activeChannels == 0) {
    Serial.print(" none");
  } else {
    for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
      if ((activeChannels & static_cast<uint8_t>(1U << channel)) != 0) {
        Serial.printf(" CH%u", channel + 1);
      }
    }
  }
  Serial.printf(", brightness %u/255; estimated LED current %lu mA, "
                "power scale %u/255; status:",
                brightness,
                static_cast<unsigned long>(powerReport.estimatedCurrentMa),
                powerReport.appliedScale);
  constexpr StatusLed statuses[] = {StatusLed::System, StatusLed::Network,
                                    StatusLed::MtaFeed, StatusLed::Mode};
  for (StatusLed status : statuses) {
    Serial.printf(" %s=%s", statusName(status), statusColor(status));
  }
  Serial.println();
#elif defined(BROOKLYN_TARGET_PCB)
  DisplayState limitedFrame[map::TOTAL_LED_COUNT]{};
  power::FramePowerReport powerReport{};
  power::limitFrameToBudget(framebuffer, map::TOTAL_LED_COUNT, brightness,
                            power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP,
                            limitedFrame, powerReport);
  const bool transmitted =
      backend::transmitFrame(limitedFrame, map::TOTAL_LED_COUNT);
  Serial.printf("Physical LED frame: %s; estimated current %lu mA, limited "
                "current %lu mA, power scale %u/255, brightness cap %u/255\n",
                transmitted ? "transmitted" : "suppressed (DISARMED)",
                static_cast<unsigned long>(powerReport.estimatedCurrentMa),
                static_cast<unsigned long>(powerReport.limitedCurrentMa),
                powerReport.appliedScale,
                power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP);
#endif
}

bool runSelfTest() {
  constexpr uint8_t expectedCounts[map::CHANNEL_COUNT] = {37, 38, 37, 37,
                                                           42, 37, 38, 40};
  if (map::CHANNEL_COUNT != 8) {
    return failSelfTest("expected exactly 8 channels");
  }
  uint16_t total = 0;
  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    const uint8_t count =
        map::channelLedCount(static_cast<PhysicalChannel>(channel + 1));
    if (count != expectedCounts[channel]) {
      return failSelfTest("channel count mismatch");
    }
    total += count;
  }
  if (total != map::TOTAL_LED_COUNT || total != 306) {
    return failSelfTest("total LED count is not 306");
  }

  GlobalPhysicalIndex mappedIndex{};
  if (map::toGlobalIndex({PhysicalChannel::CH1, 0}, mappedIndex) &&
      mappedIndex.value == 0) {
  } else {
    return failSelfTest("first physical address mapping failed");
  }
  if (map::toGlobalIndex({PhysicalChannel::CH1, 37}, mappedIndex)) {
    return failSelfTest("channel index bounds check failed");
  }
  if (map::toGlobalIndex({static_cast<PhysicalChannel>(0), 0}, mappedIndex)) {
    return failSelfTest("invalid channel bounds check failed");
  }
  PhysicalAddress invalidAddress{};
  if (map::toPhysicalAddress({map::TOTAL_LED_COUNT}, invalidAddress)) {
    return failSelfTest("global index bounds check failed");
  }

  constexpr StatusLed statuses[] = {StatusLed::System, StatusLed::Network,
                                    StatusLed::MtaFeed, StatusLed::Mode};
  constexpr uint16_t expectedDesignators[] = {252, 251, 306, 307};
  for (uint8_t index = 0; index < 4; ++index) {
    PhysicalAddress address{};
    BoardLedDesignator designator{};
    map::PhysicalLed led{};
    if (!map::statusLedAddress(statuses[index], address) ||
        address.channel != PhysicalChannel::CH8 || address.index != index ||
        !map::statusLedDesignator(statuses[index], designator) ||
        designator.value != expectedDesignators[index] ||
        !map::physicalLed({static_cast<uint16_t>(266 + index)}, led) ||
        led.role != LedRole::Status ||
        led.designator.value != expectedDesignators[index]) {
      return failSelfTest("channel 8 status LED mapping mismatch");
    }
  }

  begin();
  if (!setLed({PhysicalChannel::CH3, 12}, RED)) {
    return failSelfTest("individual LED set failed");
  }
  Rgb color{};
  if (!getLed({PhysicalChannel::CH3, 12}, color) || color.red != RED.red ||
      color.green != RED.green || color.blue != RED.blue) {
    return failSelfTest("individual LED get failed");
  }
  if (!setGlobalLed({75}, BLUE) || !getGlobalLed({75}, color) ||
      color.blue != BLUE.blue) {
    return failSelfTest("global LED set/get failed");
  }
  if (setLed({PhysicalChannel::CH8, 40}, WHITE) ||
      getLed({PhysicalChannel::CH8, 40}, color)) {
    return failSelfTest("framebuffer bounds check failed");
  }

  setGlobalBrightness(300);
  if (globalBrightness() != 255) {
    return failSelfTest("brightness upper limit failed");
  }
  setGlobalBrightness(0);
  if (globalBrightness() != 0) {
    return failSelfTest("brightness lower limit failed");
  }
  setGlobalBrightness(128);
  clear();
  for (uint16_t index = 0; index < map::TOTAL_LED_COUNT; ++index) {
    if (!getGlobalLed({index}, color) || isLit(color)) {
      return failSelfTest("framebuffer clear failed");
    }
  }
  if (!fillChannel(PhysicalChannel::CH2, GREEN) ||
      fillChannel(static_cast<PhysicalChannel>(0), GREEN)) {
    return failSelfTest("channel fill or channel bounds check failed");
  }

  DisplayState fullWhite[map::TOTAL_LED_COUNT]{};
  DisplayState limitedFrame[map::TOTAL_LED_COUNT]{};
  DisplayState zeroFrame[map::TOTAL_LED_COUNT]{};
  for (DisplayState &state : fullWhite) {
    state.color = WHITE;
  }
  if (power::estimateFrameCurrentMa(zeroFrame, map::TOTAL_LED_COUNT, 255) !=
      0) {
    return failSelfTest("zero-frame current estimate failed");
  }
  const uint32_t fullWhiteEstimate =
      power::estimateFrameCurrentMa(fullWhite, map::TOTAL_LED_COUNT, 255);
  if (fullWhiteEstimate <= power::FIRMWARE_LED_CURRENT_BUDGET_MA) {
    return failSelfTest("full-white frame should exceed LED power budget");
  }
  const uint8_t budgetScale = power::brightnessScaleForBudget(
      fullWhite, map::TOTAL_LED_COUNT, 255);
  if (budgetScale >= 255) {
    return failSelfTest("power scaling did not reduce brightness");
  }
  power::FramePowerReport powerReport{};
  power::limitFrameToBudget(fullWhite, map::TOTAL_LED_COUNT, 255, 255,
                            limitedFrame, powerReport);
  if (powerReport.limitedCurrentMa >
      power::FIRMWARE_LED_CURRENT_BUDGET_MA) {
    return failSelfTest("power-limited frame exceeds configured budget");
  }
  if (backend::physicalLedOutputPermissionEnabled() ||
      backend::physicalLedOutputsArmed()) {
    return failSelfTest("physical LED outputs must remain disarmed");
  }
  for (uint16_t index = 0; index < map::TOTAL_LED_COUNT; ++index) {
    if (limitedFrame[index].color.red > fullWhite[index].color.red ||
        limitedFrame[index].color.green > fullWhite[index].color.green ||
        limitedFrame[index].color.blue > fullWhite[index].color.blue) {
      return failSelfTest("power scaling increased a color component");
    }
  }
  clear();
  setGlobalBrightness(255);

  Serial.println("LED engine self-test: PASS");
  return true;
}

void runXiaoDemonstration() {
#if defined(BROOKLYN_TARGET_XIAO)
  clear();
  setStatusLed(StatusLed::System, GREEN);
  setStatusLed(StatusLed::Network, BLUE);
  setStatusLed(StatusLed::MtaFeed, RED);
  setStatusLed(StatusLed::Mode, WHITE);
  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    const uint8_t firstMapIndex =
        channel == 7 ? 4 : 0;
    setLed({static_cast<PhysicalChannel>(channel + 1), firstMapIndex}, WHITE);
  }
  Serial.println("LED software demonstration:");
  render();
  delay(1500);
  clear();
  render();
#endif
}

}  // namespace brooklyn::led
