// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA RESOURCE GATE (bench only): esp-mqtt + verified TLS next to the MTA
// HTTPS pipeline on the XIAO ESP32-S3. This is a measurement harness, not the
// production TelemetryService.
//
// Active ONLY when ALL are true: BROOKLYN_TARGET_XIAO, BROOKLYN_NETWORK_ENABLED,
// and the git-ignored include/secrets.h defines BROOKLYN_MQTT_BENCH_PASSWORD.
// Otherwise every function below is an empty no-op and esp-mqtt is not linked in.
// Publish-only: no subscriptions, no inbound application message path. Never
// touches LEDs, GPIO or LED_ENABLE.

#pragma once

#include <stdint.h>

namespace brooklyn::telemetry_bench {

// Call before networkService.begin() (checkpoint A).
void begin();
// Call every loop() before / after networkService.update(). `networkReady` is
// network::State::Ready (Wi-Fi up AND valid SNTP); `stateCode` is the State value.
void beforeNetworkUpdate();
void afterNetworkUpdate(uint32_t nowMs, bool networkReady, uint8_t stateCode);
// Call when networkService.consumeCompletedCycle() returned true.
void noteCompletedCycle(uint8_t outcome, uint8_t attempted, uint8_t processed);
// Serial command hook; returns true if the character was consumed.
bool handleCommand(int command);

}  // namespace brooklyn::telemetry_bench
