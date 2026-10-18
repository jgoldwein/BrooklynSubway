// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4J ESP32 network layer. See network_service.h.
// Not validated on hardware (ESP32-S3 not connected during Phase 4J).

#include "network_service.h"

#include "diagnostics.h"
#include "wifi_policy.h"

#include <Arduino.h>

#include <atomic>

#if BROOKLYN_NETWORK_ENABLED
#include "wifi_portal.h"

#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <soc/soc_memory_types.h>
#include <lwip/dns.h>
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Embedded Mozilla-derived CA bundle shipped with the Arduino-ESP32 framework
// (includes Amazon Root CA 1, which anchors api-endpoint.mta.info). The server
// certificate is always validated; setInsecure() is never used.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
#endif

namespace brooklyn::network {

#if BROOKLYN_NETWORK_ENABLED

namespace {

// Developer-only fallback (include/secrets.h, git-ignored). Used ONLY when no
// credentials are stored in NVS; never copied into NVS, never overrides them.
#ifdef BROOKLYN_WIFI_SSID
constexpr const char *kSsid = BROOKLYN_WIFI_SSID;
#else
constexpr const char *kSsid = "";
#endif
#ifdef BROOKLYN_WIFI_PASSWORD
constexpr const char *kPassword = BROOKLYN_WIFI_PASSWORD;
#else
constexpr const char *kPassword = "";
#endif

wifi::Credentials gActive{};  // credentials currently used for station mode
bool gActiveFromNvs = false;

wifi::NvsCredentialStore &store() {
  static wifi::NvsCredentialStore instance;
  return instance;
}

wifi::Portal &portal() {
  static wifi::Portal instance(store());
  return instance;
}

// Logs feed failures (name, category, bytes, elapsed). Diagnostic only.
class FailureLogger : public transport::FeedFailureObserver {
 public:
  explicit FailureLogger(web::StatusSnapshot &status) : status_(status) {}

  void onFeedFailure(const transport::FeedFailure &failure) override {
    char line[112];
    transport::formatFeedFailure(line, sizeof(line), failure);
    Serial.println(line);
    status_.hasLastFeedFailure = true;
    snprintf(status_.lastFeedFailureName, sizeof(status_.lastFeedFailureName),
             "%s", failure.name);
    snprintf(status_.lastFeedFailureCategory,
             sizeof(status_.lastFeedFailureCategory), "%s",
             transport::downloadStatusName(failure.status));
    status_.lastFeedFailureBytes =
        failure.bytesReceived > UINT32_MAX
            ? UINT32_MAX
            : static_cast<uint32_t>(failure.bytesReceived);
    status_.lastFeedFailureElapsedMs = failure.elapsedMs;
  }

 private:
  web::StatusSnapshot &status_;
};

constexpr uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }

struct DnsProbeContext {
  std::atomic<bool> pending{false};
  std::atomic<bool> complete{false};
  std::atomic<bool> succeeded{false};
  std::atomic<uint32_t> address{0};
  uint32_t startedAtMs = 0;
};

void dnsProbeCallback(const char *, const ip_addr_t *address, void *context) {
  auto *probe = static_cast<DnsProbeContext *>(context);
  if (address != nullptr && IP_IS_V4(address)) {
    const uint32_t resolvedAddress = address->u_addr.ip4.addr;
    probe->address.store(resolvedAddress, std::memory_order_relaxed);
    probe->succeeded.store(resolvedAddress != 0, std::memory_order_relaxed);
  } else {
    probe->succeeded.store(false, std::memory_order_relaxed);
  }
  probe->complete.store(true, std::memory_order_release);
  probe->pending.store(false, std::memory_order_release);
}

class EspNetworkDiagnosticProbe : public diagnostics::Probe {
 public:
  void capture(diagnostics::Snapshot &snapshot) override {
    snapshot.wifiAssociated = WiFi.status() == WL_CONNECTED;
    if (!snapshot.wifiAssociated) return;
    copyString(snapshot.ssid, sizeof(snapshot.ssid), WiFi.SSID());
    copyAddress(snapshot.ip, sizeof(snapshot.ip), WiFi.localIP());
    copyAddress(snapshot.subnet, sizeof(snapshot.subnet), WiFi.subnetMask());
    copyAddress(snapshot.gateway, sizeof(snapshot.gateway), WiFi.gatewayIP());
    copyAddress(snapshot.dnsPrimary, sizeof(snapshot.dnsPrimary), WiFi.dnsIP(0));
    copyAddress(snapshot.dnsSecondary, sizeof(snapshot.dnsSecondary),
                WiFi.dnsIP(1));
    const uint32_t unixTime = static_cast<uint32_t>(time(nullptr));
    if (unixTime >= kMinValidUnixTime) {
      snapshot.unixTime = unixTime;
      snapshot.hasUnixTime = true;
    }
  }

