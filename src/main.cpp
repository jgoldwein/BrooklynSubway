// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "board_pins.h"
#include "controls.h"
#include "diagnostics.h"
#include "hardware.h"
#include "led_engine.h"

#include <Arduino.h>

namespace {

void runXiaoLedTest() {
  brooklyn::diagnostics::begin();
  brooklyn::led::begin();
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: press T to rerun LED self-test");
}

}  // namespace

void setup() {
  const bool hardwareReady = brooklyn::hardware::beginSafe();

  Serial.begin(115200);
  const uint32_t serialWaitStartedMs = millis();
#if defined(BROOKLYN_TARGET_XIAO)
  constexpr uint32_t SERIAL_CONNECT_TIMEOUT_MS = 10000;
#else
  constexpr uint32_t SERIAL_CONNECT_TIMEOUT_MS = 1500;
#endif
  while (!Serial &&
         static_cast<uint32_t>(millis() - serialWaitStartedMs) <
             SERIAL_CONNECT_TIMEOUT_MS) {
    delay(10);
  }

  brooklyn::diagnostics::begin();
  if (!hardwareReady) {
    Serial.println("ERROR: I2C initialization failed; LED outputs remain disabled.");
  }
  brooklyn::controls::begin();
  brooklyn::led::begin();
#if defined(BROOKLYN_TARGET_XIAO)
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: press T to rerun LED self-test");
#endif
}

void loop() {
  brooklyn::controls::update();
  brooklyn::diagnostics::update();
#if defined(BROOKLYN_TARGET_XIAO)
  while (Serial.available() > 0) {
    const int command = Serial.read();
    if (command == 't' || command == 'T') {
      runXiaoLedTest();
    }
  }
#endif
  delay(5);
}
