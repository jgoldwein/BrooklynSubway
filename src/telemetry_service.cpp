// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA production telemetry service. See telemetry_service.h.

#include "telemetry_service.h"

#if defined(BROOKLYN_TARGET_XIAO) && BROOKLYN_NETWORK_ENABLED
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

#if defined(BROOKLYN_TARGET_XIAO) && BROOKLYN_NETWORK_ENABLED && \
    defined(BROOKLYN_MQTT_BENCH_PASSWORD) && !defined(BROOKLYN_MQTT_BENCH_DISABLE)
#define BROOKLYN_TELEMETRY_ACTIVE 1
#else
#define BROOKLYN_TELEMETRY_ACTIVE 0
#endif

#if BROOKLYN_TELEMETRY_ACTIVE

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// The Arduino WiFiClientSecure library ships its own esp_crt_bundle.h that shadows the
// ESP-IDF one; esp-mqtt uses the native ESP-IDF verifier so it never shares mutable state
// with the MTA HTTPS client's certificate bundle.
extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

namespace brooklyn::telemetry {
namespace {

// Temporary bench identity: account/topic segment "brooklynmap"; the collector maps it to
// this synthetic canonical UUID with --test-alias brooklynmap=<UUID>.
constexpr char kBrokerUri[] = "mqtts://mqtt.goldwein.net:8884";
constexpr char kUsername[] = "brooklynmap";
constexpr char kClientId[] = "brooklynmap-xiao-bench";
constexpr char kTopicStatus[] = "brooklyn/v1/brooklynmap/status";
constexpr char kTopicEvent[] = "brooklyn/v1/brooklynmap/event";
constexpr char kTopicAvail[] = "brooklyn/v1/brooklynmap/availability";
constexpr char kDeviceId[] = "00000000-0000-4000-8000-000000000b01";
constexpr char kDeviceName[] = "XIAO Bench";
constexpr char kFirmwareVersion[] = "4aa-telemetry";
constexpr char kBuildId[] = "xiao-bench";

constexpr int kKeepAliveS = 60;
constexpr int kMqttBuffer = 1024;
constexpr uint32_t kTaskStackBytes = 6144;
constexpr UBaseType_t kTaskPriority = 1;
constexpr uint32_t kIdleWaitMs = 100;
constexpr uint32_t kStatsLogMs = 60000;
constexpr size_t kMqttQueueDepth = 16;

static_assert(static_cast<uint8_t>(DisplayState::NoData) == 5, "display state mapping");

enum class MqttEvent : uint8_t { Connected, Disconnected, Published };
struct QueuedMqttEvent {
  MqttEvent type;
  uint32_t generation;
  int32_t messageId;
};

SemaphoreHandle_t gLock = nullptr;
QueueHandle_t gMqttQueue = nullptr;
TaskHandle_t gTask = nullptr;
char *gPayload = nullptr;  // PSRAM, allocated once; esp-mqtt copies into its own outbox
Core gCore;
Snapshot gSnapshot;
char gBootId[37];
ResetReason gReset = ResetReason::Unknown;
esp_mqtt_client_handle_t gClient = nullptr;
uint32_t gGeneration = 0;
volatile bool gDropWifiRequested = false;
volatile bool gStatsRequested = false;

uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000u; }
uint32_t unixNow() {
  const time_t t = time(nullptr);
  return t > 0 ? static_cast<uint32_t>(t) : 0u;
}

uint32_t rngFn(void *) { return esp_random(); }

void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char *fmt, ...) {
  char line[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.printf("[TELEM] %s\n", line);
}

ResetReason mapReset() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return ResetReason::PowerOn;
    case ESP_RST_SW: return ResetReason::Software;
    case ESP_RST_PANIC: return ResetReason::Panic;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return ResetReason::Watchdog;
    case ESP_RST_BROWNOUT: return ResetReason::Brownout;
    case ESP_RST_DEEPSLEEP: return ResetReason::DeepSleep;
    default: return ResetReason::Other;
  }
}

class Lock {
 public:
  Lock() { xSemaphoreTake(gLock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gLock); }
};

// Runs in the esp-mqtt task: record only, never block, never call back into the client.
void onMqttEvent(void *handlerArg, esp_event_base_t, int32_t eventId, void *eventData) {
  const esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(eventData);
  QueuedMqttEvent out{MqttEvent::Disconnected,
                      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handlerArg)), 0};
  switch (eventId) {
    case MQTT_EVENT_CONNECTED:
      out.type = MqttEvent::Connected;
      break;
    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_ERROR:
      out.type = MqttEvent::Disconnected;
      break;
    case MQTT_EVENT_PUBLISHED:
      out.type = MqttEvent::Published;
      out.messageId = event->msg_id;
      break;
    default:
      return;
  }
  xQueueSend(gMqttQueue, &out, 0);
}

