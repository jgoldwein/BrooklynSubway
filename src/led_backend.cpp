// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_backend.h"
#include "hardware.h"
#include "controls.h"

#include "board_pins.h"
#include "led_map.h"
#include "semantic_api.h"
#include "led_power.h"
#include "physical_output_safety.h"
#include "live_display.h"

#include <Arduino.h>

#if defined(BROOKLYN_TARGET_PCB)
#include <Adafruit_NeoPixel.h>

#if !defined(BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT)
#error "BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT must be explicitly defined"
#endif
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT != 0 && \
    BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT != 1
#error "BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT must be 0 or 1"
#endif

namespace {

using brooklyn::led::DisplayState;
using brooklyn::led::Rgb;
namespace safety = brooklyn::led::safety;

Adafruit_NeoPixel *productionStrips() {
  static Adafruit_NeoPixel strips[8] = {
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[0],
                        brooklyn::pins::LED_DATA[0],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[1],
                        brooklyn::pins::LED_DATA[1],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[2],
                        brooklyn::pins::LED_DATA[2],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[3],
                        brooklyn::pins::LED_DATA[3],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[4],
                        brooklyn::pins::LED_DATA[4],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[5],
                        brooklyn::pins::LED_DATA[5],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[6],
                        brooklyn::pins::LED_DATA[6],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[7],
                        brooklyn::pins::LED_DATA[7],
                        NEO_GRB + NEO_KHZ800),
  };
  return strips;
}

bool initialized = false;
safety::Controller outputController;

void forceDataPinsLow() {
  for (uint8_t pin : brooklyn::pins::LED_DATA) {
    digitalWrite(pin, LOW);
  }
}

bool setEnable(void *, bool enabled) {
  if (enabled) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
    digitalWrite(brooklyn::pins::LED_ENABLE, HIGH);
    return true;
#else
    return false;
#endif
  }
  digitalWrite(brooklyn::pins::LED_ENABLE, LOW);
  return true;
}

bool initializeStrips(void *) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  initialized = false;
  auto *strips = productionStrips();
  for (uint8_t channel = 0; channel < brooklyn::led::map::CHANNEL_COUNT;
       ++channel) {
    if (strips[channel].getPixels() == nullptr) {
      Serial.printf("LED backend initialization failed: CH%u buffer allocation\n",
                    channel + 1);
      forceDataPinsLow();
      initialized = false;
      return false;
    }
  }
  for (uint8_t channel = 0; channel < brooklyn::led::map::CHANNEL_COUNT;
       ++channel) {
    strips[channel].begin();
    digitalWrite(brooklyn::pins::LED_DATA[channel], LOW);
  }
  forceDataPinsLow();
  initialized = true;
  return true;
#else
  return false;
#endif
}

bool setDataPinsLow(void *) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  forceDataPinsLow();
  return true;
#else
  return false;
#endif
}

bool prepareBlack(void *, const DisplayState *black, DisplayState *limited,
                  uint16_t count) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  if (black == nullptr || limited == nullptr ||
      count != brooklyn::led::map::TOTAL_LED_COUNT) {
    return false;
  }
  brooklyn::led::power::FramePowerReport report{};
  brooklyn::led::power::limitFrameToBudget(
      black, count, 255,
      brooklyn::led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP, limited,
      report);
  return safety::isBlackFrame(limited, count);
#else
  (void)black;
  (void)limited;
  (void)count;
  return false;
#endif
}

bool transmitOnHardware(void *, const DisplayState *frame, uint16_t count) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  if (!initialized || frame == nullptr ||
      count != brooklyn::led::map::TOTAL_LED_COUNT) {
    return false;
  }
  auto *strips = productionStrips();
  uint16_t start = 0;
  for (uint8_t channel = 0; channel < brooklyn::led::map::CHANNEL_COUNT;
       ++channel) {
    for (uint8_t index = 0;
         index < brooklyn::pins::LED_COUNT[channel]; ++index) {
      const Rgb color = frame[start + index].color;
      strips[channel].setPixelColor(index, color.red, color.green, color.blue);
    }
    strips[channel].show();
    digitalWrite(brooklyn::pins::LED_DATA[channel], LOW);
    start += brooklyn::pins::LED_COUNT[channel];
  }
  forceDataPinsLow();
  return true;