  void startDns(diagnostics::DnsTarget target, const char *hostname,
                uint32_t, uint32_t nowMs) override {
    DnsProbeContext &context = dnsContext(target);
    CheckResultState &result = immediateState(target);
    if (context.pending.load(std::memory_order_acquire)) {
      result = CheckResultState::Busy;
      return;
    }
    context.complete.store(false, std::memory_order_relaxed);
    context.succeeded.store(false, std::memory_order_relaxed);
    context.address.store(0, std::memory_order_relaxed);
    context.startedAtMs = nowMs;
    context.pending.store(true, std::memory_order_release);
    result = CheckResultState::Pending;

    ip_addr_t address{};
    const err_t error =
        dns_gethostbyname(hostname, &address, dnsProbeCallback, &context);
    if (error == ERR_OK) {
      const uint32_t resolvedAddress =
          IP_IS_V4(&address) ? address.u_addr.ip4.addr : 0;
      context.address.store(resolvedAddress, std::memory_order_relaxed);
      context.succeeded.store(resolvedAddress != 0, std::memory_order_relaxed);
      context.complete.store(true, std::memory_order_release);
      context.pending.store(false, std::memory_order_release);
    } else if (error != ERR_INPROGRESS) {
      context.succeeded.store(false, std::memory_order_relaxed);
      context.complete.store(true, std::memory_order_release);
      context.pending.store(false, std::memory_order_release);
    }
  }

  diagnostics::CheckState pollDns(diagnostics::DnsTarget target,
                                  uint32_t nowMs,
                                  diagnostics::CheckResult &result) override {
    const CheckResultState immediate = immediateState(target);
    if (immediate == CheckResultState::Busy) {
      result.elapsedMs = 0;
      return diagnostics::CheckState::Busy;
    }
    DnsProbeContext &context = dnsContext(target);
    if (context.complete.load(std::memory_order_acquire)) {
      result.elapsedMs = elapsed(nowMs, context.startedAtMs);
      result.address[0] = '\0';
      if (context.succeeded.load(std::memory_order_relaxed)) {
        const IPAddress address(
            context.address.load(std::memory_order_relaxed));
        copyAddress(result.address, sizeof(result.address), address);
      }
      context.complete.store(false, std::memory_order_relaxed);
      return context.succeeded.load(std::memory_order_relaxed)
                 ? diagnostics::CheckState::Succeeded
                 : diagnostics::CheckState::Failed;
    }
    if (!context.pending.load(std::memory_order_acquire)) {
      result.elapsedMs = 0;
      return diagnostics::CheckState::Failed;
    }
    if (elapsed(nowMs, context.startedAtMs) >= diagnostics::kDnsTimeoutMs) {
      result.elapsedMs = elapsed(nowMs, context.startedAtMs);
      return diagnostics::CheckState::TimedOut;
    }
    return diagnostics::CheckState::Pending;
  }

  diagnostics::CheckState connectTcp(const char *ipv4, uint16_t port,
                                     uint32_t timeoutMs, uint32_t,
                                     uint32_t &elapsedMs) override {
    IPAddress address;
    if (!address.fromString(ipv4)) {
      elapsedMs = 0;
      return diagnostics::CheckState::Failed;
    }
    const uint32_t startedAtMs = millis();
    WiFiClient client;
    const bool connected =
        client.connect(address, port, static_cast<int32_t>(timeoutMs)) == 1;
    elapsedMs = elapsed(millis(), startedAtMs);
    client.stop();
    return connected ? diagnostics::CheckState::Succeeded
                     : diagnostics::CheckState::Failed;
  }

 private:
  enum class CheckResultState : uint8_t { Pending, Busy, Available };

  static void copyString(char *destination, size_t capacity,
                         const String &value) {
    value.toCharArray(destination, capacity);
  }

  static void copyAddress(char *destination, size_t capacity,
                          const IPAddress &address) {
    const String text = address.toString();
    text.toCharArray(destination, capacity);
  }

  DnsProbeContext &dnsContext(diagnostics::DnsTarget target) {
    return target == diagnostics::DnsTarget::Ntp ? ntpDns_ : mtaDns_;
  }

  CheckResultState &immediateState(diagnostics::DnsTarget target) {
    return target == diagnostics::DnsTarget::Ntp ? ntpImmediate_
                                                  : mtaImmediate_;
  }

