// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4J: ESP32 Wi-Fi / SNTP / HTTPS GTFS-RT service.
//
// OFF by default (BROOKLYN_NETWORK_ENABLED=0): begin() and update() then do
// nothing - no Wi-Fi association, no SNTP, no PSRAM allocation, no sockets.
// This layer never touches LEDs, GPIO or LED_ENABLE.
//
// NOT validated on hardware: the ESP32-S3 was not connected during Phase 4J.

#pragma once

#include "feed_transport.h"
#include "network_config.h"
#include "train_pipeline.h"

#include <stdint.h>

namespace brooklyn::network {

enum class State : uint8_t {
  Disabled,        // compile-time/config disabled
  NoCredentials,   // enabled but secrets.h missing or SSID empty
  WifiConnecting,
  TimeSyncing,
  Ready,
};

class NetworkService {
 public:
  explicit NetworkService(pipeline::TrainPipeline &pipeline);
  // Never blocks on the network. Prints the network status line.
  void begin();
  // Call from loop(). Returns immediately when disabled.
  void update(uint32_t nowMs);

  State state() const { return state_; }
  const transport::TransportCounters &counters() const { return counters_; }
  size_t bufferCapacity() const { return buffer_.capacity; }

 private:
  bool credentialsPresent() const;
  void allocateBuffer();
  void serviceWifi(uint32_t nowMs);
  void serviceTime(uint32_t nowMs);
  void pollNow(uint32_t nowMs);
  void printStatus(const transport::CycleResult &result) const;

  pipeline::TrainPipeline &pipeline_;
  transport::TransportCounters counters_;
  transport::FeedBuffer buffer_;
  transport::PollScheduler scheduler_;
  State state_;
  bool bufferTried_;
  bool wifiStarted_;
  bool timeStarted_;
  uint32_t lastWifiAttemptMs_;
  uint32_t timeStartMs_;
};

}  // namespace brooklyn::network
