// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA production telemetry service. See telemetry_service.h.

#include "telemetry_service.h"
#include "monitor_seed.h"

#if BROOKLYN_NETWORK_ENABLED
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
#include "ota_esp.h"

// The Arduino WiFiClientSecure library ships its own esp_crt_bundle.h that shadows the
// ESP-IDF one; esp-mqtt uses the native ESP-IDF verifier so it never shares mutable state
// with the MTA HTTPS client's certificate bundle.
extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

namespace brooklyn::telemetry {
namespace {

monitor::Settings gDesired, gActive;
monitor::Runtime gRuntime;
monitor::ConnectionHistory gHistory;
monitor::ResourceGate gResources;
uint32_t gDesiredRevision=1, gActiveRevision=0;
char gBrokerUri[120],gUsername[43],gTopicStatus[80],gTopicEvent[80],gTopicAvail[80];
bool gConfigured=false;
uint32_t gPauseDepth=0;
bool gSuspended=true;
uint64_t gSnapshotAtMs=0;
int32_t gStatusMessageId=0;

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
  char line[320];
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
  cfg.uri = gBrokerUri;
  cfg.client_id = gUsername;
  cfg.username = gUsername;
  cfg.password = gActive.password;
  cfg.keepalive = kKeepAliveS;
  cfg.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
  cfg.lwt_topic = gTopicAvail;
  cfg.lwt_msg = "offline";
  cfg.lwt_msg_len = 7;
  cfg.lwt_qos = 1;
  cfg.lwt_retain = 1;
  cfg.buffer_size = kMqttBuffer;
  cfg.out_buffer_size = kMqttBuffer;
  cfg.disable_clean_session = false;
  cfg.disable_auto_reconnect = true;  // the telemetry core owns reconnect and backoff
  cfg.network_timeout_ms = 5000;
  cfg.task_prio = kTaskPriority;
  cfg.task_stack = kTaskStackBytes;
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
  gStatusMessageId=0;
  ++gGeneration;  // events from the old client are stale from here on
}

void buildStatusInput(StatusInput &s, uint64_t sequence, const Snapshot &snap, const Counters &c) {
  s.deviceId = gActive.deviceId;
  s.deviceName = gActive.name;
  s.bootId = gBootId;
  s.sequence = sequence;
  s.uptimeMs = nowMs();
  const uint32_t unixTime = unixNow();
  s.unixTime = timeIsValid(unixTime) ? unixTime : 0;
  s.firmwareTarget = ota::WebOtaSession::targetName();
  s.firmwareVersion = ota::WebOtaSession::firmwareVersion();
  s.buildId = s.firmwareVersion;
#if defined(BROOKLYN_TARGET_PCB)
  s.buildTypeBench = false;
#endif
  const esp_partition_t *running = esp_ota_get_running_partition();
  s.activePartition = running != nullptr ? running->label : nullptr;
  s.ota = ota::healthConfirmed()?OtaState::Confirmed:OtaState::PendingVerification;
  s.reset = gReset;
  s.heapFreeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s.heapMinFreeBytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  s.hasPsram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM)>0;
  s.psramFreeBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  s.psramMinFreeBytes = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
  s.wifiConnected = WiFi.status() == WL_CONNECTED;
  if (s.wifiConnected) {
    s.hasRssi = true;
    s.rssiDbm = WiFi.RSSI();
    if (s.rssiDbm > 0 || s.rssiDbm < -127) s.hasRssi = false;
  }
  s.dns = DnsHealth::Unknown;  // the diagnostics snapshot is private to NetworkService
  s.reportNetworkAddresses=true;
  if(s.wifiConnected) {
    memcpy(s.wanIpv4,snap.wanIpv4,sizeof(s.wanIpv4));
    const IPAddress ip=WiFi.localIP();
    snprintf(s.lanIpv4,sizeof(s.lanIpv4),"%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
  }
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
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT
  s.physicalOutputBuildAllowed = true;
  s.controller = snap.physicalOutputEnabled?ControllerState::Armed:ControllerState::Disabled;
  s.arming = snap.physicalOutputEnabled?ArmingState::Armed:ArmingState::Disarmed;
  s.currentLimit = CurrentLimitState::Unknown; // There is no physical current sensor.
#else
  s.controller = ControllerState::NotPresent;
  s.arming = ArmingState::NotPresent;
  s.currentLimit = CurrentLimitState::NotPresent;
#endif
  s.extEventsDropped = c.eventsDropped;
  s.extReconnects = c.reconnects;
  s.extAcksMissed = c.acksMissed;
  s.extPublishFailures = c.publishFailures;
  s.hasConnectionHistory = true;
  s.wifiDisconnects=gHistory.wifiLosses;
  s.mqttDisconnects=gHistory.mqttLosses;
  s.lastWifiLossMs=gHistory.lastWifiLossMs;
  s.lastMqttLossMs=gHistory.lastMqttLossMs;
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
  say("%s state=%s attempts=%lu up=%lus int_free=%lu int_min=%lu int_largest=%lu psram_free=%lu stack_hw=%lu mqtt_stack_hw=%lu "
      "stable=%d streak=%lu conn=%lu reconn=%lu pubs(ok=%lu miss=%lu fail=%lu) ack_ms(last=%lu max=%lu) "
      "status=%lu ev(sent=%lu q=%u drop=%lu coal=%lu) wraps=%lu",
      why, monitor::stateName(gRuntime.state),static_cast<unsigned long>(gCore.counters().connectAttempts),static_cast<unsigned long>(nowMs() / 1000u),
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
                         : esp_mqtt_client_enqueue(gClient, gTopicAvail, "online", 6, 1, 1, true);
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
    len = buildEventJson(gPayload, kMaxPayloadBytes + 1, gActive.deviceId, gBootId, action.sequence, action.event,
                         nowMs(), unixNow());
  }
  (void)status;

  PublishResult result = PublishResult::TooLarge;
  int id = 0;
  if (len > 0) {
    id = gClient == nullptr ? -1
                            : esp_mqtt_client_enqueue(gClient, isStatus ? gTopicStatus : gTopicEvent, gPayload,
                                                      static_cast<int>(len), 1, isStatus ? 1 : 0, true);
    result = id > 0 ? PublishResult::Queued : PublishResult::Failed;
  }
  Lock lock;
  if(isStatus && id>0)gStatusMessageId=id;
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
      if(gCore.phase()!=Phase::Idle && gCore.counters().disconnects<2147483647u) {
        const bool wasConnected=gCore.phase()==Phase::Connected;
        const uint32_t before=gCore.counters().disconnects;
        gCore.onDisconnected(t);
        if(wasConnected && gCore.counters().disconnects!=before)gHistory.mqttLost(t);
      }
      say("DISCONNECTED / transport error");
      break;
    case MqttEvent::Published:
      gCore.onPublished(e.messageId, t);
      if(e.messageId==gStatusMessageId) {
        gRuntime.hasStatusAck=true;gRuntime.lastStatusAckMs=t;gStatusMessageId=0;
      }
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
    gHistory.observeWifi(wifiUp,nowMs());
    if (wifiUp && !wifiWasUp && everWifi) {
      Lock lock;
      gCore.enqueueEvent(makeWifiReconnectedEvent(nowMs()), nowMs());
    }
    if (wifiUp) everWifi = true;
    wifiWasUp = wifiUp;

    Action action;
    {
      Lock lock;
      const uint64_t now=nowMs();
      const bool replacing=gActiveRevision!=gDesiredRevision;
      // Close the old session before changing any of the strings owned by MQTT.
      if(replacing && gCore.phase()==Phase::Idle) {
        monitor::wipe(&gActive,sizeof(gActive));gActive=gDesired;
        gActiveRevision=gDesiredRevision;
        snprintf(gBrokerUri,sizeof(gBrokerUri),"mqtts://%s:%u",gActive.host,unsigned(gActive.port));
        snprintf(gUsername,sizeof(gUsername),"bkmap-%s",gActive.deviceId);
        snprintf(gTopicStatus,sizeof(gTopicStatus),"brooklyn/v1/%s/status",gUsername);
        snprintf(gTopicEvent,sizeof(gTopicEvent),"brooklyn/v1/%s/event",gUsername);
        snprintf(gTopicAvail,sizeof(gTopicAvail),"brooklyn/v1/%s/availability",gUsername);
        gRuntime.hasStatusAck=false;
      }
      const bool paused=__atomic_load_n(&gPauseDepth,__ATOMIC_RELAXED)!=0;
      const bool fresh=gSnapshotAtMs && now-gSnapshotAtMs<=monitor::kSnapshotMaxAgeMs;
      const bool memory=gResources.allow(gClient!=nullptr,
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),now);
      Inputs in;
      in.networkReady = gSnapshot.networkReady;
      in.wifiUp = wifiUp;
      in.timeValid = timeIsValid(unixNow());
      in.credentials = gConfigured && gActive.enabled && !replacing && !paused && fresh && memory;
      action = gCore.next(now, in);
      gSuspended=gCore.phase()==Phase::Idle && gClient==nullptr;
      auto &r=gRuntime;
      r.wifiDisconnects=gHistory.wifiLosses;r.mqttDisconnects=gHistory.mqttLosses;
      r.lastWifiLossMs=gHistory.lastWifiLossMs;r.lastMqttLossMs=gHistory.lastMqttLossMs;
      r.state=!gConfigured?monitor::State::NotConfigured:!gDesired.enabled?monitor::State::Disabled:
        paused?monitor::State::Paused:!in.networkReady || !in.wifiUp || !in.timeValid?monitor::State::WaitingNetwork:
        !fresh?monitor::State::SnapshotStale:!memory?monitor::State::WaitingMemory:
        gCore.phase()==Phase::Connected?monitor::State::Online:gCore.failureStreak()?monitor::State::Retrying:monitor::State::Connecting;
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
  if(gLock)return;
  uint8_t random[16];
  for (size_t i = 0; i < sizeof(random); i += 4) {
    const uint32_t r = esp_random();
    memcpy(random + i, &r, 4);
  }
  formatUuid4(random, gBootId);
  gReset = mapReset();
  gLock = xSemaphoreCreateMutex();
  if (gLock == nullptr) {
    say("init FAILED; monitoring unavailable, map continues");
    return;
  }
  (void)monitor::applyPrivateEnrollment();
  const bool loaded=monitor::load(monitor::nvsStore(),gDesired,gConfigured);
  gRuntime.configured=gConfigured;gRuntime.enabled=gConfigured && gDesired.enabled;
  gRuntime.state=!loaded?monitor::State::Failed:!gConfigured?monitor::State::NotConfigured:
    gDesired.enabled?monitor::State::WaitingNetwork:monitor::State::Disabled;
  gCore.begin(gBootId, rngFn, nullptr);
  gCore.enqueueEvent(makeBootEvent(gReset, nowMs()), nowMs());
  bool emit = false;
  const EventRecord reset = makeResetEvent(gReset, nowMs(), emit);
  if (emit) gCore.enqueueEvent(reset, nowMs());
  say("production monitoring: %s; unique per-device credentials required",monitor::stateName(gRuntime.state));
}

void update(const Snapshot &snapshot) {
  if (gLock == nullptr) return;
  bool start=false;
  {
    Lock lock;
    gSnapshot = snapshot;gSnapshotAtMs=nowMs();
    start=gConfigured && gDesired.enabled && !gTask && snapshot.networkReady;
    if(gConfigured && gDesired.enabled && !gTask && !snapshot.networkReady)gRuntime.state=monitor::State::WaitingNetwork;
  }
  // No task/socket/PSRAM allocation until an individual identity is provisioned.
  static uint64_t nextInitMs=0;
  if(start && nowMs()>=nextInitMs) {
    nextInitMs=nowMs()+60000;
    if(!monitor::resourcesAllow(false,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))) {
      Lock lock;gRuntime.state=monitor::State::WaitingMemory;return;
    }
    if(!gMqttQueue)gMqttQueue=xQueueCreate(kMqttQueueDepth,sizeof(QueuedMqttEvent));
    if(!gPayload)gPayload=static_cast<char *>(heap_caps_malloc(kMaxPayloadBytes+1,MALLOC_CAP_SPIRAM));
    if(!gPayload)gPayload=static_cast<char *>(heap_caps_malloc(kMaxPayloadBytes+1,MALLOC_CAP_INTERNAL));
    if(!gMqttQueue || !gPayload || xTaskCreate(telemetryTask,"telemetry",kTaskStackBytes,nullptr,kTaskPriority,&gTask)!=pdPASS) {
      Lock lock;gRuntime.state=monitor::State::Failed;say("monitor task unavailable; map continues; retry in 60 s");
    } else say("monitor worker started; heartbeat about 30 s; verified TLS; no subscriptions");
  }
}

