// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA resource-gate bench harness. See telemetry_bench.h.

#include "telemetry_bench.h"

#if defined(BROOKLYN_TARGET_XIAO) && BROOKLYN_NETWORK_ENABLED
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

#if defined(BROOKLYN_TARGET_XIAO) && BROOKLYN_NETWORK_ENABLED && \
    defined(BROOKLYN_MQTT_BENCH_PASSWORD) && !defined(BROOKLYN_MQTT_BENCH_DISABLE)
#define BROOKLYN_TELEMETRY_BENCH_ACTIVE 1
#else
#define BROOKLYN_TELEMETRY_BENCH_ACTIVE 0
#endif

#if BROOKLYN_TELEMETRY_BENCH_ACTIVE

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// The Arduino WiFiClientSecure library ships its own esp_crt_bundle.h that shadows the
// ESP-IDF one and exposes only arduino_esp_crt_bundle_*(), a single process-wide bundle
// that every WiFiClientSecure::setCACertBundle() call frees and rebuilds. esp-mqtt uses
// the native ESP-IDF verifier instead, so it never shares mutable state with the MTA
// HTTPS client. (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y, full bundle, in this framework.)
extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

namespace brooklyn::telemetry_bench {
namespace {

// Temporary bench identity: account/topic segment "brooklynmap"; the collector maps it
// to this synthetic canonical UUID with --test-alias brooklynmap=<UUID>.
constexpr char kBrokerHost[] = "mqtt.goldwein.net";
constexpr uint16_t kBrokerPort = 8884;
constexpr char kUsername[] = "brooklynmap";
constexpr char kClientId[] = "brooklynmap-xiao-bench";
constexpr char kTopicStatus[] = "brooklyn/v1/brooklynmap/status";
constexpr char kTopicEvent[] = "brooklyn/v1/brooklynmap/event";
constexpr char kTopicAvail[] = "brooklyn/v1/brooklynmap/availability";
constexpr char kDeviceId[] = "00000000-0000-4000-8000-000000000b01";
constexpr uint32_t kMinValidUnixTime = 1767225600;  // mirrors the network service

constexpr int kKeepAliveS = 60;
constexpr int kRxBuffer = 1024;  // esp-mqtt default; measured, see docs
constexpr int kTxBuffer = 1024;
constexpr uint32_t kHeartbeatMs = 30000;
constexpr uint32_t kAckTimeoutMs = 15000;
constexpr size_t kPayloadCapacity = 4608;
constexpr size_t kTarget2k = 2048;
constexpr size_t kTarget4k = 4000;  // under the 4,096 B design ceiling

enum class Mqtt : uint8_t { None, Created, Connecting, Connected, Disconnected, Error };
const char *mqttName(Mqtt s) {
  switch (s) {
    case Mqtt::None: return "none";
    case Mqtt::Created: return "created";
    case Mqtt::Connecting: return "connecting";
    case Mqtt::Connected: return "connected";
    case Mqtt::Disconnected: return "disconnected";
    default: return "error";
  }
}

struct Snapshot {
  uint32_t freeInternal, minInternal, largestInternal, freePsram, minPsram;
};

struct Event {
  uint32_t ms;
  int32_t id;
  int32_t a, b, c;
};

esp_mqtt_client_handle_t gClient = nullptr;
volatile Mqtt gMqtt = Mqtt::None;
char *gPayload = nullptr;  // PSRAM scratch so it does not disturb internal-heap numbers

// Written by the MQTT task, read by loop().
constexpr int kEvRing = 24;
Event gEvRing[kEvRing];
volatile uint32_t gEvHead = 0;
uint32_t gEvTail = 0;
volatile int32_t gAckedId = -1;

// Heap sampler window (sampler task writes, loop resets).
volatile uint32_t gWinMinFree = UINT32_MAX;
volatile uint32_t gWinMinLargest = UINT32_MAX;

Snapshot gPre{};
bool gHavePre = false;
uint32_t gPreMs = 0;
uint32_t gUpdateStartMs = 0;
uint32_t gLastUpdateDurationMs = 0;

// Script state.
enum class Step : uint8_t {
  WaitReady, WaitConnect, Settle, WaitOnlineAck, WaitEventAck, Wait2kAck, Wait4kAck,
  WaitRestoreAck, Running, Negative
};
Step gStep = Step::WaitReady;
uint32_t gStepMs = 0;
int32_t gPendingId = -1;
uint32_t gPendingSentMs = 0;
bool gWifiSeen = false, gReadySeen = false;
bool gEverConnected = false;
bool gNeedReannounce = false;
uint32_t gConnectedAtMs = 0;
uint32_t gLastHeartbeatMs = 0;
uint32_t gSequence = 0;
char gBootId[37];
uint32_t gCycles = 0;
uint8_t gLastOutcome = 255, gLastAttempted = 0, gLastProcessed = 0;
bool gMtaRunning = false;
uint32_t gConnectCount = 0, gDisconnectCount = 0, gErrorCount = 0, gBeforeConnectCount = 0;
uint32_t gAckCount = 0;
uint32_t gDropAtMs = 0;
uint32_t gNegStartMs = 0;
bool gNegConnected = false;
bool gNegSawTlsError = false;
char gStatusNote[96] = "";

void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char *fmt, ...) {
  char line[224];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.printf("[BENCH] %s\n", line);
}

Snapshot takeSnapshot() {
  Snapshot s;
  s.freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s.minInternal = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  s.largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  s.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  s.minPsram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
  return s;
}

uint32_t stackBytes(TaskHandle_t handle) {
  // nullptr means the calling (loop) task for FreeRTOS; a missing named task is reported as 0.
  return static_cast<uint32_t>(uxTaskGetStackHighWaterMark(handle)) * 4;
}

void checkpoint(const char *label, const char *note) {
  const Snapshot s = takeSnapshot();
  const uint32_t loopHw = stackBytes(nullptr);
  const TaskHandle_t mqttTask = xTaskGetHandle("mqtt_task");
  const uint32_t mqttHw = mqttTask == nullptr ? 0 : stackBytes(mqttTask);
  say("CP %s t=%lu int_free=%lu int_min=%lu int_largest=%lu psram_free=%lu psram_min=%lu "
      "mqtt=%s mta=%s loop_stack_hw=%lu mqtt_stack_hw=%lu | %s",
      label, static_cast<unsigned long>(millis()), static_cast<unsigned long>(s.freeInternal),
      static_cast<unsigned long>(s.minInternal), static_cast<unsigned long>(s.largestInternal),
      static_cast<unsigned long>(s.freePsram), static_cast<unsigned long>(s.minPsram),
      mqttName(gMqtt), gMtaRunning ? "running" : (gCycles == 0 ? "none-yet" : "idle"),
      static_cast<unsigned long>(loopHw), static_cast<unsigned long>(mqttHw), note);
}

void samplerTask(void *) {
  for (;;) {
    const uint32_t f = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const uint32_t l = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (f < gWinMinFree) gWinMinFree = f;
    if (l < gWinMinLargest) gWinMinLargest = l;
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

void resetWindow() {
  gWinMinFree = UINT32_MAX;
  gWinMinLargest = UINT32_MAX;
}

void pushEvent(int32_t id, int32_t a, int32_t b, int32_t c) {
  const uint32_t head = gEvHead;
  gEvRing[head % kEvRing] = Event{millis(), id, a, b, c};
  gEvHead = head + 1;
}

// Runs in the MQTT task: record only, never block, never call back into the client.
void onMqttEvent(void *, esp_event_base_t, int32_t eventId, void *eventData) {
  const esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(eventData);
  switch (eventId) {
    case MQTT_EVENT_BEFORE_CONNECT:
      gMqtt = Mqtt::Connecting;
      pushEvent(eventId, 0, 0, 0);
      break;
    case MQTT_EVENT_CONNECTED:
      gMqtt = Mqtt::Connected;
      pushEvent(eventId, event->session_present, 0, 0);
      break;
    case MQTT_EVENT_DISCONNECTED:
      gMqtt = Mqtt::Disconnected;
      pushEvent(eventId, 0, 0, 0);
      break;
    case MQTT_EVENT_PUBLISHED:
      gAckedId = event->msg_id;
      pushEvent(eventId, event->msg_id, 0, 0);
      break;
    case MQTT_EVENT_ERROR:
      gMqtt = Mqtt::Error;
      if (event->error_handle != nullptr) {
        pushEvent(eventId, event->error_handle->error_type,
                  event->error_handle->esp_tls_last_esp_err,
                  event->error_handle->esp_tls_cert_verify_flags);
        pushEvent(1000, event->error_handle->esp_tls_stack_err,
                  event->error_handle->esp_transport_sock_errno,
                  event->error_handle->connect_return_code);
      } else {
        pushEvent(eventId, -1, 0, 0);
      }
      break;
    default:
      pushEvent(eventId, 0, 0, 0);
      break;
  }
}

const char *eventName(int32_t id) {
  switch (id) {
    case MQTT_EVENT_BEFORE_CONNECT: return "BEFORE_CONNECT";
    case MQTT_EVENT_CONNECTED: return "CONNECTED";
    case MQTT_EVENT_DISCONNECTED: return "DISCONNECTED";
    case MQTT_EVENT_PUBLISHED: return "PUBLISHED(PUBACK)";
    case MQTT_EVENT_ERROR: return "ERROR";
    case 1000: return "ERROR_DETAIL";
    default: return "OTHER";
  }
}

// Returns true once per drained event; handles bookkeeping that must run in loop().
void drainEvents() {
  while (gEvTail != gEvHead) {
    const Event e = gEvRing[gEvTail % kEvRing];
    ++gEvTail;
    say("EVT t=%lu %s a=%ld b=0x%lx c=0x%lx", static_cast<unsigned long>(e.ms),
        eventName(e.id), static_cast<long>(e.a), static_cast<unsigned long>(e.b),
        static_cast<unsigned long>(e.c));
    if (e.id == MQTT_EVENT_CONNECTED) {
      ++gConnectCount;
      gConnectedAtMs = e.ms;
      if (gStep == Step::Negative) {
        gNegConnected = true;
      } else if (gEverConnected) {
        gNeedReannounce = true;
      }
      gEverConnected = true;
    } else if (e.id == MQTT_EVENT_DISCONNECTED) {
      ++gDisconnectCount;
    } else if (e.id == MQTT_EVENT_ERROR) {
      ++gErrorCount;
      if (gStep == Step::Negative && e.a == MQTT_ERROR_TYPE_TCP_TRANSPORT) gNegSawTlsError = true;
    } else if (e.id == MQTT_EVENT_BEFORE_CONNECT) {
      ++gBeforeConnectCount;
    }
  }
}

void makeUuid4(char out[37]) {
  uint8_t b[16];
  esp_fill_random(b, sizeof(b));
  b[6] = static_cast<uint8_t>((b[6] & 0x0F) | 0x40);
  b[8] = static_cast<uint8_t>((b[8] & 0x3F) | 0x80);
  snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0],
           b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14],
           b[15]);
}

