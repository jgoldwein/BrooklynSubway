// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_backend.h"

#include "board_pins.h"
#include "led_map.h"
#include "led_power.h"
#include "physical_output_safety.h"

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
  if (!ready) disarmLedOutputs();
}

namespace {
bool runDiagnostic(bool walkChannelOne) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  const uint32_t durationMs = walkChannelOne ? 500 : 2000;
  const uint8_t pixelCount = walkChannelOne ? 37 : 1;
  constexpr uint8_t kRed = 4;
  DisplayState frame[map::TOTAL_LED_COUNT]{};
  GlobalPhysicalIndex index{};
  map::PhysicalLed physical{};
  disarmLedOutputs();
  if (!diagnosticSystemReady) {
    Serial.println("LED diagnostic refused: startup checks have not passed");
    return false;
  }
  // Validate the full requested physical range before enabling any outputs.
  if (map::channelLedCount(PhysicalChannel::CH1) != 37) return false;
  for (uint8_t pixel = 0; pixel < pixelCount; ++pixel) {
    if (!map::toGlobalIndex({PhysicalChannel::CH1, pixel}, index) ||
        !map::physicalLed(index, physical) ||
        physical.address.channel != PhysicalChannel::CH1 ||
        physical.address.index != pixel ||
        physical.role != LedRole::Map ||
        (pixel == 0 && physical.designator.value != 42)) {
      Serial.println("LED diagnostic refused: CH1 mapping mismatch");
      return false;
    }
  }
  Serial.printf("LED diagnostic: CH1 GPIO4, %u pixel(s), red=4/255, %lu ms per pixel; GPIO1 enable; POWER aborts walk\n",
                pixelCount, static_cast<unsigned long>(durationMs));
  if (!outputController.initialize(kOutputOperations, true) ||
      !outputController.arm(kOutputOperations, true)) {
    disarmLedOutputs();
    Serial.println("LED diagnostic FAIL: black-first initialization/arming");
    return false;
  }
  bool lit = true;
  for (uint8_t pixel = 0; pixel < pixelCount; ++pixel) {
    if (walkChannelOne && digitalRead(brooklyn::pins::BUTTON_POWER) == LOW) {
      Serial.println("LED diagnostic: aborted by POWER");
      lit = false;
      break;
    }
    if (!map::toGlobalIndex({PhysicalChannel::CH1, pixel}, index) ||
        !map::physicalLed(index, physical)) {
      lit = false;
      break;
    }
    Serial.printf("LED diagnostic: CH1 index %u LED%u\n", pixel,
                  physical.designator.value);
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
  (void)walkChannelOne;
  Serial.println("LED diagnostic unavailable on this target");
  return false;
#endif
}

}  // namespace

bool runFirstLedDiagnostic() { return runDiagnostic(false); }
bool runChannelOneDiagnostic() { return runDiagnostic(true); }

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