  DnsProbeContext ntpDns_{};
  DnsProbeContext mtaDns_{};
  CheckResultState ntpImmediate_ = CheckResultState::Available;
  CheckResultState mtaImmediate_ = CheckResultState::Available;
};

EspNetworkDiagnosticProbe &diagnosticProbe() {
  static EspNetworkDiagnosticProbe instance;
  return instance;
}

class HttpsFeedConnection : public transport::FeedConnection {
 public:
  explicit HttpsFeedConnection(const NetworkConfig &config) : config_(config) {}

  transport::OpenResult open(const char *url, int32_t &contentLength) override {
    contentLength = transport::kUnknownLength;
    if (WiFi.status() != WL_CONNECTED) return transport::OpenResult::WifiUnavailable;

    client_.setCACertBundle(rootca_crt_bundle_start);
    client_.setHandshakeTimeout((config_.connectTimeoutMs + 999) / 1000);
    http_.setReuse(false);       // one connection per feed
    http_.useHTTP10(true);       // no chunked bodies: completeness stays provable
    http_.setConnectTimeout(static_cast<int32_t>(config_.connectTimeoutMs));
    http_.setTimeout(static_cast<uint16_t>(config_.readIdleTimeoutMs));
    if (!http_.begin(client_, url)) return transport::OpenResult::HttpFailure;
    active_ = true;
    startedMs_ = millis();

    const int code = http_.GET();
    if (code < 0) {
      if (WiFi.status() != WL_CONNECTED) return transport::OpenResult::WifiUnavailable;
      if (code == HTTPC_ERROR_READ_TIMEOUT) return transport::OpenResult::Timeout;
      // HTTPClient returns only a generic negative code, so TCP connect failures
      // and TLS handshake/certificate failures cannot be told apart here; both
      // land in the secure-connection bucket. No error-string parsing.
      return transport::OpenResult::SecureConnectFailure;
    }
    if (code != HTTP_CODE_OK) return transport::OpenResult::HttpFailure;
    contentLength = http_.getSize();  // -1 when absent
    stream_ = http_.getStreamPtr();
    return stream_ == nullptr ? transport::OpenResult::HttpFailure : transport::OpenResult::Ok;
  }

  int32_t read(uint8_t *dest, size_t maxLen) override {
    if (stream_ == nullptr) return transport::kReadError;
    const uint32_t waitStart = millis();
    for (;;) {
      const int avail = stream_->available();
      if (avail > 0) {
        const size_t n = static_cast<size_t>(avail) < maxLen ? static_cast<size_t>(avail) : maxLen;
        const size_t got = stream_->readBytes(dest, n);
        if (got > 0) return static_cast<int32_t>(got);
      } else if (!stream_->connected()) {
        return transport::kReadEof;
      }
      const uint32_t now = millis();
      if (elapsed(now, waitStart) >= config_.readIdleTimeoutMs ||
          elapsed(now, startedMs_) >= config_.feedDeadlineMs) {
        return transport::kReadTimeout;
      }
      delay(1);
    }
  }

  void close() override {
    if (active_) http_.end();
    client_.stop();
    stream_ = nullptr;
    active_ = false;
  }

 private:
  const NetworkConfig &config_;
  WiFiClientSecure client_;
  HTTPClient http_;
  WiFiClient *stream_ = nullptr;
  bool active_ = false;
  uint32_t startedMs_ = 0;
};

class MillisClock : public transport::Clock {
 public:
  uint32_t nowMs() const override { return millis(); }
};

HttpsFeedConnection &connection() {
  static HttpsFeedConnection instance(kDefaultNetworkConfig);
  return instance;
}

const char *stateName(State s) {
  switch (s) {
    case State::Disabled: return "DISABLED";
    case State::NoCredentials: return "NO CREDENTIALS";
    case State::Provisioning: return "PROVISIONING (setup AP BrooklynMap-Avi)";
    case State::WifiConnecting: return "WIFI CONNECTING";
    case State::TemporarilyUnavailable: return "WIFI TEMPORARILY UNAVAILABLE (retrying)";
    case State::TimeSyncing: return "TIME SYNCING";
    case State::Ready: return "READY";
  }
  return "?";
}

}  // namespace

NetworkService::NetworkService(pipeline::TrainPipeline &pipeline)
    : pipeline_(pipeline), counters_{}, buffer_{nullptr, 0}, scheduler_{},
      state_(State::Disabled), bufferTried_(false), wifiStarted_(false),
      timeStarted_(false), recoveryRequested_(false), lastWifiAttemptMs_(0), timeStartMs_(0),
      completedCycle_{}, completedCycleAtMs_(0), completedCyclePending_(false),
      configuration_(config::nvsStore()),
      lanWebUi_(configuration_), serverOwnership_{}, webStatus_{}, latestCycle_{},
      hasLatestCycle_(false), mdnsAttempted_(false), mdnsStarted_(false) {}

