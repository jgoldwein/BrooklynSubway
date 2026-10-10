#include "board_pins.h"
#include "controls.h"
#include "diagnostics.h"
#include "hardware.h"

#include <Arduino.h>

void setup() {
  const bool hardwareReady = brooklyn::hardware::beginSafe();

  Serial.begin(115200);
  const uint32_t serialWaitStartedMs = millis();
  while (!Serial &&
         static_cast<uint32_t>(millis() - serialWaitStartedMs) < 1500) {
    delay(10);
  }

  brooklyn::diagnostics::begin();
  if (!hardwareReady) {
    Serial.println("ERROR: I2C initialization failed; LED outputs remain disabled.");
  }
  brooklyn::controls::begin();
}

void loop() {
  brooklyn::controls::update();
  brooklyn::diagnostics::update();
  delay(5);
}
