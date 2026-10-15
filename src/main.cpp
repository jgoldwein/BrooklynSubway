// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "board_pins.h"
#include "controls.h"
#include "diagnostics.h"
#include "hardware.h"
#include "led_backend.h"
#include "led_engine.h"
#include "logical_map.h"
#include "network_service.h"
#include "train_pipeline.h"

#include <Arduino.h>

namespace {

// Phase 4I: in-memory train pipeline. No feed source exists yet; it is exercised
// once at startup with an empty cycle and never drives LEDs.
brooklyn::pipeline::TrainPipeline trainPipeline;

// Phase 4J: GTFS-RT transport. Disabled unless built with
// -DBROOKLYN_NETWORK_ENABLED=1 and credentials exist; never drives LEDs.
brooklyn::network::NetworkService networkService(trainPipeline);

void runXiaoLedTest() {
  brooklyn::diagnostics::begin();
  brooklyn::led::begin();
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: T reruns LED self-test; M validates logical map");
}

void runXiaoLogicalMapTest() {
  brooklyn::subway::ValidationReport report{};
  const char *failureReason = nullptr;
  if (!brooklyn::subway::runSelfTest(report, failureReason)) {
    Serial.printf("Logical map validation: FAIL - %s\n",
                  failureReason == nullptr ? "unknown failure" : failureReason);
    return;
  }

  Serial.printf(
      "Logical map validation: PASS; elements %u, stations %u, segments %u, "
      "assigned %u, unassigned %u, routes %u\n",
      report.elements, report.stations, report.segments, report.assigned,
      report.unassigned, report.routes);
  for (uint16_t index = 0; index < 3 && index < report.elements; ++index) {
    brooklyn::subway::MapElement element{};
    if (brooklyn::subway::elementAt(index, element)) {
      Serial.printf("  map ID %u: CH%u[%u], global %u, LED%u (unassigned)\n",
                    element.id.value,
                    static_cast<uint8_t>(element.physical.address.channel),
                    element.physical.address.index,
                    element.physical.globalIndex.value,
                    element.physical.designator.value);
    }
  }
  Serial.println("No verified route/station assignments are currently available.");
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

  brooklyn::controls::begin();
  brooklyn::led::begin();
#if defined(BROOKLYN_TARGET_PCB)
  const bool physicalBackendReady =
      brooklyn::led::backend::initializePhysicalLedBackend();
#endif

  brooklyn::diagnostics::begin();
  Serial.printf("Train pipeline self-check (no feeds, no LED output): %s\n",
                brooklyn::pipeline::selfCheckEmptyCycle(trainPipeline) ? "PASS" : "FAIL");
  networkService.begin();
  if (!hardwareReady) {
    Serial.println("ERROR: I2C initialization failed; LED outputs remain disabled.");
  }
#if defined(BROOKLYN_TARGET_PCB)
  if (!physicalBackendReady) {
    brooklyn::led::backend::disarmLedOutputs();
    Serial.println(
        "ERROR: Physical LED backend initialization failed; outputs DISARMED.");
  }
#endif
#if defined(BROOKLYN_TARGET_XIAO)
  (void)brooklyn::led::backend::initializePhysicalLedBackend();
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: T reruns LED self-test; M validates logical map");
#endif
}

void loop() {
  brooklyn::controls::update();
  brooklyn::diagnostics::update();
  networkService.update(millis());
#if defined(BROOKLYN_TARGET_XIAO)
  while (Serial.available() > 0) {
    const int command = Serial.read();
    if (command == 't' || command == 'T') {
      runXiaoLedTest();
    } else if (command == 'm' || command == 'M') {
      runXiaoLogicalMapTest();
    }
  }
#endif
  delay(5);
}