uint8_t NetworkService::brightness() const {
  return configuration_.brightness();
}

uint32_t NetworkService::routeVisibilityMask() const {
  return configuration_.routeVisibilityMask();
}

void NetworkService::setWebStatus(const web::StatusSnapshot &status) {
  webStatus_.displayState = status.displayState;
  webStatus_.hasDataAge = status.hasDataAge;
  webStatus_.dataAgeSeconds = status.dataAgeSeconds;
  webStatus_.hasCompleteAge = status.hasCompleteAge;
  webStatus_.completeAgeSeconds = status.completeAgeSeconds;
  webStatus_.resolvedTrains = status.resolvedTrains;
  webStatus_.occupiedLeds = status.occupiedLeds;
  webStatus_.collisionCount = status.collisionCount;
}

bool NetworkService::consumeCompletedCycle(transport::CycleResult &result,
                                           uint32_t &completedAtMs) {
  if (!completedCyclePending_) return false;
  result = completedCycle_;
  completedAtMs = completedCycleAtMs_;
  completedCyclePending_ = false;
  return true;
}

bool NetworkService::credentialsPresent() const { return gActive.ssid[0] != '\0'; }

void NetworkService::loadCredentials() {
  memset(&gActive, 0, sizeof(gActive));
  gActiveFromNvs = store().load(gActive);
  if (!gActiveFromNvs) {
    memset(&gActive, 0, sizeof(gActive));
    if (kSsid[0] != '\0' &&
        wifi::validateCredentials(kSsid, kPassword) == wifi::CredentialCheck::Ok) {
      strncpy(gActive.ssid, kSsid, wifi::kMaxSsidLen);
      strncpy(gActive.password, kPassword, wifi::kMaxPassLen);
    }
  }
}

void NetworkService::begin() {
  if (!kDefaultNetworkConfig.enabled) {
    state_ = State::Disabled;
    Serial.printf("Network: %s\n", stateName(state_));
    return;
  }
  const bool savedConfigurationLoaded = configuration_.load();
  strncpy(webStatus_.deviceName, web::kDefaultDeviceName,
          sizeof(webStatus_.deviceName) - 1);
  strncpy(webStatus_.resetReason, brooklyn::diagnostics::resetReasonName(),
          sizeof(webStatus_.resetReason) - 1);
#if defined(BROOKLYN_TARGET_PCB)
  strncpy(webStatus_.target, "Brooklyn PCB / ESP32-S3-WROOM-1U-N16R8",
          sizeof(webStatus_.target) - 1);
#else
  strncpy(webStatus_.target, "XIAO ESP32-S3 bench target",
          sizeof(webStatus_.target) - 1);
#endif
  Serial.printf("LAN UI: config namespace %s; brightness %u/255; "
                "route mask 0x%05lx (%s); config state %u B, web service state %u B\n",
                config::kNvsNamespace, configuration_.brightness(),
                static_cast<unsigned long>(
                    configuration_.routeVisibilityMask()),
                savedConfigurationLoaded ? "NVS" : "default",
                static_cast<unsigned>(sizeof(configuration_)),
                static_cast<unsigned>(sizeof(lanWebUi_)));
  WiFi.persistent(false);  // the Wi-Fi driver must not keep its own copy of the password
  if (!WiFi.setHostname(web::kHostname)) {
    Serial.println("LAN UI: station hostname configuration failed");
  }
  loadCredentials();
  scheduler_.configure(kDefaultNetworkConfig.pollIntervalMs);
  const bool fallback = !gActiveFromNvs && credentialsPresent();
  Serial.printf("Network: credentials source: %s\n",
                gActiveFromNvs ? "NVS" : (fallback ? "secrets.h developer fallback" : "none"));
  if (wifi::decideBootMode(gActiveFromNvs, fallback, false) == wifi::BootMode::Provisioning) {
    startProvisioning(millis());
  } else {
    state_ = State::WifiConnecting;
    allocateBuffer();
  }
  Serial.printf("Network: %s\n", stateName(state_));
}

