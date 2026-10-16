// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4L: ESP32 Wi-Fi credential storage (NVS) and captive provisioning portal.
//
// Compiled only when BROOKLYN_NETWORK_ENABLED=1. Never touches LEDs, GPIO or
// LED_ENABLE. Credentials are never printed or rendered.
//
// SECURITY NOTE: Preferences/NVS is NOT encrypted storage unless the device is
// provisioned with NVS encryption + flash encryption (not done in Phase 4L).

#pragma once

#include "network_config.h"
#include "wifi_policy.h"

#if BROOKLYN_NETWORK_ENABLED

#include <DNSServer.h>
#include <WebServer.h>

namespace brooklyn::wifi {

constexpr const char *kApSsid = "BrooklynMap-Avi";  // setup AP contract
constexpr uint32_t kConnectAttemptMs = 20000;       // one portal association attempt
constexpr uint32_t kSavedLingerMs = 8000;           // let the "saved" page reach the phone
constexpr uint32_t kStoredPortalTimeoutMs = 10UL * 60UL * 1000UL;  // only when creds exist

// NVS namespace "bkmap_wifi" (versioned, map-owned). clear() empties only it.
class NvsCredentialStore : public CredentialStore {
 public:
  bool load(Credentials &out) override;
  bool save(const Credentials &in) override;
  bool clear() override;
  bool exists();
};

class Portal {
 public:
  enum class Result : uint8_t { Active, Connected, TimedOut };

  explicit Portal(CredentialStore &store);
  // Starts the open AP at 192.168.4.1, wildcard DNS and the web server.
  bool begin(bool hasStored, uint32_t nowMs);
  void stop();
  bool active() const { return active_; }
  // Call from loop(); non-blocking.
  Result service(uint32_t nowMs);
  // Valid after Result::Connected (the credentials that were just stored).
  const Credentials &connected() const { return candidate_; }

 private:
  void handleRoot();
  void handleScan();
  void handleConnect();
  void handleOther();
  void startScan();
  void redirectHome();
  void sendPage(PortalStatus status);
  void harvestScan();

  CredentialStore &store_;
  DNSServer dns_;
  WebServer server_;
  bool active_ = false;
  bool hasStored_ = false;
  bool scanPending_ = false;
  bool connecting_ = false;
  bool saved_ = false;
  PortalStatus lastStatus_ = PortalStatus::None;
  uint32_t startedMs_ = 0;
  uint32_t connectStartMs_ = 0;
  uint32_t savedAtMs_ = 0;
  Credentials candidate_{};
  char connectedIp_[16] = {};
  ScanEntry scan_[kMaxScanEntries];
  size_t scanCount_ = 0;
  char page_[6144];
};

}  // namespace brooklyn::wifi

#endif  // BROOKLYN_NETWORK_ENABLED