const char *resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    default: return "other";
  }
}

// Builds a heartbeat that matches docs/schemas/brooklyn-telemetry-v1.schema.json.
// `padExtensions` adds bounded x_ extensions (schema-valid) up to ~targetBytes;
// `transportPad` appends a non-schema "pad" member (transport-size test only).
size_t buildStatus(char *buf, size_t cap, size_t targetBytes, bool padExtensions,
                   bool transportPad) {
  const Snapshot s = takeSnapshot();
  const time_t now = time(nullptr);
  const bool synced = static_cast<uint32_t>(now) >= kMinValidUnixTime;
  char iso[24] = "";
  char timeField[40] = "null";
  if (synced) {
    struct tm tmv;
    gmtime_r(&now, &tmv);
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    snprintf(timeField, sizeof(timeField), "\"%s\"", iso);
  }
  const char *poll = gCycles == 0 ? "idle" : (gLastOutcome == 0 ? "complete" : "partial");
  const uint32_t attempted = gCycles == 0 ? 0 : gLastAttempted;
  const uint32_t success = gCycles == 0 ? 0 : gLastProcessed;
  const uint32_t mask = success >= 16 ? 0xFFFF : ((1u << success) - 1u);
  const uint32_t seq = ++gSequence;
  int n = snprintf(
      buf, cap,
      "{\"protocol\":\"brooklyn-telemetry\",\"version\":1,\"kind\":\"heartbeat\","
      "\"device_id\":\"%s\",\"device_name\":\"XIAO Bench\",\"boot_id\":\"%s\","
      "\"sequence\":%lu,\"uptime_ms\":%lu,\"time_synced\":%s,\"device_time_utc\":%s,"
      "\"firmware\":{\"target\":\"xiao-s3\",\"version\":\"4aa-bench\",\"build_id\":\"resource-gate\","
      "\"build_type\":\"bench\",\"image_sha256\":\"%064d\",\"active_partition\":\"app0\","
      "\"ota_state\":\"unknown\"},"
      "\"system\":{\"reset_reason\":\"%s\",\"boot_count\":1,\"heap_free_bytes\":%lu,"
      "\"heap_min_free_bytes\":%lu,\"psram_free_bytes\":%lu,\"psram_min_free_bytes\":%lu},"
      "\"network\":{\"wifi_connected\":%s,\"wifi_rssi_dbm\":%d,\"dns_health\":\"unknown\"},"
      "\"mta\":{\"feed_success_count\":%lu,\"feed_attempted_count\":%lu,\"feed_success_mask\":%lu,"
      "\"poll_state\":\"%s\",\"last_complete_age_s\":null,\"vehicles_processed\":0,"
      "\"vehicles_resolved\":0,\"occupied_map_led_count\":0,\"collision_count\":0},"
      "\"display\":{\"state\":\"WaitingForData\",\"logical_brightness\":0,"
      "\"route_visibility_mask\":4294967295},"
      "\"integrity\":{\"application_reported_status\":\"unknown\",\"semantic_check\":\"unknown\","
      "\"led_mapping_check\":\"unknown\"},"
      "\"configuration\":{\"schema_version\":1,\"load_valid\":true},"
      "\"safety\":{\"physical_output_build_allowed\":false,\"controller_state\":\"not_present\","
      "\"arming_state\":\"not_present\",\"current_limit_state\":\"not_present\"}",
      kDeviceId, gBootId, static_cast<unsigned long>(seq),
      static_cast<unsigned long>(millis()), synced ? "true" : "false", timeField, 0,
      resetReasonName(), static_cast<unsigned long>(s.freeInternal),
      static_cast<unsigned long>(s.minInternal), static_cast<unsigned long>(s.freePsram),
      static_cast<unsigned long>(s.minPsram), WiFi.status() == WL_CONNECTED ? "true" : "false",
      WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127, static_cast<unsigned long>(success),
      static_cast<unsigned long>(attempted), static_cast<unsigned long>(mask), poll);
  if (n < 0 || static_cast<size_t>(n) >= cap) return 0;
  size_t len = static_cast<size_t>(n);

  if (padExtensions) {
    // Up to 8 extensions with <=64-char string values: still schema-valid.
    const char *prefix = ",\"extensions\":{";
    len += snprintf(buf + len, cap - len, "%s", prefix);
    for (int i = 0; i < 8; ++i) {
      if (len + 1 >= targetBytes) break;
      len += snprintf(buf + len, cap - len, "%s\"x_pad%d\":\"%064d\"", i == 0 ? "" : ",", i, i);
    }
    len += snprintf(buf + len, cap - len, "}");
  }
  if (transportPad) {
    len += snprintf(buf + len, cap - len, ",\"pad\":\"");
    while (len + 3 < targetBytes && len + 3 < cap) buf[len++] = 'A';
    buf[len++] = '"';
  }
  if (len + 2 > cap) return 0;
  buf[len++] = '}';
  buf[len] = '\0';
  return len;
}