void NetworkService::startProvisioning(uint32_t nowMs) {
  stopLanWebUi();
  serverOwnership_.stop();
  if (!serverOwnership_.enterProvisioning()) {
    Serial.println("Provisioning: refused while LAN HTTP server owns port 80");
    state_ = State::NoCredentials;
    return;
  }
  if (portal().begin(gActiveFromNvs, nowMs)) {
    state_ = State::Provisioning;
    Serial.println("Provisioning: setup AP \"BrooklynMap-Avi\" at http://192.168.4.1/ (2.4 GHz only)");
  } else {
    serverOwnership_.stop();
    state_ = State::NoCredentials;
    Serial.println("Provisioning: FAILED to start the setup AP");
  }
}

void NetworkService::serviceProvisioning(uint32_t nowMs) {
  const wifi::Portal::Result r = portal().service(nowMs);
  if (r == wifi::Portal::Result::Active) return;
  serverOwnership_.stop();
  loadCredentials();
  wifiStarted_ = false;
  timeStarted_ = false;
  if (r == wifi::Portal::Result::Connected) {
    Serial.println("Provisioning: complete; AP stopped, resuming station mode");
    if (!bufferTried_) allocateBuffer();
    // The portal's association is still up; serviceWifi() promotes it to TimeSyncing.
    state_ = State::WifiConnecting;
  } else {
    Serial.println("Provisioning: timed out; AP stopped, resuming with stored credentials");
    state_ = credentialsPresent() ? State::WifiConnecting : State::NoCredentials;
  }
}

void NetworkService::requestProvisioning() {
  if (!kDefaultNetworkConfig.enabled) {
    Serial.println("Network: DISABLED; cannot start setup mode");
    return;
  }
  recoveryRequested_ = true;
}

bool NetworkService::forgetCredentials() {
  const bool ok = store().clear();
  memset(&gActive, 0, sizeof(gActive));
  gActiveFromNvs = false;
  Serial.printf("Network: stored Wi-Fi credentials %s\n", ok ? "ERASED (map namespace only)" : "erase FAILED");
  return ok;
}

void NetworkService::allocateBuffer() {
  bufferTried_ = true;
  // PSRAM only, never internal RAM. CONFIG_SPIRAM_IGNORE_NOTFOUND means a
  // missing PSRAM is silent at boot, so verify the pointer's actual region.
  if (!psramFound()) {
    Serial.println("Network: PSRAM not found; feed buffer unavailable");
    return;
  }
  const size_t psramFreeBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t intFreeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  Serial.printf("Mem init: PSRAM total %u free-before %u; internal free-before %u largest-block %u; request %u\n",
                static_cast<unsigned>(ESP.getPsramSize()), static_cast<unsigned>(psramFreeBefore),
                static_cast<unsigned>(intFreeBefore),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(kFeedBufferCapacity));
  void *p = heap_caps_malloc(kFeedBufferCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const bool external = p != nullptr && esp_ptr_external_ram(p);
  Serial.printf("Mem init: alloc %s ptr %p external-ram %s; PSRAM free-after %u; internal free-after %u\n",
                p != nullptr ? "OK" : "FAIL", p, external ? "YES" : "NO",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  if (!external) {
    if (p != nullptr) heap_caps_free(p);
    Serial.println("Network: PSRAM feed buffer allocation failed");
    return;
  }
  buffer_ = {static_cast<uint8_t *>(p), kFeedBufferCapacity};
  Serial.printf("Network: feed buffer %u B in PSRAM\n", static_cast<unsigned>(kFeedBufferCapacity));
}

void NetworkService::serviceWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    state_ = State::TimeSyncing;
    timeStarted_ = false;
    return;
  }
  if (!wifiStarted_ || elapsed(nowMs, lastWifiAttemptMs_) >= kDefaultNetworkConfig.wifiRetryMs) {
    // Retries are spaced by wifiRetryMs: never a tight reconnect loop.
    const int statusBefore = static_cast<int>(WiFi.status());
    WiFi.mode(WIFI_STA);
    WiFi.begin(gActive.ssid, gActive.password);
    wifiStarted_ = true;
    lastWifiAttemptMs_ = nowMs;
    Serial.printf("Network: Wi-Fi association attempt (WiFi.status() before = %d)\n", statusBefore);
  }
  // Credentials exist; an invisible SSID is a temporary condition, never a reason
  // to erase credentials or enter the portal.
  state_ = WiFi.status() == WL_NO_SSID_AVAIL ? State::TemporarilyUnavailable : State::WifiConnecting;
}

void NetworkService::serviceTime(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED) {
    state_ = State::WifiConnecting;
    return;
  }
  if (!timeStarted_ || elapsed(nowMs, timeStartMs_) >= kDefaultNetworkConfig.timeSyncTimeoutMs) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");  // UTC; no zone or DST offsets
    timeStarted_ = true;
    timeStartMs_ = nowMs;
  }
  if (static_cast<uint32_t>(time(nullptr)) >= kMinValidUnixTime) {
    state_ = State::Ready;
    Serial.printf("Network: UTC time synchronized (%lu)\n",
                  static_cast<unsigned long>(time(nullptr)));
  }
}