bool startClient() {
  esp_mqtt_client_config_t cfg{};
  cfg.uri = kBrokerUri;
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
  cfg.buffer_size = kMqttBuffer;
  cfg.out_buffer_size = kMqttBuffer;
  cfg.disable_clean_session = false;
  cfg.disable_auto_reconnect = true;  // the telemetry core owns reconnect and backoff
  cfg.crt_bundle_attach = esp_crt_bundle_attach;  // verified chain AND hostname
  gClient = esp_mqtt_client_init(&cfg);
  if (gClient == nullptr) return false;
  ++gGeneration;
  if (esp_mqtt_client_register_event(gClient, MQTT_EVENT_ANY, onMqttEvent,
                                     reinterpret_cast<void *>(static_cast<uintptr_t>(gGeneration))) != ESP_OK ||
      esp_mqtt_client_start(gClient) != ESP_OK) {
    esp_mqtt_client_destroy(gClient);
    gClient = nullptr;
    return false;
  }
  return true;
}

void stopClient() {
  if (gClient == nullptr) return;
  esp_mqtt_client_stop(gClient);
  esp_mqtt_client_destroy(gClient);
  gClient = nullptr;
  ++gGeneration;  // events from the old client are stale from here on
}

void buildStatusInput(StatusInput &s, uint64_t sequence, const Snapshot &snap, const Counters &c) {
  s.deviceId = kDeviceId;
  s.deviceName = kDeviceName;
  s.bootId = gBootId;
  s.sequence = sequence;
  s.uptimeMs = nowMs();
  const uint32_t unixTime = unixNow();
  s.unixTime = timeIsValid(unixTime) ? unixTime : 0;
  s.firmwareTarget = "xiao-s3";
  s.firmwareVersion = kFirmwareVersion;
  s.buildId = kBuildId;
  s.buildTypeBench = true;
  const esp_partition_t *running = esp_ota_get_running_partition();
  s.activePartition = running != nullptr ? running->label : nullptr;
  s.ota = OtaState::Unknown;
  s.reset = gReset;
  s.heapFreeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s.heapMinFreeBytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  s.hasPsram = true;
  s.psramFreeBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  s.psramMinFreeBytes = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
  s.wifiConnected = WiFi.status() == WL_CONNECTED;
  if (s.wifiConnected) {
    s.hasRssi = true;
    s.rssiDbm = WiFi.RSSI();
    if (s.rssiDbm > 0 || s.rssiDbm < -127) s.hasRssi = false;
  }
  s.dns = DnsHealth::Unknown;  // the diagnostics snapshot is private to NetworkService
  s.feedSuccessCount = snap.feedSuccess > 8 ? 8 : snap.feedSuccess;
  s.feedAttemptedCount = snap.feedAttempted > 8 ? 8 : snap.feedAttempted;
  s.hasFeedMask = snap.hasFeedMask;
  s.feedMask = snap.feedMask;
  s.poll = snap.poll;
  s.hasCompleteAge = snap.hasCompleteAge;
  s.completeAgeSeconds = snap.completeAgeSeconds;
  s.vehiclesProcessed = snap.vehiclesProcessed;
  s.vehiclesResolved = snap.vehiclesResolved;
  s.occupiedMapLeds = snap.occupiedMapLeds > 302 ? 302 : snap.occupiedMapLeds;
  s.hasCollisions = snap.hasCollisions;
  s.collisionCount = snap.collisionCount;
  s.display = snap.display;
  s.logicalBrightness = snap.brightness;
  s.hasRouteMask = snap.hasRouteMask;
  s.routeVisibilityMask = snap.routeVisibilityMask;
  s.appStatus = snap.systemHealthy ? AppStatus::ReportedOk : AppStatus::ReportedDegraded;
  s.physicalOutputBuildAllowed = false;
  s.controller = ControllerState::NotPresent;  // XIAO bench: no safety controller exists
  s.arming = ArmingState::NotPresent;
  s.currentLimit = CurrentLimitState::NotPresent;
  s.extEventsDropped = c.eventsDropped;
  s.extReconnects = c.reconnects;
  s.extAcksMissed = c.acksMissed;
  s.extPublishFailures = c.publishFailures;
}

uint32_t taskStackHighWaterBytes(TaskHandle_t handle) {
  // ESP-IDF FreeRTOS reports the high-water mark in bytes, not words.
  return handle == nullptr ? 0u : static_cast<uint32_t>(uxTaskGetStackHighWaterMark(handle));
}