size_t buildBootEvent(char *buf, size_t cap) {
  const uint32_t seq = ++gSequence;
  const int n = snprintf(
      buf, cap,
      "{\"protocol\":\"brooklyn-telemetry\",\"version\":1,\"kind\":\"event\","
      "\"device_id\":\"%s\",\"boot_id\":\"%s\",\"sequence\":%lu,\"uptime_ms\":%lu,"
      "\"time_synced\":true,\"device_time_utc\":null,\"event_type\":\"boot\","
      "\"severity\":\"info\",\"attributes\":{\"reset_reason\":\"%s\"}}",
      kDeviceId, gBootId, static_cast<unsigned long>(seq), static_cast<unsigned long>(millis()),
      resetReasonName());
  return (n < 0 || static_cast<size_t>(n) >= cap) ? 0 : static_cast<size_t>(n);
}

bool createClient(const char *uri) {
  if (gClient != nullptr) return false;
  esp_mqtt_client_config_t cfg{};
  cfg.uri = uri;
  cfg.client_id = kClientId;
  cfg.username = kUsername;
  cfg.password = BROOKLYN_MQTT_BENCH_PASSWORD;
  cfg.keepalive = kKeepAliveS;
  cfg.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
  cfg.lwt_topic = kTopicAvail;
  cfg.lwt_msg = "offline";
  cfg.lwt_msg_len = 7;
  cfg.lwt_qos = 1;
  cfg.lwt_retain = 1;
  cfg.buffer_size = kRxBuffer;
  cfg.out_buffer_size = kTxBuffer;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;  // verified chain AND hostname
  // Native auto-reconnect stays ON (esp-mqtt default, 10 s) so its behaviour is measured.
  gClient = esp_mqtt_client_init(&cfg);
  if (gClient == nullptr) return false;
  if (esp_mqtt_client_register_event(gClient, MQTT_EVENT_ANY, onMqttEvent, nullptr) != ESP_OK) {
    esp_mqtt_client_destroy(gClient);
    gClient = nullptr;
    return false;
  }
  gMqtt = Mqtt::Created;
  return true;
}

