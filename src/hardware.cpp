// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "hardware.h"

#include "board_pins.h"

#if defined(BROOKLYN_TARGET_PCB)
#include <Wire.h>
#endif

namespace brooklyn::hardware {

bool beginSafe() {
#if defined(BROOKLYN_TARGET_PCB)
  digitalWrite(pins::LED_ENABLE, LOW);
  pinMode(pins::LED_ENABLE, OUTPUT);
  digitalWrite(pins::LED_ENABLE, LOW);

  pinMode(pins::ROTARY_A, INPUT);
  pinMode(pins::ROTARY_B, INPUT);
  pinMode(pins::ROTARY_PUSH, INPUT);

  pinMode(pins::BUTTON_TEST, INPUT);
  pinMode(pins::BUTTON_ROUTE, INPUT);
  pinMode(pins::BUTTON_DIM, INPUT);
  pinMode(pins::BUTTON_POWER, INPUT);
  pinMode(pins::BUTTON_MODE, INPUT);
  pinMode(pins::BUTTON_BRIGHT, INPUT);

  pinMode(pins::POWER_STAT, INPUT);

  digitalWrite(pins::AVI_LOCATION_LED, LOW);
  pinMode(pins::AVI_LOCATION_LED, OUTPUT);

  return Wire.begin(pins::I2C_SDA, pins::I2C_SCL);
#else
  return true;
#endif
}

}  // namespace brooklyn::hardware
