// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA production telemetry service (publish-only MQTT). ESP32 adapter around the
// host-tested brooklyn::telemetry::Core.
//
// Live telemetry is compiled in ONLY when BROOKLYN_TARGET_XIAO, BROOKLYN_NETWORK_ENABLED
// and the git-ignored include/secrets.h defines BROOKLYN_MQTT_BENCH_PASSWORD. Every other
// build gets empty no-ops: no esp-mqtt client, no bench credential, no tasks.
//
// The service owns a dedicated FreeRTOS task. Heartbeats, reconnects and event drains are
// driven by that task's own clock and never by loop(), the MTA cycle, or SNTP. loop()
// only hands over a small snapshot (and display transitions) through a short mutex.
// Never touches LEDs, GPIO, LED_ENABLE or any hardware output.

#pragma once

#include <stdint.h>

#include "telemetry_core.h"

namespace brooklyn::telemetry {

// Latest stable application state, published by loop() each iteration.
struct Snapshot {
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
};

// Call before networkService.begin().
void begin();
// Call every loop() iteration; copies the snapshot under a short mutex.
void update(const Snapshot &snapshot);
// Call when the operational display changed state.
void noteDisplayTransition(DisplayState previous, DisplayState current);
// XIAO serial bench command hook; returns true if the character was consumed.
bool handleCommand(int command);

}  // namespace brooklyn::telemetry