void destroyClient() {
  if (gClient == nullptr) return;
  esp_mqtt_client_stop(gClient);
  esp_mqtt_client_destroy(gClient);
  gClient = nullptr;
  gMqtt = Mqtt::None;
}

char gUri[64];
void buildUri(const char *host) { snprintf(gUri, sizeof(gUri), "mqtts://%s:%u", host, kBrokerPort); }

int32_t publishNow(const char *topic, const char *data, size_t len, bool retain) {
  if (gClient == nullptr || gMqtt != Mqtt::Connected) return -1;
  gAckedId = -1;
  const int id = esp_mqtt_client_enqueue(gClient, topic, data, static_cast<int>(len), 1,
                                         retain ? 1 : 0, true);
  gPendingId = id;
  gPendingSentMs = millis();
  say("PUBLISH topic=%s len=%u retain=%d qos=1 msg_id=%d outbox=%d", topic,
      static_cast<unsigned>(len), retain ? 1 : 0, id, esp_mqtt_client_get_outbox_size(gClient));
  return id;
}

bool ackReceived() { return gPendingId > 0 && gAckedId == gPendingId; }

bool ackOrTimeout(const char *what) {
  if (ackReceived()) {
    ++gAckCount;
    say("PUBACK %s msg_id=%ld latency_ms=%lu", what, static_cast<long>(gPendingId),
        static_cast<unsigned long>(millis() - gPendingSentMs));
    gPendingId = -1;
    return true;
  }
  if (gPendingId <= 0 || millis() - gPendingSentMs > kAckTimeoutMs) {
    say("PUBACK %s MISSING (msg_id=%ld) after %lu ms", what, static_cast<long>(gPendingId),
        static_cast<unsigned long>(millis() - gPendingSentMs));
    gPendingId = -1;
    return true;  // proceed, failure is recorded in the log
  }
  return false;
}