void NetworkService::printStatus(const transport::CycleResult &r) const {
  static const char *const kNames[] = {"COMPLETE", "PARTIAL", "NO FEEDS", "SKIPPED(buffer)",
                                       "SKIPPED(wifi)", "SKIPPED(time)"};
  const pipeline::Counters &p = pipeline_.counters();
  Serial.printf("Network cycle %s: feeds %u/%u (skipped by budget %u), trains %lu, LEDs %u (collisions %u), decodeFail %lu\n",
                kNames[static_cast<uint8_t>(r.outcome)], r.processed, r.attempted, r.skippedByBudget,
                static_cast<unsigned long>(p.resolvedTrains), p.occupiedLeds, p.collisionLeds,
                static_cast<unsigned long>(p.feedDecodeFailures));
  Serial.printf("Network counters: wifi %lu time %lu secure %lu http %lu timeout %lu zero %lu "
                "large %lu partial %lu unkLen %lu buf %lu cycles %lu partialCycles %lu "
                "emptyCycles %lu budgetExceeded %lu skippedByBudget %lu bytes %llu\n",
                static_cast<unsigned long>(counters_.wifiUnavailable),
                static_cast<unsigned long>(counters_.timeUnavailable),
                static_cast<unsigned long>(counters_.secureConnectFailures),
                static_cast<unsigned long>(counters_.httpFailures),
                static_cast<unsigned long>(counters_.timeouts),
                static_cast<unsigned long>(counters_.zeroLength),
                static_cast<unsigned long>(counters_.feedTooLarge),
                static_cast<unsigned long>(counters_.partialDownload),
                static_cast<unsigned long>(counters_.unknownLength),
                static_cast<unsigned long>(counters_.bufferUnavailable),
                static_cast<unsigned long>(counters_.pollCycles),
                static_cast<unsigned long>(counters_.partialCycles),
                static_cast<unsigned long>(counters_.emptyCycles),
                static_cast<unsigned long>(counters_.cycleBudgetExceeded),
                static_cast<unsigned long>(counters_.feedsSkippedByBudget),
                static_cast<unsigned long long>(counters_.bytesDownloaded));
  Serial.printf("Mem cycle %lu: buf %p external-ram %s; PSRAM free %u min %u; internal free %u min %u largest %u\n",
                static_cast<unsigned long>(counters_.pollCycles), static_cast<const void *>(buffer_.data),
                (buffer_.data != nullptr && esp_ptr_external_ram(buffer_.data)) ? "YES" : "NO",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
}

void NetworkService::pollNow(uint32_t nowMs) {
  static const MillisClock clock;
  FailureLogger logger(webStatus_);
  semantic::RenderConfig renderConfig{};
  renderConfig.brightness = configuration_.brightness();
  transport::PipelineSink sink(pipeline_, renderConfig);
  sink.setNow(static_cast<uint32_t>(time(nullptr)));
  const transport::PollPreconditions pre{WiFi.status() == WL_CONNECTED,
                                         static_cast<uint32_t>(time(nullptr)) >= kMinValidUnixTime};
  const transport::CycleResult r = transport::runPollCycle(
      sink, connection(), buffer_, kFeeds, kFeedCount, pre,
      kDefaultNetworkConfig.acceptUnknownLength, counters_,
      transport::CycleBudget{&clock, kDefaultNetworkConfig.cycleBudgetMs}, &logger);
  latestCycle_ = r;
  hasLatestCycle_ = true;
  completedCycle_ = r;
  completedCycleAtMs_ = millis();
  completedCyclePending_ = true;
  scheduler_.cycleFinished(completedCycleAtMs_);
  (void)nowMs;
  printStatus(r);
}

void NetworkService::update(uint32_t nowMs) {
  if (state_ == State::Disabled) return;
  if (recoveryRequested_) {
    recoveryRequested_ = false;
    if (state_ != State::Provisioning) {
      Serial.println("Network: setup mode requested by user (stored credentials kept)");
      startProvisioning(nowMs);
    }
  }
  if (state_ == State::Provisioning) {
    serviceProvisioning(nowMs);
    if (state_ == State::Provisioning) return;
  }
  serviceLanWebUi(nowMs);
  switch (state_) {
    case State::Disabled:
    case State::NoCredentials:
      return;
    case State::Provisioning:
      return;
    case State::WifiConnecting:
    case State::TemporarilyUnavailable:
      serviceWifi(nowMs);
      return;
    case State::TimeSyncing:
      serviceTime(nowMs);
      return;
    case State::Ready:
      if (WiFi.status() != WL_CONNECTED) {
        state_ = State::WifiConnecting;
        return;
      }
      // The cycle blocks loop() while it downloads (documented limitation).
      if (scheduler_.due(nowMs)) pollNow(nowMs);
      return;
  }
}

void NetworkService::stopLanWebUi() {
  lanWebUi_.stop();
  if (mdnsStarted_) {
    MDNS.end();
    mdnsStarted_ = false;
  }
  mdnsAttempted_ = false;
}

void NetworkService::serviceLanWebUi(uint32_t nowMs) {
  if (state_ == State::Provisioning || WiFi.status() != WL_CONNECTED) {
    if (serverOwnership_.mode() == web::ServerMode::NormalLan) {
      stopLanWebUi();
      serverOwnership_.stop();
    }
    webStatus_.networkConnected = false;
    webStatus_.ssid[0] = '\0';
    webStatus_.ip[0] = '\0';
    webStatus_.rssi = -127;
    webStatus_.mdnsAvailable = false;
    return;
  }

  if (!mdnsAttempted_) {
    mdnsAttempted_ = true;
    mdnsStarted_ = MDNS.begin(web::kHostname);
    if (mdnsStarted_) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("LAN UI: mDNS available at %s\n", web::kMdnsUrl);
    } else {
      Serial.println("LAN UI: mDNS unavailable; DHCP address remains available");
    }
  }

  if (serverOwnership_.mode() == web::ServerMode::Provisioning) return;
  bool serverStarted = false;
  if (serverOwnership_.mode() != web::ServerMode::NormalLan) {
    if (!serverOwnership_.enterNormalLan()) return;
    if (!lanWebUi_.begin()) {
      serverOwnership_.stop();
      Serial.println("LAN UI: failed to start HTTP server");
      return;
    }
    serverStarted = true;
  }

  if (serverStarted) {
    const IPAddress ip = WiFi.localIP();
    const IPAddress subnet = WiFi.subnetMask();
    const IPAddress gateway = WiFi.gatewayIP();
    const IPAddress dnsPrimary = WiFi.dnsIP(0);
    const IPAddress dnsSecondary = WiFi.dnsIP(1);
    const String connectedSsid = WiFi.SSID();
    const String ipText = ip.toString();
    const String subnetText = subnet.toString();
    const String gatewayText = gateway.toString();
    const String dnsPrimaryText = dnsPrimary.toString();
    const String dnsSecondaryText = dnsSecondary.toString();
    connectedSsid.toCharArray(webStatus_.ssid, sizeof(webStatus_.ssid));
    ipText.toCharArray(webStatus_.ip, sizeof(webStatus_.ip));
    subnetText.toCharArray(webStatus_.subnet, sizeof(webStatus_.subnet));
    gatewayText.toCharArray(webStatus_.gateway, sizeof(webStatus_.gateway));
    dnsPrimaryText.toCharArray(webStatus_.dnsPrimary,
                               sizeof(webStatus_.dnsPrimary));
    dnsSecondaryText.toCharArray(webStatus_.dnsSecondary,
                                 sizeof(webStatus_.dnsSecondary));
  }
  webStatus_.uptimeSeconds = millis() / 1000;
  webStatus_.networkConnected = true;
  webStatus_.rssi = static_cast<int16_t>(WiFi.RSSI());
  webStatus_.mdnsAvailable = mdnsStarted_;
  webStatus_.timeSynchronized =
      static_cast<uint32_t>(time(nullptr)) >= kMinValidUnixTime;
  webStatus_.unixTime = webStatus_.timeSynchronized
                            ? static_cast<uint32_t>(time(nullptr))
                            : 0;
  webStatus_.hasCycle = hasLatestCycle_;
  if (hasLatestCycle_) {
    webStatus_.cycleOutcome = latestCycle_.outcome;
    webStatus_.feedsTotal = latestCycle_.attempted;
    if (latestCycle_.outcome == transport::CycleOutcome::Complete ||
        latestCycle_.outcome == transport::CycleOutcome::Partial ||
        latestCycle_.outcome == transport::CycleOutcome::NoFeeds) {
      const pipeline::Counters &counts = pipeline_.counters();
      const uint32_t processed = counts.feedsProcessed;
      const uint32_t usable =
          processed > counts.feedDecodeFailures
              ? processed - counts.feedDecodeFailures
              : 0;
      webStatus_.feedsSucceeded =
          usable > UINT8_MAX ? UINT8_MAX : static_cast<uint8_t>(usable);
    } else {
      webStatus_.feedsSucceeded = 0;
    }
  }
  webStatus_.freeInternalHeap =
      static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  webStatus_.minimumInternalHeap = static_cast<uint32_t>(
      heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
  webStatus_.psramPresent = psramFound();
  webStatus_.freePsram = webStatus_.psramPresent
                             ? static_cast<uint32_t>(
                                   heap_caps_get_free_size(MALLOC_CAP_SPIRAM))
                             : 0;
  webStatus_.minimumFreePsram =
      webStatus_.psramPresent
          ? static_cast<uint32_t>(
                heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM))
          : 0;
  webStatus_.psramFeedBufferPresent = buffer_.data != nullptr;
  webStatus_.transportCounters = counters_;
  webStatus_.networkDiagnostics = networkDiagnostics_.snapshot();
  lanWebUi_.service(webStatus_);

  if (lanWebUi_.consumeNetworkDiagnosticsRequest()) {
    if (networkDiagnostics_.start(diagnosticProbe())) {
      webStatus_.networkDiagnostics = networkDiagnostics_.snapshot();
      Serial.println("Network diagnostics: user-triggered read-only run started");
    }
  }
  serviceNetworkDiagnostics(nowMs);

  if (lanWebUi_.consumeSetupRequest()) {
    Serial.println("LAN UI: user requested Wi-Fi setup; existing credentials kept");
    startProvisioning(nowMs);
    return;
  }
  if (lanWebUi_.consumeRebootRequest()) {
    Serial.println("LAN UI: user requested reboot");
    delay(250);
    ESP.restart();
  }
}

