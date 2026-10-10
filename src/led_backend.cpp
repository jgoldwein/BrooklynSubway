// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_backend.h"

#include "board_pins.h"
#include "led_map.h"

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
bool armed = false;

void forceDataPinsLow() {
  for (uint8_t pin : brooklyn::pins::LED_DATA) {
    digitalWrite(pin, LOW);
  }
}

}  // namespace
#endif

namespace brooklyn::led::backend {

bool initializePhysicalLedBackend() {
#if defined(BROOKLYN_TARGET_PCB)
  disarmLedOutputs();
  auto *strips = productionStrips();
  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    if (strips[channel].getPixels() == nullptr) {
      Serial.printf("LED backend initialization failed: CH%u buffer allocation\n",
                    channel + 1);
      forceDataPinsLow();
      initialized = false;
      return false;
    }
  }

  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    strips[channel].begin();
    strips[channel].clear();
    strips[channel].show();
    digitalWrite(pins::LED_DATA[channel], LOW);
  }

  forceDataPinsLow();
  initialized = true;
  armed = false;
  Serial.println(
      "Physical LED backend initialized; black frame sent with outputs DISARMED.");
  return true;
#else
  return false;
#endif
}

bool armLedOutputs() {
#if defined(BROOKLYN_TARGET_PCB)
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1
  if (!initialized) {
    Serial.println("LED output arm refused: physical backend not initialized.");
    disarmLedOutputs();
    return false;
  }
  disarmLedOutputs();
  digitalWrite(pins::LED_ENABLE, HIGH);
  armed = true;
  Serial.println("Physical LED outputs ARMED.");
  return true;
#else
  disarmLedOutputs();
  Serial.println(
      "LED output arm refused: disabled by BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0.");
  return false;
#endif
#else
  Serial.println("LED output arm refused: XIAO is software-simulation only.");
  return false;
#endif
}

void disarmLedOutputs() {
#if defined(BROOKLYN_TARGET_PCB)
  digitalWrite(pins::LED_ENABLE, LOW);
  armed = false;
  if (initialized) {
    forceDataPinsLow();
  }
#endif
}

bool physicalLedOutputsArmed() {
#if defined(BROOKLYN_TARGET_PCB)
  return armed;
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
  if (!initialized || !armed || !physicalLedOutputPermissionEnabled()) {
    return false;
  }
  if (frame == nullptr || count != map::TOTAL_LED_COUNT) {
    Serial.println("LED frame rejected: invalid framebuffer; outputs DISARMED.");
    disarmLedOutputs();
    return false;
  }

  Adafruit_NeoPixel *strips = productionStrips();
  uint16_t start = 0;
  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    for (uint8_t index = 0; index < pins::LED_COUNT[channel]; ++index) {
      const Rgb color = frame[start + index].color;
      strips[channel].setPixelColor(index, color.red, color.green, color.blue);
    }
    strips[channel].show();
    digitalWrite(pins::LED_DATA[channel], LOW);
    start += pins::LED_COUNT[channel];
  }
  forceDataPinsLow();
  return true;
#else
  (void)frame;
  (void)count;
  return false;
#endif
}

}  // namespace brooklyn::led::backend