void logStats(const char *why) {
  Counters c;
  uint32_t streak;
  size_t queued;
  bool stable;
  {
    Lock lock;
    c = gCore.counters();
    streak = gCore.failureStreak();
    queued = gCore.queueSize();
    stable = gGeneration != 0 && gCore.stable();
  }
  say("%s up=%lus int_free=%lu int_min=%lu int_largest=%lu psram_free=%lu stack_hw=%lu mqtt_stack_hw=%lu "
      "stable=%d streak=%lu conn=%lu reconn=%lu pubs(ok=%lu miss=%lu fail=%lu) ack_ms(last=%lu max=%lu) "
      "status=%lu ev(sent=%lu q=%u drop=%lu coal=%lu) wraps=%lu",
      why, static_cast<unsigned long>(nowMs() / 1000u),
      static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
      static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
      static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      static_cast<unsigned long>(taskStackHighWaterBytes(gTask)),
      static_cast<unsigned long>(taskStackHighWaterBytes(xTaskGetHandle("mqtt_task"))), stable ? 1 : 0,
      static_cast<unsigned long>(streak), static_cast<unsigned long>(c.connects),
      static_cast<unsigned long>(c.reconnects), static_cast<unsigned long>(c.publishesAcked),
      static_cast<unsigned long>(c.acksMissed), static_cast<unsigned long>(c.publishFailures),
      static_cast<unsigned long>(c.lastAckLatencyMs), static_cast<unsigned long>(c.maxAckLatencyMs),
      static_cast<unsigned long>(c.statusSent), static_cast<unsigned long>(c.eventsSent),
      static_cast<unsigned>(queued), static_cast<unsigned long>(c.eventsDropped),
      static_cast<unsigned long>(c.eventsCoalesced), static_cast<unsigned long>(c.sequenceWraps));
}

void execute(const Action &action) {
  const uint64_t t = nowMs();
  switch (action.type) {
    case ActionType::StartClient: {
      const bool ok = startClient();
      say("connect attempt %s", ok ? "started" : "FAILED to start");
      Lock lock;
      gCore.onClientStarted(ok, nowMs());
      return;
    }
    case ActionType::StopClient: {
      stopClient();
      Lock lock;
      gCore.onClientStopped(nowMs());
      say("client stopped; failure streak=%lu next attempt in %lu ms",
          static_cast<unsigned long>(gCore.failureStreak()),
          static_cast<unsigned long>(gCore.nextConnectAtMs() > t ? gCore.nextConnectAtMs() - t : 0));
      return;
    }
    case ActionType::PublishOnline: {
      const int id = gClient == nullptr
                         ? -1
                         : esp_mqtt_client_enqueue(gClient, kTopicAvail, "online", 6, 1, 1, true);
      Lock lock;
      gCore.publishResult(action, id > 0 ? PublishResult::Queued : PublishResult::Failed, id, nowMs());
      say("PUBLISH availability online msg_id=%d", id);
      return;
    }
    case ActionType::PublishStatus:
    case ActionType::PublishEvent:
      break;
    default:
      return;
  }

  StatusInput *status = nullptr;
  size_t len = 0;
  const bool isStatus = action.type == ActionType::PublishStatus;
  if (isStatus) {
    static StatusInput input;  // single task owner; keeps the task stack small
    Snapshot snap;
    Counters counters;
    {
      Lock lock;
      snap = gSnapshot;
      counters = gCore.counters();
    }
    input = StatusInput();
    buildStatusInput(input, action.sequence, snap, counters);
    status = &input;
    len = buildStatusJson(gPayload, kMaxPayloadBytes + 1, input);
  } else {
    len = buildEventJson(gPayload, kMaxPayloadBytes + 1, kDeviceId, gBootId, action.sequence, action.event,
                         nowMs(), unixNow());
  }
  (void)status;

  PublishResult result = PublishResult::TooLarge;
  int id = 0;
  if (len > 0) {
    id = gClient == nullptr ? -1
                            : esp_mqtt_client_enqueue(gClient, isStatus ? kTopicStatus : kTopicEvent, gPayload,
                                                      static_cast<int>(len), 1, isStatus ? 1 : 0, true);
    result = id > 0 ? PublishResult::Queued : PublishResult::Failed;
  }
  Lock lock;
  gCore.publishResult(action, result, id, nowMs());
  say("PUBLISH %s seq=%llu len=%u retain=%d msg_id=%d%s", isStatus ? "status" : eventTypeName(action.event.type),
      static_cast<unsigned long long>(action.sequence), static_cast<unsigned>(len), isStatus ? 1 : 0, id,
      len == 0 ? " REJECTED(too large)" : "");
}