void NetworkService::serviceNetworkDiagnostics(uint32_t nowMs) {
  const diagnostics::RunState previousState =
      networkDiagnostics_.snapshot().state;
  if (previousState != diagnostics::RunState::Running) return;
  networkDiagnostics_.step(diagnosticProbe(), nowMs);
  webStatus_.networkDiagnostics = networkDiagnostics_.snapshot();
  if (webStatus_.networkDiagnostics.state == diagnostics::RunState::Complete) {
    const diagnostics::Snapshot &result = webStatus_.networkDiagnostics;
    Serial.printf(
        "Network diagnostics complete: wifi=%s ntp-dns=%s (%lums) "
        "mta-dns=%s (%lums) tcp443=%s (%lums)\n",
        result.wifiAssociated ? "associated" : "disconnected",
        diagnostics::checkStateName(result.ntpDns.state),
        static_cast<unsigned long>(result.ntpDns.elapsedMs),
        diagnostics::checkStateName(result.mtaDns.state),
        static_cast<unsigned long>(result.mtaDns.elapsedMs),
        diagnostics::checkStateName(result.tcp443.state),
        static_cast<unsigned long>(result.tcp443.elapsedMs));
  }
}

#else  // !BROOKLYN_NETWORK_ENABLED: no Wi-Fi, TLS or SNTP code is linked.

