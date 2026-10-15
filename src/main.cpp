// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "board_pins.h"
#include "controls.h"
#include "diagnostics.h"
#include "hardware.h"
#include "led_backend.h"
#include "led_engine.h"
#include "network_service.h"
#include "semantic_api.h"
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
  using namespace brooklyn::semantic;
  if (!selfCheck()) {
    Serial.println("Semantic map validation: FAIL");
    return;
  }
  uint16_t mapped = 0, stations = 0, segments = 0, physicalUnmapped = 0;
  for (uint16_t i = 0; i < kPhysicalLedCount; ++i) {
    if (semanticForLed(i) == nullptr) {
      ++physicalUnmapped;
      continue;
    }
    ++mapped;
    if (isStationLed(i)) ++stations;
    if (isSegmentLed(i)) ++segments;
  }
  Serial.printf(
      "Semantic map validation: PASS; map LEDs %u (tables %u), STATION %u (tables %u), "
      "SEGMENT %u (tables %u), registry entities %u, routes %u, physical LEDs %u "
      "(non-map %u)\n",
      mapped, kMapLedCount, stations, kStationLedCount, segments, kSegmentLedCount,
      kRegistryCount, kRouteCount, kPhysicalLedCount, physicalUnmapped);
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
