// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "operational_display.h"
#include "feed_transport.h"
#include "network_diagnostics.h"
#include "normal_config.h"
#include "blue_diagnostics.h"
#include "oled_clock.h"
#include "runtime_observation.h"
#include "personalization.h"

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::web {

// Includes signed-in controls, all route diagnostics and escaped status text.
constexpr size_t kStatusPageCapacity = 28672;
size_t renderPersonalizationPage(char *out,size_t capacity,const personal::Settings &settings,const char *csrf);

constexpr const char *kDefaultDeviceName = "BrooklynMap";
constexpr const char *kHostname = "brooklynmap";
constexpr const char *kMdnsUrl = "http://brooklynmap.local/";
constexpr const char *kFirmwareId = "Phase 4W";

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
  char lastSound[128]="No sound recorded this boot";
  uint32_t uptimeSeconds = 0;
  pipeline::BlueReport blueReport{};
  semantic::TrainColors trainColors=semantic::kDefaultTrainColors;
  uint32_t unknownTiming = 511;
  uint8_t oledContrast = 207;
  uint16_t oledReturnSeconds = config::kDefaultOledReturnSeconds;
  uint64_t screenPreferences = config::kDefaultScreenPreferences;
  bool quietHoursActive = false;
  uint32_t blueHoldSeconds = 20;
  uint32_t pollSeconds = 15;
  uint8_t oledAddress = config::kDefaultOledAddress;
  oled::Status oledStatus{};
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
  bool adminConfigured = false;
  bool adminSessionActive = false;
  char firmwareVersion[16] = {};
  bool otaBootConfirmed = false;
  bool physicalLiveAvailable = false;
  bool physicalLedOutputEnabled = false;
  bool physicalPowerPaused = false;
  bool startupIndicatorsActive=false;
  bool routeFocusActive=false;
  semantic::Route focusedRoute=semantic::Route::None;
  uint32_t physicalFramesWritten = 0;
  uint32_t physicalMapChanges = 0;
  uint32_t physicalFrameAgeSeconds = 0;
  uint16_t physicalBlinkingLeds = 0;
  uint16_t visibleMapLeds = 0;
  uint16_t darkOccupiedMapLeds = 0;
  display::IndicationCounts indications{};
  uint32_t routeVisibilityMask = semantic::kAllRoutesVisibleMask;
  char availableVersion[33] = {};  // empty when no newer release is known
  int16_t rssi = -127;
  bool mdnsAvailable = false;
  bool timeSynchronized = false;
  uint32_t unixTime = 0;
  display::State displayState = display::State::WaitingForData;
  bool fetchingFeeds = false;
  bool networkBusy = false; // Shared background worker, including weather TLS.
  bool hasCycle = false;
  transport::CycleOutcome cycleOutcome = transport::CycleOutcome::NoFeeds;
  uint16_t processedFeedMask = 0;
  uint8_t feedsSucceeded = 0;
  uint8_t feedsTotal = 0;
  bool hasDataAge = false;
  uint32_t dataAgeSeconds = 0;
  bool hasCompleteAge = false;
  uint32_t completeAgeSeconds = 0;
  uint32_t resolvedTrains = 0;
  uint32_t approachingTrains = 0;
  uint16_t occupiedLeds = 0;
  uint16_t collisionCount = 0;
  uint32_t freeInternalHeap = 0;
  uint32_t largestInternalBlock=0;
  observation::BootInfo boot{};
  uint64_t deviceId=0;
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

inline bool otaTransportBusy(const StatusSnapshot &status) {return status.fetchingFeeds || status.networkBusy;}
inline uint32_t effectiveRouteMask(const StatusSnapshot &status) {
  return status.routeFocusActive?semantic::routeVisibilityBit(status.focusedRoute):status.routeVisibilityMask;
}
size_t renderRuntimeJson(char *out, size_t capacity, const observation::RuntimeReport &report);

bool parseIndicationCursor(const char *text, uint32_t &after);
size_t renderIndicationJson(char *out, size_t capacity, const display::IndicationReport &report);

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
                      uint32_t routeVisibilityMask,
                      const char *csrfToken = nullptr);
const char *loginReturnPath(const char *target);
const char *loginTargetForRequest(const char *path);
size_t renderDefaultsPage(char *out,size_t capacity,const char *csrf);
size_t renderHelpPage(char *out,size_t capacity,const char *version,const char *target);
size_t renderScreenOptionsPage(char *out,size_t capacity,uint64_t preferences,bool quietNow,const char *csrf);
size_t renderLoginPage(char *out, size_t capacity, const char *csrfToken, const char *target = "");
size_t renderTrainMonitorPage(char *out, size_t capacity, const StatusSnapshot &status);
size_t renderTrainMonitorJson(char *out, size_t capacity, const StatusSnapshot &status);
size_t renderStatusJson(char *out, size_t capacity,
                        const StatusSnapshot &status, uint8_t brightness,
                        uint32_t routeVisibilityMask);
size_t renderNetworkDiagnosticsJson(
    char *out, size_t capacity,
    const network::diagnostics::Snapshot &diagnostics);

}  // namespace brooklyn::web