NetworkService::NetworkService(pipeline::TrainPipeline &pipeline)
    : pipeline_(pipeline), counters_{}, buffer_{nullptr, 0}, scheduler_{},
      state_(State::Disabled), bufferTried_(false), wifiStarted_(false),
      timeStarted_(false), recoveryRequested_(false), lastWifiAttemptMs_(0), timeStartMs_(0) {}

uint8_t NetworkService::brightness() const {
  return config::kDefaultBrightness;
}

uint32_t NetworkService::routeVisibilityMask() const {
  return config::kDefaultRouteVisibilityMask;
}

bool NetworkService::consumeCompletedCycle(transport::CycleResult &, uint32_t &) {
  return false;
}

bool NetworkService::credentialsPresent() const { return false; }
void NetworkService::loadCredentials() {}
void NetworkService::startProvisioning(uint32_t) {}
void NetworkService::serviceProvisioning(uint32_t) {}
void NetworkService::requestProvisioning() {
  Serial.println("Network: DISABLED; cannot start setup mode");
}
bool NetworkService::forgetCredentials() { return false; }
void NetworkService::allocateBuffer() {}
void NetworkService::serviceWifi(uint32_t) {}
void NetworkService::serviceTime(uint32_t) {}
void NetworkService::pollNow(uint32_t) {}
void NetworkService::printStatus(const transport::CycleResult &) const {}

void NetworkService::begin() {
  state_ = State::Disabled;
  Serial.println("Network: DISABLED (BROOKLYN_NETWORK_ENABLED=0; no Wi-Fi, no fetch)");
}

void NetworkService::update(uint32_t) {}

#endif

}  // namespace brooklyn::network