void publishHeartbeat(bool retain, bool padded, size_t target, bool transportPad) {
  const size_t len = buildStatus(gPayload, kPayloadCapacity, target, padded, transportPad);
  if (len == 0) {
    say("ERROR status payload did not fit");
    return;
  }
  publishNow(kTopicStatus, gPayload, len, retain);
}

void goStep(Step s) {
  gStep = s;
  gStepMs = millis();
}

void startNegative() {
  if (gStep != Step::Running) {
    say("NEG ignored: wait until the bench is Running");
    return;
  }
  IPAddress ip;
  if (WiFi.hostByName(kBrokerHost, ip) != 1) {
    say("NEG: DNS resolve failed; try again");
    return;
  }
  // Same server, but addressed by IP: certificate hostname verification MUST fail.
  char host[24];
  snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  say("NEG: stopping client; reconnecting by IP literal to force a hostname mismatch");
  destroyClient();
  buildUri(host);
  gNegConnected = false;
  gNegSawTlsError = false;
  goStep(Step::Negative);
  gNegStartMs = millis();
  if (!createClient(gUri) || esp_mqtt_client_start(gClient) != ESP_OK) {
    say("NEG: client create/start failed");
  }
  memset(gUri, 0, sizeof(gUri));
}

void finishNegative() {
  say("NEG RESULT: connected=%s tls_error_seen=%s -> %s", gNegConnected ? "YES" : "no",
      gNegSawTlsError ? "yes" : "no", gNegConnected ? "FAIL (insecure!)" : "PASS (refused)");
  destroyClient();
  buildUri(kBrokerHost);
  if (createClient(gUri) && esp_mqtt_client_start(gClient) == ESP_OK) {
    say("NEG: correct endpoint restored");
  }
  gNeedReannounce = true;
  gEverConnected = true;
  goStep(Step::Running);
}

}  // namespace