void handleMqttEvent(const QueuedMqttEvent &e) {
  Lock lock;
  if (e.generation != gGeneration) return;  // stale event from a destroyed client
  const uint64_t t = nowMs();
  switch (e.type) {
    case MqttEvent::Connected:
      gCore.onConnected(t);
      say("CONNECTED (MQTT 3.1.1, verified TLS)");
      break;
    case MqttEvent::Disconnected:
      gCore.onDisconnected(t);
      say("DISCONNECTED / transport error");
      break;
    case MqttEvent::Published:
      gCore.onPublished(e.messageId, t);
      say("PUBACK msg_id=%ld latency_ms=%lu", static_cast<long>(e.messageId),
          static_cast<unsigned long>(gCore.counters().lastAckLatencyMs));
      break;
  }
}

void telemetryTask(void *) {
  bool wifiWasUp = false;
  bool everWifi = false;
  uint64_t lastStatsMs = nowMs();
  for (;;) {
    if (gDropWifiRequested) {
      gDropWifiRequested = false;
      say("TEST: dropping Wi-Fi association");
      WiFi.disconnect();
    }
    if (gStatsRequested) {
      gStatsRequested = false;
      logStats("stats");
    }

    const bool wifiUp = WiFi.status() == WL_CONNECTED;
    if (wifiUp && !wifiWasUp && everWifi) {
      Lock lock;
      gCore.enqueueEvent(makeWifiReconnectedEvent(nowMs()), nowMs());
    }
    if (wifiUp) everWifi = true;
    wifiWasUp = wifiUp;

    Action action;
    {
      Lock lock;
      Inputs in;
      in.networkReady = gSnapshot.networkReady;
      in.wifiUp = wifiUp;
      in.timeValid = timeIsValid(unixNow());
      in.credentials = true;
      action = gCore.next(nowMs(), in);
    }
    if (action.type != ActionType::None) {
      execute(action);
      continue;
    }

    QueuedMqttEvent e;
    if (xQueueReceive(gMqttQueue, &e, pdMS_TO_TICKS(kIdleWaitMs)) == pdTRUE) {
      handleMqttEvent(e);
      while (xQueueReceive(gMqttQueue, &e, 0) == pdTRUE) handleMqttEvent(e);
    }
    if (nowMs() - lastStatsMs >= kStatsLogMs) {
      lastStatsMs = nowMs();
      logStats("stats");
    }
  }
}

}  // namespace

void begin() {
  uint8_t random[16];
  for (size_t i = 0; i < sizeof(random); i += 4) {
    const uint32_t r = esp_random();
    memcpy(random + i, &r, 4);
  }
  formatUuid4(random, gBootId);
  gReset = mapReset();
  gLock = xSemaphoreCreateMutex();
  gMqttQueue = xQueueCreate(kMqttQueueDepth, sizeof(QueuedMqttEvent));
  gPayload = static_cast<char *>(heap_caps_malloc(kMaxPayloadBytes + 1, MALLOC_CAP_SPIRAM));
  if (gLock == nullptr || gMqttQueue == nullptr || gPayload == nullptr) {
    say("init FAILED (lock/queue/payload buffer); telemetry disabled");
    return;
  }
  gCore.begin(gBootId, rngFn, nullptr);
  gCore.enqueueEvent(makeBootEvent(gReset, nowMs()), nowMs());
  bool emit = false;
  const EventRecord reset = makeResetEvent(gReset, nowMs(), emit);
  if (emit) gCore.enqueueEvent(reset, nowMs());
  if (xTaskCreate(telemetryTask, "telemetry", kTaskStackBytes, nullptr, kTaskPriority, &gTask) != pdPASS) {
    say("task create FAILED; telemetry disabled");
    return;
  }
  say("service ACTIVE (XIAO only): task stack=%lu B prio=%u, boot_id=%s reset=%s device_id=%s",
      static_cast<unsigned long>(kTaskStackBytes), static_cast<unsigned>(kTaskPriority), gBootId,
      resetReasonName(gReset), kDeviceId);
  say("Commands: d=drop Wi-Fi (test), p=print stats");
}

void update(const Snapshot &snapshot) {
  if (gLock == nullptr) return;
  Lock lock;
  gSnapshot = snapshot;
}

void noteDisplayTransition(DisplayState previous, DisplayState current) {
  if (gLock == nullptr) return;
  EventRecord event;
  if (!makeDisplayEvent(previous, current, nowMs(), event)) return;
  Lock lock;
  gCore.enqueueEvent(event, nowMs());
}

bool handleCommand(int command) {
  if (gLock == nullptr) return false;
  if (command == 'd' || command == 'D') {
    gDropWifiRequested = true;
    return true;
  }
  if (command == 'p' || command == 'P') {
    gStatsRequested = true;
    return true;
  }
  return false;
}

}  // namespace brooklyn::telemetry

#else  // telemetry not active in this build

namespace brooklyn::telemetry {
void begin() {}
void update(const Snapshot &) {}
void noteDisplayTransition(DisplayState, DisplayState) {}
bool handleCommand(int) { return false; }
}  // namespace brooklyn::telemetry

#endif
