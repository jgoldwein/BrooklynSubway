#include "personalization.h"
// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "board_pins.h"
#include "controls.h"
#include "diagnostics.h"
#include "display_output.h"
#include "live_display.h"
#include <time.h>
#include "hardware.h"
#include "oled_clock.h"
#include "led_engine.h"
#include "led_backend.h"
#include "network_service.h"
#include "operational_display.h"
#include "ota_esp.h"
#include "semantic_api.h"
#include "telemetry_service.h"
#include "train_pipeline.h"

#include <Arduino.h>

#include <string.h>
#include <esp_timer.h>
#if BROOKLYN_NETWORK_ENABLED
#include <WiFi.h>
#endif

#if defined(BROOKLYN_NETWORK_ENABLED)
// The HTTPS release download runs a TLS handshake on the loop task; the 8 KB default overflowed.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
#endif

namespace {

// The pipeline builds each cycle's candidate independently of the committed display frame.
brooklyn::pipeline::TrainPipeline trainPipeline;
brooklyn::display::Controller operationalDisplay;
brooklyn::semantic::RgbFrame logicalDisplayFrame{};
bool systemHealthy = true;
uint32_t renderedFrameRevision = UINT32_MAX;
uint8_t renderedBrightness = UINT8_MAX;
uint32_t renderedRouteVisibilityMask = UINT32_MAX;
bool renderedApproachingLit = true;
brooklyn::transport::CycleResult lastCycle{};
bool hasLastCycle = false;

static_assert(static_cast<uint8_t>(brooklyn::display::State::NoData) ==
                  static_cast<uint8_t>(brooklyn::telemetry::DisplayState::NoData) &&
              static_cast<uint8_t>(brooklyn::display::State::StaleHold) ==
                  static_cast<uint8_t>(brooklyn::telemetry::DisplayState::StaleHold) &&
              static_cast<uint8_t>(brooklyn::display::State::DegradedPartial) ==
                  static_cast<uint8_t>(brooklyn::telemetry::DisplayState::DegradedPartial),
              "telemetry display state order must match display::State");


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
  brooklyn::telemetry::noteDisplayTransition(
      static_cast<brooklyn::telemetry::DisplayState>(previous),
      static_cast<brooklyn::telemetry::DisplayState>(current));

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

// Hands the telemetry task a compact copy of application state; never blocks on the network.
void publishTelemetrySnapshot(uint32_t nowMs) {
  using brooklyn::transport::CycleOutcome;
  brooklyn::telemetry::Snapshot snap;
  snap.networkReady = networkService.state() == brooklyn::network::State::Ready;
  snap.systemHealthy = systemHealthy;
  snap.display = static_cast<brooklyn::telemetry::DisplayState>(operationalDisplay.state());
  snap.brightness = networkService.brightness();
  snap.hasRouteMask = true;
  snap.routeVisibilityMask = networkService.routeVisibilityMask();
  if (hasLastCycle) {
    snap.poll = lastCycle.outcome == CycleOutcome::Complete  ? brooklyn::telemetry::PollState::Complete
                : lastCycle.outcome == CycleOutcome::Partial ? brooklyn::telemetry::PollState::Partial
                : lastCycle.outcome == CycleOutcome::NoFeeds ? brooklyn::telemetry::PollState::NoFeeds
                                                              : brooklyn::telemetry::PollState::Unknown;
    snap.feedAttempted = lastCycle.attempted;
    snap.feedSuccess = lastCycle.processed;
    snap.hasFeedMask = true;
    snap.feedMask = static_cast<uint8_t>(lastCycle.processedMask & 0xff);
  }
  snap.hasCompleteAge = operationalDisplay.hasCompleteCycle();
  if (snap.hasCompleteAge) snap.completeAgeSeconds = operationalDisplay.completeAgeMs(nowMs) / 1000;
  snap.vehiclesProcessed = networkService.pipelineCounters().vehiclesSeen;
  snap.vehiclesResolved = operationalDisplay.frame().resolvedTrains;
  snap.occupiedMapLeds = operationalDisplay.frame().occupiedLeds;
  snap.hasCollisions = true;
  snap.collisionCount = operationalDisplay.frame().collisionLeds;
  brooklyn::telemetry::update(snap);
}

void updateLogicalDisplay(uint32_t nowMs) {
  brooklyn::oled::setSetupMode(networkService.state() == brooklyn::network::State::Provisioning);
  brooklyn::oled::setTrackedTrains(operationalDisplay.frame().resolvedTrains,
      systemHealthy && operationalDisplay.hasFrame() &&
      (operationalDisplay.state() == brooklyn::display::State::Fresh ||
       operationalDisplay.state() == brooklyn::display::State::DegradedPartial));
  bool changed = false;
  const uint8_t brightness = networkService.brightness();
  const uint32_t unknownTiming = networkService.unknownTiming();
  const auto colors=networkService.trainColors();
  static auto renderedColors=brooklyn::semantic::kDefaultTrainColors;
  static uint32_t renderedUnknownTiming = UINT32_MAX;
  const uint32_t routeVisibilityMask = networkService.routeVisibilityMask();
  const bool approachingLit = brooklyn::semantic::approachingPhaseLit(nowMs);
  if (renderedFrameRevision != operationalDisplay.frameRevision() ||
      renderedBrightness != brightness || renderedUnknownTiming != unknownTiming || !brooklyn::semantic::sameTrainColors(renderedColors,colors) ||
      renderedRouteVisibilityMask != routeVisibilityMask ||
      (operationalDisplay.frame().approachingTrains != 0 &&
       renderedApproachingLit != approachingLit)) {
    brooklyn::semantic::RenderConfig renderConfig{};
    renderConfig.brightness = brightness;
    renderConfig.routeVisibilityMask = routeVisibilityMask;
    renderConfig.approachingLit = approachingLit;
    brooklyn::semantic::renderTrainFrame(operationalDisplay.frame(),
                                         logicalDisplayFrame, renderConfig);
    renderedFrameRevision = operationalDisplay.frameRevision();
    renderedBrightness = brightness;
    renderedUnknownTiming = unknownTiming;
    renderedColors=colors;
    renderedRouteVisibilityMask = routeVisibilityMask;
    renderedApproachingLit = approachingLit;
    changed = true;
  }
  const brooklyn::display::StatusIndicators status{
      systemHealthy,
      wifiIndicator(networkService.state()),
      networkService.state() == brooklyn::network::State::Provisioning || networkService.state() == brooklyn::network::State::NoCredentials,
      operationalDisplay.feedAttempted(),
      operationalDisplay.state(),
      brightness,
      static_cast<uint8_t>(brooklyn::oled::selectedScreen()),
  };
  brooklyn::hardware::setFeedIndicator(systemHealthy &&
      operationalDisplay.state() == brooklyn::display::State::Fresh &&
      operationalDisplay.hasFrame() && operationalDisplay.dataAgeMs(nowMs) <= brooklyn::display::kPartialGraceMs &&
      networkService.state() == brooklyn::network::State::Ready);
  static brooklyn::display::StatusIndicators previousStatus{};
  brooklyn::hardware::observeService(
      networkService.state()==brooklyn::network::State::Ready && operationalDisplay.hasFrame() &&
      operationalDisplay.dataAgeMs(nowMs)<=brooklyn::display::kPartialGraceMs,
      systemHealthy && !status.provisioning,
      (networkService.screenPreferences()&brooklyn::config::kServiceSoundsBit)!=0);
  static bool hasPreviousStatus = false;
  const bool statusChanged =
      !hasPreviousStatus ||
      previousStatus.systemHealthy != status.systemHealthy ||
      previousStatus.wifi != status.wifi ||
      previousStatus.provisioning != status.provisioning ||
      previousStatus.feedAttempted != status.feedAttempted ||
      previousStatus.displayState != status.displayState ||
      previousStatus.oledScreen != status.oledScreen ||
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
    if (changed) {
      brooklyn::display::live::Snapshot snapshot{};
      snapshot.frame = operationalDisplay.frame();
      snapshot.status = status;
      snapshot.frameAtMs = nowMs - operationalDisplay.dataAgeMs(nowMs);
      snapshot.routeMask = routeVisibilityMask;
      snapshot.brightness = brightness;
      snapshot.unknownTiming = unknownTiming;
      snapshot.colors=colors;
      snapshot.hasFrame = operationalDisplay.hasFrame();
      snapshot.clockUnix = static_cast<uint32_t>(time(nullptr));
      snapshot.clockAtMs = nowMs;
      brooklyn::display::live::publish(snapshot);
    }
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
  networkService.begin(); // Complete any pending defaults reset before loading saved text.
  brooklyn::personal::load();
#if BROOKLYN_NETWORK_ENABLED
  brooklyn::alerts::loadPreferences();
#endif
  brooklyn::oled::setScreenPreferences(networkService.screenPreferences());
  brooklyn::hardware::setQuiet(brooklyn::oled::quietHoursActive(time(nullptr)),brooklyn::oled::quietMapBrightness());
  bool startupIndicatorsReady=true;
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  brooklyn::hardware::enableStatusReadyCues();
  uint8_t startupCap=networkService.brightness();
  if(brooklyn::oled::quietHoursActive(time(nullptr)) && startupCap>brooklyn::oled::quietMapBrightness()) startupCap=brooklyn::oled::quietMapBrightness();
  if(networkService.state()==brooklyn::network::State::NoCredentials || networkService.state()==brooklyn::network::State::Provisioning) startupCap=0;
  startupIndicatorsReady=brooklyn::led::backend::showStartupStatus(hardwareReady,false,startupCap);
  const uint32_t redPhaseStarted=millis();
#endif
  const bool pipelineReady = brooklyn::pipeline::selfCheckEmptyCycle(trainPipeline);
  systemHealthy = hardwareReady && pipelineReady && startupIndicatorsReady;
  brooklyn::led::backend::setDiagnosticSystemReady(systemHealthy);
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  constexpr uint32_t kInitialRedMs=150;
  if(systemHealthy) {
    const uint32_t elapsed=millis()-redPhaseStarted;
    if(startupCap && elapsed<kInitialRedMs) delay(kInitialRedMs-elapsed);
    if(!brooklyn::led::backend::showStartupStatus(hardwareReady,true,startupCap)) {
      systemHealthy=false;brooklyn::led::backend::setDiagnosticSystemReady(false);
    } else if(startupCap) {
      brooklyn::hardware::observeStatusGreen(1);
      brooklyn::hardware::serviceKnobClick(millis(),true);
      delay(90); // Boot-only SYSTEM cue; runtime cues never delay input sampling.
      brooklyn::hardware::serviceKnobClick(millis(),true);
    }
  }
#endif
  Serial.printf("Train pipeline self-check (no feeds, station LEDs dark): %s\n",
                pipelineReady ? "PASS" : "FAIL");
  brooklyn::telemetry::begin();
#if BROOKLYN_NETWORK_ENABLED
  brooklyn::ota::healthSetupComplete(systemHealthy);
#endif
  if (!hardwareReady) {
    Serial.println("ERROR: I2C initialization failed; LED outputs remain disabled.");
  }
  brooklyn::oled::setScreenPreferences(networkService.screenPreferences());
  brooklyn::oled::setSetupMode(networkService.state()==brooklyn::network::State::Provisioning || networkService.state()==brooklyn::network::State::NoCredentials);
  brooklyn::hardware::setQuiet(brooklyn::oled::quietHoursActive(time(nullptr)),brooklyn::oled::quietMapBrightness());
  brooklyn::hardware::indicateStartup(systemHealthy);
  if (systemHealthy) (void)brooklyn::display::live::begin();
  if(!brooklyn::display::live::available()) brooklyn::oled::setScreenPreferences(networkService.screenPreferences() & ~brooklyn::config::kStartupDedicationBit);
  if (hardwareReady) brooklyn::oled::begin(networkService.oledAddress(), networkService.oledContrast());
#if defined(BROOKLYN_TARGET_XIAO)
  if (brooklyn::led::runSelfTest()) {
    brooklyn::led::runXiaoDemonstration();
  }
  Serial.println("XIAO bench: T LED self-test; M semantic report; W Wi-Fi setup portal; X,X forget Wi-Fi");
#endif
}

namespace {
void publishOledScreenData(uint32_t nowMs) {
  static bool published = false;
  static uint32_t lastAtMs = 0;
  if (published && static_cast<uint32_t>(nowMs-lastAtMs) < 1000) return;
  published = true; lastAtMs = nowMs;
  brooklyn::oled::ScreenData data;
  data.sampledAtMs = nowMs;
  data.sound=brooklyn::hardware::lastSound();
  data.unixTime=static_cast<uint32_t>(time(nullptr));
  data.weather=networkService.weatherStatus();
  data.alerts=brooklyn::alerts::summary(static_cast<uint32_t>(time(nullptr)));
  data.uptimeSeconds = static_cast<uint64_t>(esp_timer_get_time())/1000000u;
  const auto live = brooklyn::display::live::stats();
  brooklyn::oled::setReturnSeconds(networkService.oledReturnSeconds());
  brooklyn::oled::setScreenPreferences(networkService.screenPreferences());
  data.indications = live.indications;
  data.routeMask=live.routeFocusActive?live.effectiveRouteMask:networkService.routeVisibilityMask();
  brooklyn::oled::setLineCounts(data,operationalDisplay.frame(),live.routeLights);
  data.lightsEnabled = brooklyn::display::live::enabled();
  data.hasUpdate = operationalDisplay.hasFrame();
  data.updateAgeSeconds = operationalDisplay.dataAgeMs(nowMs)/1000;
  if (hasLastCycle) {
    data.feedsSucceeded = lastCycle.processed;
    data.feedsTotal = brooklyn::network::kFeedCount;
  }
  using brooklyn::oled::FeedHealth;
  using brooklyn::display::State;
  switch (operationalDisplay.state()) {
    case State::Fresh: data.feedHealth=FeedHealth::Fresh; break;
    case State::DegradedPartial: data.feedHealth=FeedHealth::Partial; break;
    case State::DegradedHold: data.feedHealth=FeedHealth::Held; break;
    case State::StaleHold: data.feedHealth=FeedHealth::Stale; break;
    case State::NoData: data.feedHealth=FeedHealth::NoData; break;
    default: break;
  }
#if BROOKLYN_NETWORK_ENABLED
  uint8_t mac[6]{};WiFi.macAddress(mac);
  if(mac[0]||mac[1]||mac[2]||mac[3]||mac[4]||mac[5])
    snprintf(data.mac,sizeof(data.mac),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  data.wifiConnected = WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP())!=0;
  data.wifiConnecting = networkService.state()==brooklyn::network::State::WifiConnecting ||
      networkService.state()==brooklyn::network::State::TemporarilyUnavailable;
  if (data.wifiConnected) {
    const String ssid=WiFi.SSID();
    snprintf(data.ssid,sizeof(data.ssid),"%s",ssid.c_str());
    data.rssi = WiFi.RSSI();
    const auto ip = WiFi.localIP();
    snprintf(data.ip, sizeof(data.ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }
  snprintf(data.firmware, sizeof(data.firmware), "%s", brooklyn::ota::WebOtaSession::firmwareVersion());
#else
  snprintf(data.firmware, sizeof(data.firmware), "offline build");
#endif
  brooklyn::oled::publishScreenData(data);
}

void serviceBrightnessButtons() {
  brooklyn::controls::BrightnessRequest request;
  // Bounded queue; input is sampled by the live task during network fetches.
  for (uint8_t count = 0; count < 8 && brooklyn::display::live::consumeBrightnessRequest(request); ++count) {
    if (brooklyn::display::live::stats().powerMode != request.mode) continue;
    const bool screen = request.mode == 1;
    const uint8_t previous = screen ? networkService.oledContrast() : networkService.brightness();
    const uint8_t value = brooklyn::controls::steppedBrightness(previous, screen, request.brighter);
    if (value != previous && !networkService.setPhysicalBrightness(screen, value)) {
      Serial.println("Brightness buttons: could not save setting; unchanged"); continue;
    }
    if(value!=previous)brooklyn::hardware::requestSettingsSaved();
    if (screen) brooklyn::oled::setContrast(value);
    else updateLogicalDisplay(millis());
    if (brooklyn::display::live::stats().powerMode != request.mode) continue;
    if (!brooklyn::display::live::showBrightnessFeedback(request, value)) continue;
    brooklyn::hardware::signalBrightness(value, screen ? 255 : 16);
    Serial.printf("Brightness buttons: %s %u/%u%s\n", screen ? "OLED" : "LED", value,
        screen ? 255 : 16, value == previous ? " (limit)" : " saved");
  }
}
}

void loop() {
  if (brooklyn::display::live::consumeRebootRequest()) {
    Serial.println("POWER: 3-second hold; rebooting");
    brooklyn::display::live::disable();
    delay(100);
    ESP.restart();
  }
  brooklyn::controls::update();
  serviceBrightnessButtons();
  if(brooklyn::display::live::consumeEasterEggRequest() &&
      networkService.state()!=brooklyn::network::State::Provisioning &&
      networkService.state()!=brooklyn::network::State::NoCredentials) {
    static uint8_t nextEgg=0;
    const auto before=brooklyn::display::live::stats();
    const bool resume=brooklyn::display::live::enabled() || before.automaticStartPending;
    brooklyn::eggs::Kind selected;
    const bool completed=brooklyn::eggs::nextEnabled(nextEgg,selected) && brooklyn::led::backend::runEasterEgg(selected);
    if(completed && resume) brooklyn::display::live::restoreAfterSuccessfulTest(true);
  }
  const bool backgroundTest = brooklyn::display::live::consumeTestRequest();
  if (brooklyn::controls::consumeLedDiagnosticRequest() || backgroundTest) {
    const auto beforeTest = brooklyn::display::live::stats();
    const bool resumeLive = brooklyn::display::live::enabled() || beforeTest.automaticStartPending;
    const bool completed = brooklyn::led::backend::runAllRouteDiagnostics(
        brooklyn::led::backend::RouteDiagnosticSpeed::Fast);
    // A POWER/recovery abort must leave the display OFF. Successful tests may
    // resume a prior live session only if the cached data is still healthy/fresh.
    if (completed && (resumeLive || beforeTest.powerCanResume))
      brooklyn::display::live::restoreAfterSuccessfulTest(resumeLive);
  }
#if BROOKLYN_NETWORK_ENABLED
  brooklyn::ota::healthService(millis());
#endif
  const bool liveRecovery = brooklyn::display::live::consumeRecoveryRequest();
  if (brooklyn::controls::consumeWifiRecoveryRequest() || liveRecovery) {
    networkService.requestProvisioning();
  }
  brooklyn::diagnostics::update();
  networkService.update(millis());
  serviceBrightnessButtons();
  brooklyn::transport::CycleResult completedCycle{};
  uint32_t completedAtMs = 0;
  if (networkService.consumeCompletedCycle(completedCycle, completedAtMs)) {
    lastCycle = completedCycle;
    hasLastCycle = true;
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
        trainPipeline.finishCycle(static_cast<uint32_t>(time(nullptr)),
            operationalDisplay.willCommit(quality, completedAtMs));
        const brooklyn::display::Event event = operationalDisplay.completeCycle(
            trainPipeline.frame(), quality, completedAtMs);
        reportDisplayTransition(previous, event, completedAtMs);
      }
    }
    networkService.finishCycleReporting();
  }
  const uint32_t displayNowMs = millis();
  const brooklyn::display::State beforeAging = operationalDisplay.state();
  const brooklyn::display::Event agingEvent = operationalDisplay.update(displayNowMs);
  reportDisplayTransition(beforeAging, agingEvent, displayNowMs);
  updateLogicalDisplay(displayNowMs);
  publishOledScreenData(displayNowMs);
  publishTelemetrySnapshot(displayNowMs);
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
  webStatus.approachingTrains = operationalDisplay.frame().approachingTrains;
  webStatus.occupiedLeds = operationalDisplay.frame().occupiedLeds;
  webStatus.collisionCount = operationalDisplay.frame().collisionLeds;
  // NetworkService retains the last completed report while its worker builds the next.
  networkService.setWebStatus(webStatus);
#endif
#if defined(BROOKLYN_TARGET_XIAO)
  static uint32_t forgetArmedAtMs = 0;
  while (Serial.available() > 0) {
    const int command = Serial.read();
    if (brooklyn::telemetry::handleCommand(command)) {
      continue;
    }
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