void begin() {
  makeUuid4(gBootId);
  gPayload = static_cast<char *>(heap_caps_malloc(kPayloadCapacity, MALLOC_CAP_SPIRAM));
  xTaskCreate(samplerTask, "benchSampler", 2048, nullptr, 2, nullptr);
  say("Resource-gate bench ACTIVE (XIAO only). Broker %s:%u, user %s, TLS verified, MQTT 3.1.1.",
      kBrokerHost, kBrokerPort, kUsername);
  say("Commands: d=drop Wi-Fi, n=negative TLS test, p=checkpoint, u=2KiB publish, f=4KiB publish");
  say("boot_id=%s device_id=%s payload_buffer=%s", gBootId, kDeviceId,
      gPayload != nullptr ? "psram" : "MISSING");
  checkpoint("A", "boot before Wi-Fi");
}

void beforeNetworkUpdate() {
  gPre = takeSnapshot();
  gHavePre = true;
  gPreMs = millis();
  gUpdateStartMs = gPreMs;
  gMtaRunning = true;
  resetWindow();
}

void afterNetworkUpdate(uint32_t nowMs, bool networkReady, uint8_t stateCode) {
  gMtaRunning = false;
  gLastUpdateDurationMs = millis() - gUpdateStartMs;
  drainEvents();

  if (!gWifiSeen && WiFi.status() == WL_CONNECTED) {
    gWifiSeen = true;
    checkpoint("B", "Wi-Fi connected");
  }
  if (!gReadySeen && networkReady) {
    gReadySeen = true;
    checkpoint("C", "SNTP valid (network Ready)");
  }

  switch (gStep) {
    case Step::WaitReady: {
      static uint8_t lastLoggedState = 255;
      if (!networkReady) {
        if (lastLoggedState != stateCode) {
          lastLoggedState = stateCode;
          say("GATE: MQTT blocked, network state=%u (Ready requires Wi-Fi + valid SNTP); client NOT created",
              stateCode);
        }
        break;
      }
      say("GATE: network Ready with valid time -> MQTT allowed");
      checkpoint("D", "before MQTT client creation");
      buildUri(kBrokerHost);
      if (!createClient(gUri)) {
        say("ERROR: esp_mqtt_client_init failed");
        goStep(Step::Running);  // nothing more to do
        break;
      }
      checkpoint("E", "MQTT client created, not connected");
      const esp_err_t err = esp_mqtt_client_start(gClient);
      say("esp_mqtt_client_start -> %d", static_cast<int>(err));
      goStep(Step::WaitConnect);
      break;
    }
    case Step::WaitConnect:
      if (gMqtt == Mqtt::Connected) goStep(Step::Settle);
      break;
    case Step::Settle:
      if (millis() - gStepMs >= 3000) {
        checkpoint("F", "MQTT TLS connected and idle (3 s after CONNACK)");
        publishNow(kTopicAvail, "online", 6, true);
        goStep(Step::WaitOnlineAck);
      }
      break;
    case Step::WaitOnlineAck:
      if (ackOrTimeout("availability=online")) {
        checkpoint("G", "availability=online published and acknowledged");
        char event[400];
        const size_t len = buildBootEvent(event, sizeof(event));
        publishNow(kTopicEvent, event, len, false);
        goStep(Step::WaitEventAck);
      }
      break;
    case Step::WaitEventAck:
      if (ackOrTimeout("event")) {
        publishHeartbeat(true, true, kTarget2k, false);
        goStep(Step::Wait2kAck);
      }
      break;
    case Step::Wait2kAck:
      if (ackOrTimeout("status~2KiB")) {
        checkpoint("H", "~2 KiB QoS1 retained status acknowledged");
        publishHeartbeat(false, false, kTarget4k, true);
        goStep(Step::Wait4kAck);
      }
      break;
    case Step::Wait4kAck:
      if (ackOrTimeout("status~4KiB(non-retained, transport-size test)")) {
        checkpoint("I", "~4 KiB QoS1 (non-retained) acknowledged");
        publishHeartbeat(true, false, 0, false);  // restore a valid retained status
        goStep(Step::WaitRestoreAck);
      }
      break;
    case Step::WaitRestoreAck:
      if (ackOrTimeout("status(restore valid retained)")) {
        gLastHeartbeatMs = millis();
        goStep(Step::Running);
        say("RUNNING: heartbeat every %lu ms; MQTT stays connected during MTA cycles",
            static_cast<unsigned long>(kHeartbeatMs));
      }
      break;
    case Step::Running: {
      if (gClient == nullptr) break;
      if (gPendingId > 0) {
        (void)ackOrTimeout("heartbeat");
        break;
      }
      if (gMqtt != Mqtt::Connected) break;
      if (gNeedReannounce && millis() - gConnectedAtMs >= 3000) {
        gNeedReannounce = false;
        checkpoint("M", "after MQTT reconnect (3 s after CONNACK)");
        if (gDropAtMs != 0) {
          say("RECOVERY: reconnected %lu ms after the Wi-Fi drop command",
              static_cast<unsigned long>(millis() - gDropAtMs));
          gDropAtMs = 0;
        }
        publishNow(kTopicAvail, "online", 6, true);
        gLastHeartbeatMs = millis() - kHeartbeatMs;  // heartbeat right after online ack
        break;
      }
      if (millis() - gLastHeartbeatMs >= kHeartbeatMs) {
        gLastHeartbeatMs = millis();
        publishHeartbeat(true, false, 0, false);
      }
      break;
    }
    case Step::Negative:
      if (gNegConnected || millis() - gNegStartMs > 25000 ||
          (gNegSawTlsError && millis() - gNegStartMs > 8000)) {
        finishNegative();
      }
      break;
  }
  (void)nowMs;
}

