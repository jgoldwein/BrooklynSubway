// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Provisioned production/bench monitoring, publish-only MQTT with verified TLS.
// Network builds load an individual identity from bkmap_mon; no compiled credentials.
// Worker/socket allocation is deferred until valid credentials are saved and enabled.
// Non-network builds have no MQTT task. Firmware defaults erase the monitoring record.
//
// The service owns a dedicated FreeRTOS task. Heartbeats, reconnects and event drains are
// driven by that task's own clock and never by loop(), the MTA cycle, or SNTP. loop()
// only hands over a small snapshot (and display transitions) through a short mutex.
// Never touches LEDs, GPIO, LED_ENABLE or any hardware output.

#pragma once

#include <stdint.h>

#include "telemetry_core.h"
#include "monitor_config.h"
#include "remote_diagnostics.h"

namespace brooklyn::telemetry {

// Latest stable application state, published by loop() each iteration.
struct Snapshot {
  Diagnostics diagnostics{};
  char wanIpv4[16]{};
  bool networkReady = false;  // network::State::Ready (Wi-Fi up AND valid SNTP)
  bool systemHealthy = true;
  DisplayState display = DisplayState::WaitingForData;
  uint8_t brightness = 0;
  bool hasRouteMask = false;
  uint32_t routeVisibilityMask = 0;
  PollState poll = PollState::Idle;
  uint8_t feedAttempted = 0;
  uint8_t feedSuccess = 0;
  bool hasFeedMask = false;
  uint8_t feedMask = 0;
  bool hasCompleteAge = false;
  uint32_t completeAgeSeconds = 0;
  uint32_t vehiclesProcessed = 0;
  uint32_t vehiclesResolved = 0;
  uint16_t occupiedMapLeds = 0;
  bool hasCollisions = false;
  uint32_t collisionCount = 0;
  bool physicalOutputEnabled = false;
};

// Call before networkService.begin().
void begin();
// Call every loop() iteration; copies the snapshot under a short mutex.
void update(const Snapshot &snapshot);
// Call when the operational display changed state.
void noteDisplayTransition(DisplayState previous, DisplayState current);
void noteSettingsSaved(const char *codeDefinedGroup);
void noteUpdateStatus(bool automatic,uint32_t intervalMs,bool attempted,bool success,
                      uint32_t checkAgeMs,uint32_t nextMs,bool available,const char *version,uint8_t phase);
void noteFeedError(uint8_t feed,uint8_t category,int32_t http,int32_t tls);
void noteInstallPhase(uint8_t phase);
void planRestart(RestartIntent intent);
// A bounded best-effort send, without opening a connection just for this report.
void flushDiagnostics(uint32_t maximumMs=350);
// XIAO serial bench command hook; returns true if the character was consumed.
bool handleCommand(int command);
monitor::Settings monitoringSettings();
monitor::Runtime monitoringStatus();
bool configureMonitoring(const monitor::Settings &);
bool clearMonitoring();
// Waits a bounded interval for the sender to relinquish its TLS connection.
bool pauseForUpdate();
void resumeAfterUpdate();
struct UpdatePause {
  bool ready;
  UpdatePause():ready(pauseForUpdate()){}
  ~UpdatePause(){resumeAfterUpdate();}
};

}  // namespace brooklyn::telemetry
