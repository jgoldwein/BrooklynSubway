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
#include "normal_config.h"

#if BROOKLYN_NETWORK_ENABLED
#include "lan_web_policy.h"
#include "lan_web_ui.h"
#endif

#include <stdint.h>

namespace brooklyn::network {

enum class State : uint8_t {
  Disabled,        // compile-time/config disabled
  NoCredentials,   // enabled, no credentials anywhere, portal not (yet) running
  Provisioning,    // setup AP + captive portal active; poller not running
  WifiConnecting,  // credentials exist; associating
  TemporarilyUnavailable,  // credentials exist; SSID not visible (keeps retrying)
  TimeSyncing,     // associated, waiting for SNTP
  Ready,           // online: polling
};

class NetworkService {
 public:
  explicit NetworkService(pipeline::TrainPipeline &pipeline);
  // Association retries are asynchronous; a due multi-feed poll runs synchronously.
  void begin();
  // Call from loop(). Returns immediately when disabled.
  void update(uint32_t nowMs);
  // Returns each finished transport cycle once; skipped cycles leave the pipeline untouched.
  bool consumeCompletedCycle(transport::CycleResult &result, uint32_t &completedAtMs);

  // Explicit user recovery: enter the setup portal. Credentials are kept.
  void requestProvisioning();
  // Explicit user action: erase the map's own stored credentials (NVS namespace only).
  bool forgetCredentials();
  uint8_t brightness() const;
  uint32_t routeVisibilityMask() const;
#if BROOKLYN_NETWORK_ENABLED
  void setWebStatus(const web::StatusSnapshot &status);
#endif

  State state() const { return state_; }
  const transport::TransportCounters &counters() const { return counters_; }
  size_t bufferCapacity() const { return buffer_.capacity; }

 private:
  bool credentialsPresent() const;
  void loadCredentials();
  void startProvisioning(uint32_t nowMs);
  void serviceProvisioning(uint32_t nowMs);
  void allocateBuffer();
  void serviceWifi(uint32_t nowMs);
  void serviceTime(uint32_t nowMs);
  void pollNow(uint32_t nowMs);
  void printStatus(const transport::CycleResult &result) const;
#if BROOKLYN_NETWORK_ENABLED
  void serviceLanWebUi(uint32_t nowMs);
  void serviceNetworkDiagnostics(uint32_t nowMs);
  void stopLanWebUi();
#endif

  pipeline::TrainPipeline &pipeline_;
  transport::TransportCounters counters_;
  transport::FeedBuffer buffer_;
  transport::PollScheduler scheduler_;
  State state_;
  bool bufferTried_;
  bool wifiStarted_;
  bool timeStarted_;
  bool recoveryRequested_;
  uint32_t lastWifiAttemptMs_;
  uint32_t timeStartMs_;
  transport::CycleResult completedCycle_{};
  uint32_t completedCycleAtMs_ = 0;
  bool completedCyclePending_ = false;
#if BROOKLYN_NETWORK_ENABLED
  config::DeviceConfiguration configuration_;
  web::LanWebUi lanWebUi_;
  web::ServerOwnership serverOwnership_;
  web::StatusSnapshot webStatus_{};
  diagnostics::Runner networkDiagnostics_{};
  transport::CycleResult latestCycle_{};
  bool hasLatestCycle_ = false;
  bool mdnsAttempted_ = false;
  bool mdnsStarted_ = false;
#endif
};

}  // namespace brooklyn::network