void noteCompletedCycle(uint8_t outcome, uint8_t attempted, uint8_t processed) {
  ++gCycles;
  gLastOutcome = outcome;
  gLastAttempted = attempted;
  gLastProcessed = processed;
  const uint32_t minFree = gWinMinFree == UINT32_MAX ? 0 : gWinMinFree;
  const uint32_t minLargest = gWinMinLargest == UINT32_MAX ? 0 : gWinMinLargest;
  char note[96];
  if (gHavePre) {
    snprintf(note, sizeof(note), "pre-cycle snapshot (cycle %lu)", static_cast<unsigned long>(gCycles));
    say("CP J t=%lu int_free=%lu int_min=%lu int_largest=%lu psram_free=%lu psram_min=%lu mqtt=%s mta=about-to-run | %s",
        static_cast<unsigned long>(gPreMs), static_cast<unsigned long>(gPre.freeInternal),
        static_cast<unsigned long>(gPre.minInternal), static_cast<unsigned long>(gPre.largestInternal),
        static_cast<unsigned long>(gPre.freePsram), static_cast<unsigned long>(gPre.minPsram),
        mqttName(gMqtt), note);
  }
  say("CP K cycle=%lu worst_int_free_during_update=%lu worst_int_largest_during_update=%lu update_ms=%lu (25 ms sampler)",
      static_cast<unsigned long>(gCycles), static_cast<unsigned long>(minFree),
      static_cast<unsigned long>(minLargest), static_cast<unsigned long>(gLastUpdateDurationMs));
  snprintf(note, sizeof(note), "outcome=%u attempted=%u processed=%u cycle=%lu connects=%lu disc=%lu err=%lu",
           outcome, attempted, processed, static_cast<unsigned long>(gCycles),
           static_cast<unsigned long>(gConnectCount), static_cast<unsigned long>(gDisconnectCount),
           static_cast<unsigned long>(gErrorCount));
  checkpoint(gCycles >= 10 ? "N" : "L", note);
}

