// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "board_pins.h"
#include "controls.h"
#include "diagnostics.h"
#include "display_output.h"
#include "hardware.h"
#include "led_engine.h"
#include "network_service.h"
#include "operational_display.h"
#include "ota_esp.h"
#include "semantic_api.h"
#include "train_pipeline.h"

#include <Arduino.h>

#include <string.h>

namespace {

// The pipeline builds each cycle's candidate independently of the committed display frame.
brooklyn::pipeline::TrainPipeline trainPipeline;
brooklyn::display::Controller operationalDisplay;
brooklyn::semantic::RgbFrame logicalDisplayFrame{};
bool systemHealthy = true;
uint32_t renderedFrameRevision = UINT32_MAX;
uint8_t renderedBrightness = UINT8_MAX;
uint32_t renderedRouteVisibilityMask = UINT32_MAX;

// Network service is disabled unless explicitly enabled; it never drives LEDs.
brooklyn::network::NetworkService networkService(trainPipeline);

brooklyn::display::WifiIndicator wifiIndicator(brooklyn::network::State state) {
  using brooklyn::display::WifiIndicator;
  using brooklyn::network::State;
  switch (state) {
    case State::Disabled: return WifiIndicator::Disabled;
    case State::NoCredentials:
    case State::Provisioning: return WifiIndicator::Unavailable;
    case State::WifiConnecting: return WifiIndicator::Connecting;
    case State::TemporarilyUnavailable: return WifiIndicator::Unavailable;
    case State::TimeSyncing: return WifiIndicator::TimeSyncing;
    case State::Ready: return WifiIndicator::Associated;
  }
  return WifiIndicator::Unavailable;
}

void reportDisplayTransition(brooklyn::display::State previous,
                             brooklyn::display::Event event, uint32_t nowMs) {
  using brooklyn::display::Event;
  const brooklyn::display::State current = operationalDisplay.state();
  if (previous == current) return;

  const char *reason = "display policy";
  if (event == Event::PartialHeld) reason = "partial cycle, holding prior frame";
  else if (event == Event::PartialCommitted) reason = "usable partial candidate committed";
  else if (event == Event::NoFeeds) reason = "no usable feeds";
  else if (event == Event::AgedStale) reason = "no usable frame within grace";
  else if (event == Event::HardStale) reason = "hard stale limit reached";
  if (event == Event::HardStale) {
    Serial.printf("Display: %s -> %s (%s; data age >= %lu ms)\n",
                  brooklyn::display::stateName(previous),
                  brooklyn::display::stateName(current), reason,
                  static_cast<unsigned long>(brooklyn::display::kHardStaleLimitMs));
    return;
  }
  if (event == Event::NoFeeds && !operationalDisplay.hasFrame()) {
    Serial.printf("Display: %s -> %s (%s; no prior usable frame)\n",
                  brooklyn::display::stateName(previous),
                  brooklyn::display::stateName(current), reason);
    return;
  }
  Serial.printf("Display: %s -> %s (%s; data age %lu ms)\n",
                brooklyn::display::stateName(previous),
                brooklyn::display::stateName(current), reason,
                static_cast<unsigned long>(operationalDisplay.dataAgeMs(nowMs)));
}

void updateLogicalDisplay(uint32_t nowMs) {
  bool changed = false;
  const uint8_t brightness = networkService.brightness();
  const uint32_t routeVisibilityMask = networkService.routeVisibilityMask();
  if (renderedFrameRevision != operationalDisplay.frameRevision() ||
      renderedBrightness != brightness ||
      renderedRouteVisibilityMask != routeVisibilityMask) {
    brooklyn::semantic::RenderConfig renderConfig{};
    renderConfig.brightness = brightness;
    renderConfig.routeVisibilityMask = routeVisibilityMask;
    brooklyn::semantic::renderTrainFrame(operationalDisplay.frame(),
                                         logicalDisplayFrame, renderConfig);
    renderedFrameRevision = operationalDisplay.frameRevision();
    renderedBrightness = brightness;
    renderedRouteVisibilityMask = routeVisibilityMask;
    changed = true;
  }
  const brooklyn::display::StatusIndicators status{
      systemHealthy,
      wifiIndicator(networkService.state()),
      networkService.state() == brooklyn::network::State::Provisioning,
      operationalDisplay.feedAttempted(),
      operationalDisplay.state(),
      brightness,
  };
  static brooklyn::display::StatusIndicators previousStatus{};
  static bool hasPreviousStatus = false;
  const bool statusChanged =
      !hasPreviousStatus ||
      previousStatus.systemHealthy != status.systemHealthy ||
      previousStatus.wifi != status.wifi ||
      previousStatus.provisioning != status.provisioning ||
      previousStatus.feedAttempted != status.feedAttempted ||
      previousStatus.displayState != status.displayState ||
      previousStatus.brightness != status.brightness;
  changed = changed || statusChanged;
  static bool reportedStatusMapFailure = false;
  const bool statusApplied =
      brooklyn::display::applyStatusOverlay(logicalDisplayFrame, status);
  if (!statusApplied && !reportedStatusMapFailure) {
    reportedStatusMapFailure = true;
    Serial.println("Display: canonical status LED mapping failed");
  }
  if (statusApplied) {
    previousStatus = status;
    hasPreviousStatus = true;
    static bool outputPending = true;
    static bool reportedOutputFailure = false;
    outputPending = outputPending || changed;
    if (outputPending) {
      if (brooklyn::display::output::publish(&logicalDisplayFrame)) {
        outputPending = false;
        reportedOutputFailure = false;
      } else if (!reportedOutputFailure) {
        reportedOutputFailure = true;
        Serial.println("Display: output bridge rejected completed RGB frame");
      }
    }
  }
}

void runXiaoLedTest() {
  brooklyn::diagnostics::begin();
  brooklyn::led::begin();
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: T LED self-test; M semantic report; W Wi-Fi setup portal; X,X forget Wi-Fi");
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

  brooklyn::diagnostics::begin();
  const bool pipelineReady = brooklyn::pipeline::selfCheckEmptyCycle(trainPipeline);
  systemHealthy = hardwareReady && pipelineReady;
  Serial.printf("Train pipeline self-check (no feeds, no LED output): %s\n",
                pipelineReady ? "PASS" : "FAIL");
  networkService.begin();
#if BROOKLYN_NETWORK_ENABLED
  brooklyn::ota::healthSetupComplete(systemHealthy);
#endif
  if (!hardwareReady) {
    Serial.println("ERROR: I2C initialization failed; LED outputs remain disabled.");
  }
#if defined(BROOKLYN_TARGET_XIAO)
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: T LED self-test; M semantic report; W Wi-Fi setup portal; X,X forget Wi-Fi");
#endif
}

void loop() {
  brooklyn::controls::update();
#if BROOKLYN_NETWORK_ENABLED
  brooklyn::ota::healthService(millis());
#endif
  if (brooklyn::controls::consumeWifiRecoveryRequest()) {
    networkService.requestProvisioning();
  }
  brooklyn::diagnostics::update();
  networkService.update(millis());
  brooklyn::transport::CycleResult completedCycle{};
  uint32_t completedAtMs = 0;
  if (networkService.consumeCompletedCycle(completedCycle, completedAtMs)) {
    using brooklyn::display::CycleQuality;
    using brooklyn::transport::CycleOutcome;
    if (completedCycle.outcome == CycleOutcome::Complete ||
        completedCycle.outcome == CycleOutcome::Partial ||
        completedCycle.outcome == CycleOutcome::NoFeeds) {
      const brooklyn::display::State previous = operationalDisplay.state();
      const brooklyn::pipeline::Counters &counts = trainPipeline.counters();
      if (counts.feedsProcessed != completedCycle.processed ||
          counts.feedDecodeFailures > counts.feedsProcessed) {
        Serial.println("Display: poll/pipeline feed accounting mismatch; candidate not committed");
      } else {
        const uint32_t usableFeeds = counts.feedsProcessed - counts.feedDecodeFailures;
        CycleQuality quality = CycleQuality::NoFeeds;
        if (usableFeeds != 0) {
          quality = completedCycle.outcome == CycleOutcome::Complete &&
                            usableFeeds == completedCycle.processed &&
                            completedCycle.processed == completedCycle.attempted
                        ? CycleQuality::Complete
                        : CycleQuality::Partial;
        }
        const brooklyn::display::Event event = operationalDisplay.completeCycle(
            trainPipeline.frame(), quality, completedAtMs);
        reportDisplayTransition(previous, event, completedAtMs);
      }
    }
  }
  const uint32_t displayNowMs = millis();
  const brooklyn::display::State beforeAging = operationalDisplay.state();
  const brooklyn::display::Event agingEvent = operationalDisplay.update(displayNowMs);
  reportDisplayTransition(beforeAging, agingEvent, displayNowMs);
  updateLogicalDisplay(displayNowMs);
#if BROOKLYN_NETWORK_ENABLED
  brooklyn::web::StatusSnapshot webStatus{};
  strncpy(webStatus.deviceName, brooklyn::web::kDefaultDeviceName,
          sizeof(webStatus.deviceName) - 1);
  webStatus.displayState = operationalDisplay.state();
  webStatus.hasDataAge = operationalDisplay.hasFrame();
  if (webStatus.hasDataAge) {
    webStatus.dataAgeSeconds = operationalDisplay.dataAgeMs(displayNowMs) / 1000;
  }
  webStatus.hasCompleteAge = operationalDisplay.hasCompleteCycle();
  if (webStatus.hasCompleteAge) {
    webStatus.completeAgeSeconds =
        operationalDisplay.completeAgeMs(displayNowMs) / 1000;
  }
  webStatus.resolvedTrains = operationalDisplay.frame().resolvedTrains;
  webStatus.occupiedLeds = operationalDisplay.frame().occupiedLeds;
  webStatus.collisionCount = operationalDisplay.frame().collisionLeds;
  networkService.setWebStatus(webStatus);
#endif
#if defined(BROOKLYN_TARGET_XIAO)
  static uint32_t forgetArmedAtMs = 0;
  while (Serial.available() > 0) {
    const int command = Serial.read();
    if (command == 't' || command == 'T') {
      runXiaoLedTest();
    } else if (command == 'm' || command == 'M') {
      runXiaoLogicalMapTest();
    } else if (command == 'w' || command == 'W') {
      networkService.requestProvisioning();
    } else if (command == 'x' || command == 'X') {
      const uint32_t now = millis();
      if (forgetArmedAtMs != 0 && static_cast<uint32_t>(now - forgetArmedAtMs) < 5000) {
        forgetArmedAtMs = 0;
        (void)networkService.forgetCredentials();
      } else {
        forgetArmedAtMs = now != 0 ? now : 1;
        Serial.println("Send X again within 5 s to ERASE stored Wi-Fi credentials");
      }
    }
  }
#endif
  delay(5);
}