void noteDisplayTransition(DisplayState previous, DisplayState current) {
  if (gLock == nullptr) return;
  EventRecord event;
  if (!makeDisplayEvent(previous, current, nowMs(), event)) return;
  Lock lock;
  gCore.enqueueEvent(event, nowMs());
}

bool handleCommand(int command) {
#if defined(BROOKLYN_TARGET_XIAO) && !defined(BROOKLYN_RELEASE_BUILD)
  if (gLock == nullptr) return false;
  if (command == 'd' || command == 'D') {
    gDropWifiRequested = true;
    return true;
  }
  if (command == 'p' || command == 'P') {
    gStatsRequested = true;
    return true;
  }
#else
  (void)command;
#endif
  return false;
}

monitor::Settings monitoringSettings() {
  if(!gLock)return {};
  Lock lock;return gDesired;
}
monitor::Runtime monitoringStatus() {
  if(!gLock) {monitor::Runtime r;r.state=monitor::State::Failed;return r;}
  Lock lock;auto r=gRuntime;r.uptimeMs=nowMs();return r;
}
bool configureMonitoring(const monitor::Settings &settings) {
  if(!gLock || !monitor::valid(settings))return false;
  const auto store=monitor::nvsStore();if(!store.write || !store.write(store.context,settings))return false;
  Lock lock;monitor::wipe(&gDesired,sizeof(gDesired));gDesired=settings;gConfigured=true;++gDesiredRevision;
  gRuntime.configured=true;gRuntime.enabled=settings.enabled;gRuntime.hasStatusAck=false;
  gRuntime.state=settings.enabled?monitor::State::Connecting:monitor::State::Disabled;
  say("monitor settings saved; enabled=%u",unsigned(settings.enabled));return true;
}
bool clearMonitoring() {
  if(!gLock)return false;
  const auto store=monitor::nvsStore();if(!store.erase || !store.erase(store.context))return false;
  Lock lock;monitor::wipe(&gDesired,sizeof(gDesired));gDesired=monitor::Settings{};gConfigured=false;++gDesiredRevision;
  gRuntime.configured=false;gRuntime.enabled=false;gRuntime.hasStatusAck=false;gRuntime.state=monitor::State::NotConfigured;
  return true;
}
bool pauseForUpdate() {
  __atomic_add_fetch(&gPauseDepth,1u,__ATOMIC_RELAXED);
  if(!gLock || !gTask)return true;
  const uint64_t end=nowMs()+6000;
  do {
    {Lock lock;if(gSuspended)return true;}
    delay(20);
  }while(nowMs()<end);
  say("update waiting for monitoring to release TLS; retry shortly");return false;
}
void resumeAfterUpdate() {
  uint32_t depth=__atomic_load_n(&gPauseDepth,__ATOMIC_RELAXED);
  while(depth && !__atomic_compare_exchange_n(&gPauseDepth,&depth,depth-1,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
}

}  // namespace brooklyn::telemetry

#else  // telemetry not active in this build

namespace brooklyn::telemetry {
void begin() {}
void update(const Snapshot &) {}
void noteDisplayTransition(DisplayState, DisplayState) {}
bool handleCommand(int) { return false; }
monitor::Settings monitoringSettings(){return {};}
monitor::Runtime monitoringStatus(){return {};}
bool configureMonitoring(const monitor::Settings &){return false;}
bool clearMonitoring(){return false;}
bool pauseForUpdate(){return true;}
void resumeAfterUpdate(){}
}  // namespace brooklyn::telemetry

#endif