bool handleCommand(int command) {
  switch (command) {
    case 'd': case 'D':
      say("CMD d: dropping Wi-Fi (TCP/TLS session dies silently; broker must fire the Last Will)");
      gDropAtMs = millis();
      WiFi.disconnect(false, false);
      return true;
    case 'n': case 'N':
      startNegative();
      return true;
    case 'p': case 'P':
      checkpoint("MANUAL", "operator request");
      say("COUNTERS connects=%lu disconnects=%lu errors=%lu before_connect=%lu pubacks=%lu cycles=%lu",
          static_cast<unsigned long>(gConnectCount), static_cast<unsigned long>(gDisconnectCount),
          static_cast<unsigned long>(gErrorCount), static_cast<unsigned long>(gBeforeConnectCount),
          static_cast<unsigned long>(gAckCount), static_cast<unsigned long>(gCycles));
      return true;
    case 'u': case 'U':
      if (gStep == Step::Running && gPendingId <= 0) publishHeartbeat(true, true, kTarget2k, false);
      return true;
    case 'f': case 'F':
      if (gStep == Step::Running && gPendingId <= 0) publishHeartbeat(false, false, kTarget4k, true);
      return true;
    default:
      return false;
  }
}

}  // namespace brooklyn::telemetry_bench

#else  // bench inactive: empty no-ops, esp-mqtt is not referenced

namespace brooklyn::telemetry_bench {
void begin() {}
void beforeNetworkUpdate() {}
void afterNetworkUpdate(uint32_t, bool, uint8_t) {}
void noteCompletedCycle(uint8_t, uint8_t, uint8_t) {}
bool handleCommand(int) { return false; }
}  // namespace brooklyn::telemetry_bench

#endif
