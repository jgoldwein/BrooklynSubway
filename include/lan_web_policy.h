// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "operational_display.h"
#include "feed_transport.h"
#include "network_diagnostics.h"

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::web {

constexpr const char *kDefaultDeviceName = "BrooklynMap";
constexpr const char *kHostname = "brooklynmap";
constexpr const char *kMdnsUrl = "http://brooklynmap.local/";
constexpr const char *kFirmwareId = "Phase 4U";

enum class ServerMode : uint8_t { Stopped, Provisioning, NormalLan };

class ServerOwnership {
 public:
  bool enterProvisioning();
  bool enterNormalLan();
  void stop() { mode_ = ServerMode::Stopped; }
  ServerMode mode() const { return mode_; }

 private:
  ServerMode mode_ = ServerMode::Stopped;
};

struct StatusSnapshot {
  uint32_t uptimeSeconds = 0;
  char deviceName[33] = {};
  char target[48] = {};
  char resetReason[24] = {};
  bool networkConnected = false;
  char ssid[33] = {};
  char ip[16] = {};
  char subnet[16] = {};
  char gateway[16] = {};
  char dnsPrimary[16] = {};
  char dnsSecondary[16] = {};
  bool manualDnsEnabled = false;
  bool manualDnsApplied = false;
  char manualDnsServer[16] = {};
  char effectiveDnsServer[16] = {};
  char effectiveDnsSource[12] = "Unavailable";
  char dnsConfigStatus[16] = "disabled";
  int16_t rssi = -127;
  bool mdnsAvailable = false;
  bool timeSynchronized = false;
  uint32_t unixTime = 0;
  display::State displayState = display::State::WaitingForData;
  bool hasCycle = false;
  transport::CycleOutcome cycleOutcome = transport::CycleOutcome::NoFeeds;
  uint8_t feedsSucceeded = 0;
  uint8_t feedsTotal = 0;
  bool hasDataAge = false;
  uint32_t dataAgeSeconds = 0;
  bool hasCompleteAge = false;
  uint32_t completeAgeSeconds = 0;
  uint32_t resolvedTrains = 0;
  uint16_t occupiedLeds = 0;
  uint16_t collisionCount = 0;
  uint32_t freeInternalHeap = 0;
  uint32_t minimumInternalHeap = 0;
  uint32_t freePsram = 0;
  uint32_t minimumFreePsram = 0;
  bool psramPresent = false;
  bool psramFeedBufferPresent = false;
  transport::TransportCounters transportCounters{};
  bool hasLastFeedFailure = false;
  char lastFeedFailureName[8] = {};
  char lastFeedFailureCategory[32] = {};
  uint32_t lastFeedFailureBytes = 0;
  uint32_t lastFeedFailureElapsedMs = 0;
  network::diagnostics::Snapshot networkDiagnostics{};
};

bool parseBrightness(const char *text, uint8_t &brightness);
struct DnsSettingsParam {
  const char *name;
  const char *value;
};
bool parseDnsSettings(const DnsSettingsParam *parameters, size_t count,
                      bool &enabled, uint32_t &ipv4);
struct RouteSelectionParam {
  const char *name;
  const char *value;
};
bool parseRouteSelection(const RouteSelectionParam *parameters, size_t count,
                         uint32_t &visibilityMask);
size_t renderHomePage(char *out, size_t capacity,
                      const StatusSnapshot &status, uint8_t brightness,
                      uint32_t routeVisibilityMask);
size_t renderStatusJson(char *out, size_t capacity,
                        const StatusSnapshot &status, uint8_t brightness,
                        uint32_t routeVisibilityMask);
size_t renderNetworkDiagnosticsJson(
    char *out, size_t capacity,
    const network::diagnostics::Snapshot &diagnostics);

}  // namespace brooklyn::web
