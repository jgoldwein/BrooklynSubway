// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4J ESP32 network layer. See network_service.h.
// Not validated on hardware (ESP32-S3 not connected during Phase 4J).

#include "network_service.h"

#include <Arduino.h>

#if BROOKLYN_NETWORK_ENABLED
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <soc/soc_memory_types.h>
#include <time.h>

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

constexpr uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }

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
    case State::WifiConnecting: return "WIFI CONNECTING";
    case State::TimeSyncing: return "TIME SYNCING";
    case State::Ready: return "READY";
  }
  return "?";
}

}  // namespace

NetworkService::NetworkService(pipeline::TrainPipeline &pipeline)
    : pipeline_(pipeline), counters_{}, buffer_{nullptr, 0}, scheduler_{},
      state_(State::Disabled), bufferTried_(false), wifiStarted_(false),
      timeStarted_(false), lastWifiAttemptMs_(0), timeStartMs_(0) {}

bool NetworkService::credentialsPresent() const { return kSsid[0] != '\0'; }

void NetworkService::begin() {
  if (!kDefaultNetworkConfig.enabled) {
    state_ = State::Disabled;
  } else if (!credentialsPresent()) {
    state_ = State::NoCredentials;
  } else {
    state_ = State::WifiConnecting;
    scheduler_.configure(kDefaultNetworkConfig.pollIntervalMs);
    allocateBuffer();
  }
  Serial.printf("Network: %s\n", stateName(state_));
  if (state_ == State::NoCredentials) {
    Serial.println("Network: copy include/secrets.example.h to include/secrets.h and set the SSID");
  }
}

void NetworkService::allocateBuffer() {
  bufferTried_ = true;
  // PSRAM only, never internal RAM. CONFIG_SPIRAM_IGNORE_NOTFOUND means a
  // missing PSRAM is silent at boot, so verify the pointer's actual region.
  if (!psramFound()) {
    Serial.println("Network: PSRAM not found; feed buffer unavailable");
    return;
  }
  void *p = heap_caps_malloc(kFeedBufferCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr || !esp_ptr_external_ram(p)) {
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
    WiFi.mode(WIFI_STA);
    WiFi.begin(kSsid, kPassword);
    wifiStarted_ = true;
    lastWifiAttemptMs_ = nowMs;
    Serial.println("Network: Wi-Fi association attempt");
  }
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
}

void NetworkService::pollNow(uint32_t nowMs) {
  static const MillisClock clock;
  transport::PipelineSink sink(pipeline_);
  sink.setNow(static_cast<uint32_t>(time(nullptr)));
  const transport::PollPreconditions pre{WiFi.status() == WL_CONNECTED,
                                         static_cast<uint32_t>(time(nullptr)) >= kMinValidUnixTime};
  const transport::CycleResult r = transport::runPollCycle(
      sink, connection(), buffer_, kFeeds, kFeedCount, pre,
      kDefaultNetworkConfig.acceptUnknownLength, counters_,
      transport::CycleBudget{&clock, kDefaultNetworkConfig.cycleBudgetMs});
  scheduler_.cycleFinished(millis());
  (void)nowMs;
  printStatus(r);
}

void NetworkService::update(uint32_t nowMs) {
  switch (state_) {
    case State::Disabled:
    case State::NoCredentials:
      return;
    case State::WifiConnecting:
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

#else  // !BROOKLYN_NETWORK_ENABLED: no Wi-Fi, TLS or SNTP code is linked.

NetworkService::NetworkService(pipeline::TrainPipeline &pipeline)
    : pipeline_(pipeline), counters_{}, buffer_{nullptr, 0}, scheduler_{},
      state_(State::Disabled), bufferTried_(false), wifiStarted_(false),
      timeStarted_(false), lastWifiAttemptMs_(0), timeStartMs_(0) {}

bool NetworkService::credentialsPresent() const { return false; }
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