#else
  (void)frame;
  (void)count;
  return false;
#endif
}

const safety::Operations kOutputOperations = {
    nullptr, setEnable, initializeStrips, setDataPinsLow, prepareBlack,
    transmitOnHardware};

}  // namespace
#endif

namespace brooklyn::led::backend {
namespace {
bool diagnosticSystemReady = false;
}

void setDiagnosticSystemReady(bool ready) {
  diagnosticSystemReady = ready;
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  if (!ready) display::live::disable();
#endif
  if (!ready) disarmLedOutputs();
}

namespace {
bool runDiagnostic(PhysicalChannel channel, bool walkChannel) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  display::live::disable();
#endif
  const uint32_t durationMs = walkChannel ? 500 : 2000;
  const uint8_t channelNumber = static_cast<uint8_t>(channel);
  constexpr uint8_t kRed = 4;
  DisplayState frame[map::TOTAL_LED_COUNT]{};
  GlobalPhysicalIndex index{};
  map::PhysicalLed physical{};
  disarmLedOutputs();
  if (!diagnosticSystemReady) {
    Serial.println("LED diagnostic refused: startup checks have not passed");
    return false;
  }
  if (channelNumber < 1 || channelNumber > map::CHANNEL_COUNT) {
    Serial.println("LED diagnostic refused: invalid channel");
    return false;
  }
  const uint8_t channelIndex = channelNumber - 1;
  const uint8_t count = map::channelLedCount(channel);
  constexpr uint16_t kFirstDesignators[] = {42, 208, 226, 220, 190, 194, 198, 252};
  if (count != brooklyn::pins::LED_COUNT[channelIndex]) {
    Serial.println("LED diagnostic refused: channel count mismatch");
    return false;
  }
  const uint8_t pixelCount = walkChannel ? count : 1;
  // Validate the full requested physical range before enabling any outputs.
  for (uint8_t pixel = 0; pixel < pixelCount; ++pixel) {
    const bool isStatus = channel == PhysicalChannel::CH8 && pixel < 4;
    if (!map::toGlobalIndex({channel, pixel}, index) ||
        !map::physicalLed(index, physical) ||
        physical.address.channel != channel || physical.address.index != pixel ||
        physical.role != (isStatus ? LedRole::Status : LedRole::Map) ||
        (pixel == 0 && physical.designator.value != kFirstDesignators[channelIndex])) {
      Serial.println("LED diagnostic refused: physical mapping mismatch");
      return false;
    }
    if (isStatus) {
      PhysicalAddress expected{};
      BoardLedDesignator designator{};
      const StatusLed status = static_cast<StatusLed>(pixel);
      if (!map::statusLedAddress(status, expected) ||
          !map::statusLedDesignator(status, designator) ||
          expected.channel != channel || expected.index != pixel ||
          designator.value != physical.designator.value) {
        Serial.println("LED diagnostic refused: status mapping mismatch");
        return false;
      }
    }
  }
  Serial.printf("LED diagnostic: CH%u GPIO%u, %u pixel(s), red=4/255, %lu ms per pixel; GPIO1 enable; POWER aborts walk\n",
                channelNumber, brooklyn::pins::LED_DATA[channelIndex], pixelCount,
                static_cast<unsigned long>(durationMs));
  if (!outputController.initialize(kOutputOperations, true) ||
      !outputController.arm(kOutputOperations, true)) {
    disarmLedOutputs();
    Serial.println("LED diagnostic FAIL: black-first initialization/arming");
    return false;
  }
  bool lit = true;
  for (uint8_t pixel = 0; pixel < pixelCount; ++pixel) {
    if (walkChannel && (digitalRead(brooklyn::pins::BUTTON_POWER) == LOW ||
                        controls::pollWifiRecoveryDuringDiagnostic())) {
      Serial.println("LED diagnostic: aborted by POWER/recovery");
      lit = false;
      break;
    }
    if (!map::toGlobalIndex({channel, pixel}, index) ||
        !map::physicalLed(index, physical)) {
      lit = false;
      break;
    }
    Serial.printf("LED diagnostic: CH%u index %u LED%u\n", channelNumber, pixel,
                  physical.designator.value);
    if (channel == PhysicalChannel::CH8 && pixel < 4) {
      constexpr const char *kLabels[] = {"SYSTEM", "NETWORK", "MTA FEED", "MODE"};
      Serial.printf("LED diagnostic: status %s\n", kLabels[pixel]);
    }
    frame[index.value].color = {kRed, 0, 0};
    lit = outputController.transmit(kOutputOperations, true, frame,
                                     map::TOTAL_LED_COUNT);
    if (lit) delay(durationMs);
    // Previous pixel is black in the next transmitted frame.
    frame[index.value].color = {0, 0, 0};
    if (!lit) break;
  }
  const bool cleared = outputController.transmit(kOutputOperations, true, frame,
                                                map::TOTAL_LED_COUNT);
  disarmLedOutputs();
  Serial.printf("LED diagnostic %s; black sent=%s; GPIO1 LOW; visual confirmation required\n",
                lit && cleared ? "sequence complete" : "FAIL",
                cleared ? "yes" : "no");
  return lit && cleared;
#else
  (void)channel;
  (void)walkChannel;
  Serial.println("LED diagnostic unavailable on this target");
  return false;
#endif
}

}  // namespace

bool runFirstLedDiagnostic() { return runDiagnostic(PhysicalChannel::CH1, false); }
bool runChannelDiagnostic(PhysicalChannel channel) { return runDiagnostic(channel, true); }
bool runChannelOneDiagnostic() { return runChannelDiagnostic(PhysicalChannel::CH1); }

bool runAllChannelDiagnostics() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  display::live::disable();
#endif
  disarmLedOutputs();
  if (!diagnosticSystemReady) return false;
  Serial.println("LED diagnostic: all-channel sequence starting; POWER aborts entire sequence");
  for (uint8_t number = 1; number <= map::CHANNEL_COUNT; ++number) {
    const auto channel = static_cast<PhysicalChannel>(number);
    if (!hardware::signalDiagnosticChannel(channel) || !runChannelDiagnostic(channel)) {
      disarmLedOutputs();
      Serial.printf("LED diagnostic: all-channel sequence stopped at CH%u; GPIO1 LOW\n", number);
      return false;
    }
  }
  disarmLedOutputs();
  Serial.println("LED diagnostic: all eight channels complete; GPIO1 LOW; visual confirmation required");
  return true;
#else
  return false;
#endif
}

bool runAllRouteDiagnostics() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  display::live::disable();
#endif
  disarmLedOutputs();
  if (!diagnosticSystemReady || !semantic::selfCheck()) return false;
  DisplayState frame[map::TOTAL_LED_COUNT]{};
  constexpr Rgb colors[] = {{4, 0, 0}, {0, 4, 0}, {0, 0, 4}};
  bool ok = true;
  for (uint8_t number = 1; number <= semantic::kRouteCount && ok; ++number) {
    const auto route = static_cast<semantic::Route>(number);
    Serial.printf("Route diagnostic: line %s; RGB 4/255; POWER aborts\n",
                  semantic::routeName(route));
    if (!outputController.initialize(kOutputOperations, true) ||
        !outputController.arm(kOutputOperations, true)) {
      disarmLedOutputs();
      return false;
    }
    for (uint16_t index = 0; index < map::TOTAL_LED_COUNT && ok; ++index) {
      const auto *mapped = semantic::semanticForLed(index);
      if (!mapped || mapped->route != route) continue;
      Serial.printf("Route diagnostic: line %s LED%u CH%u index %u\n",
                    semantic::routeName(route), mapped->designator,
                    mapped->channel, mapped->channelIndex);
      for (const auto &color : colors) {
        if (digitalRead(pins::BUTTON_POWER) == LOW ||
            controls::pollWifiRecoveryDuringDiagnostic()) { ok = false; break; }
        frame[index].color = color;
        ok = outputController.transmit(kOutputOperations, true, frame, map::TOTAL_LED_COUNT);
        if (ok) delay(250);
        frame[index].color = {0, 0, 0};
        if (!ok) break;
      }
      // Clear the previous physical LED before advancing across channels.
      if (ok) ok = outputController.transmit(kOutputOperations, true, frame,
                                             map::TOTAL_LED_COUNT);
    }
    const bool cleared = outputController.transmit(kOutputOperations, true, frame, map::TOTAL_LED_COUNT);
    disarmLedOutputs();
    ok = ok && cleared;
  }
  Serial.printf("Route diagnostic %s; GPIO1 LOW; visual confirmation required\n",
                ok ? "complete" : "stopped/failed");
  return ok;
#else
  return false;
#endif
}

bool armLiveOutput() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  return diagnosticSystemReady && semantic::selfCheck() && outputController.initialize(kOutputOperations, true) &&
      outputController.arm(kOutputOperations, true);
#else
  return false;
#endif
}

bool transmitLiveOutput(const DisplayState *frame, uint16_t count) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  if (!diagnosticSystemReady || !frame || count != map::TOTAL_LED_COUNT) return false;
  // Reject any caller that bypasses the physical cap or total-current budget.
  for (uint16_t i = 0; i < count; ++i) {
    const auto color = frame[i].color;
    if (color.red > power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP ||
        color.green > power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP ||
        color.blue > power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP) return false;
  }
  if (power::estimateFrameCurrentMa(frame, count, 255) > power::FIRMWARE_LED_CURRENT_BUDGET_MA) return false;
  return outputController.transmit(kOutputOperations, true, frame, count);
#else
  (void)frame; (void)count;
  return false;
#endif
}

void stopLiveOutput() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  DisplayState black[map::TOTAL_LED_COUNT]{};
  if (physicalLedOutputsArmed())
    outputController.transmit(kOutputOperations, true, black, map::TOTAL_LED_COUNT);
  disarmLedOutputs();
#endif
}

bool initializePhysicalLedBackend() {
#if defined(BROOKLYN_TARGET_PCB)
  const bool ready = outputController.initialize(
      kOutputOperations, physicalLedOutputPermissionEnabled());
  if (ready) {
    Serial.println(
        "Physical LED backend initialized; post-enable black required.");
  }
  return ready;
#else
  return false;
#endif
}

bool armLedOutputs() {
#if defined(BROOKLYN_TARGET_PCB)
  const bool armed = outputController.arm(
      kOutputOperations, physicalLedOutputPermissionEnabled());
  if (!armed) {
    Serial.println("LED output arm refused; black-first interlock not satisfied.");
  } else {
    Serial.println("Physical LED outputs ARMED after post-enable black frame.");
  }
  return armed;
#else
  Serial.println("LED output arm refused: XIAO is software-simulation only.");
  return false;
#endif
}

void disarmLedOutputs() {
#if defined(BROOKLYN_TARGET_PCB)
  outputController.disarm(kOutputOperations);
#endif
}

bool physicalLedOutputsArmed() {
#if defined(BROOKLYN_TARGET_PCB)
  return outputController.state() == safety::State::Armed;
#else
  return false;
#endif
}

bool physicalLedBackendInitialized() {
#if defined(BROOKLYN_TARGET_PCB)
  return initialized;
#else
  return false;
#endif
}

bool physicalLedOutputPermissionEnabled() {
#if defined(BROOKLYN_TARGET_PCB)
  return BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1;
#else
  return false;
#endif
}

bool transmitFrame(const DisplayState *frame, uint16_t count) {
#if defined(BROOKLYN_TARGET_PCB)
  const bool transmitted = outputController.transmit(
      kOutputOperations, physicalLedOutputPermissionEnabled(), frame, count);
  if (!transmitted && outputController.state() == safety::State::Fault) {
    Serial.println("LED transmission fault; outputs disabled.");
  }
  return transmitted;
#else
  (void)frame;
  (void)count;
  return false;
#endif
}

}  // namespace brooklyn::led::backend
