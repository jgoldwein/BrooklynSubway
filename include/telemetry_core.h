// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA: host-testable telemetry core. No ESP-IDF, Arduino or firmware headers.
//
// This file holds every policy decision of the telemetry service so it can be unit
// tested on the host: readiness gating, reconnect backoff, status cadence, the
// 16-slot event ring (coalescing, rate limits, overflow), boot_id and message
// sequence, QoS-1 in-flight bookkeeping, and bounded JSON serialization of the
// frozen v1 schemas (docs/schemas/). telemetry_service.cpp is the thin ESP32 adapter
// that executes the actions this core returns.
//
// Time base: all scheduling uses a 64-bit monotonic millisecond clock supplied by the
// caller (esp_timer on hardware). Nothing here reads a clock, does I/O, or allocates.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::telemetry {

constexpr size_t kMaxPayloadBytes = 4096;  // hard serialized ceiling; never truncate
constexpr size_t kEventQueueSlots = 16;
constexpr size_t kMaxInFlight = 4;
constexpr uint32_t kMinValidUnixTime = 1767225600u;  // equals network::kMinValidUnixTime
constexpr uint64_t kMaxSequence = 9007199254740991ull;  // 2^53-1, the schema maximum

constexpr uint32_t kStatusIntervalMs = 30000;
constexpr uint32_t kStatusJitterMs = 3000;  // interval is uniform in [28500, 31500]
constexpr uint32_t kBackoffInitialCeilingMs = 5000;
constexpr uint32_t kBackoffMaxMs = 15u * 60u * 1000u;
constexpr uint32_t kBackoffFloorMs = 1000;
constexpr uint32_t kStartJitterMinMs = 500;
constexpr uint32_t kStartJitterMaxMs = 3000;
constexpr uint32_t kConnectTimeoutMs = 45000;
constexpr uint32_t kAckTimeoutMs = 60000;  // equals the MQTT keepalive
constexpr uint32_t kEventCoalesceWindowMs = 60000;
constexpr uint32_t kEventSpacingMs = 10000;
constexpr uint32_t kEventsPerMinute = 12;

enum class ResetReason : uint8_t { PowerOn, Software, Panic, Watchdog, Brownout, DeepSleep, Other, Unknown };
enum class DnsHealth : uint8_t { Ok, Failed, NotTested, Unknown };
enum class PollState : uint8_t { Idle, Polling, Complete, Partial, NoFeeds, Unknown };
// Same order as display::State; telemetry_service.cpp static_asserts the mapping.
enum class DisplayState : uint8_t { WaitingForData, Fresh, DegradedHold, DegradedPartial, StaleHold, NoData };
enum class OtaState : uint8_t { Idle, PendingVerification, Confirmed, Rollback, Unknown };
enum class AppStatus : uint8_t { ReportedOk, ReportedDegraded, Unknown };
enum class CheckResult : uint8_t { Pass, Fail, Unsupported, Unknown };
enum class ControllerState : uint8_t { NotPresent, Disabled, Safe, Armed, Fault, Unsupported, Unknown };
enum class ArmingState : uint8_t { NotPresent, Disarmed, Armed, Fault, Unsupported, Unknown };
enum class CurrentLimitState : uint8_t { NotPresent, WithinLimit, OverLimit, Unsupported, Unknown };
enum class Tristate : uint8_t { Unknown, False, True };

enum class EventType : uint8_t {
  Boot,
  MonitoringEnabled,
  EnrollmentCompleted,
  WifiReconnected,
  DisplayDegraded,
  DisplayRecovered,
  DisplayStale,
  DisplayNoData,
  OtaPendingVerification,
  OtaConfirmed,
  OtaRollback,
  UnexpectedReset,
  Brownout,
  WatchdogReset,
  ConfigurationInvalid,
  HeapCritical,
  PhysicalSafetyFault,
  Count,
};
enum class Severity : uint8_t { Info, Warning, Critical };

const char *resetReasonName(ResetReason value);
const char *eventTypeName(EventType type);
const char *severityName(Severity severity);
const char *displayStateName(DisplayState state);

// ---------------------------------------------------------------------------
// Identity helpers
// ---------------------------------------------------------------------------

// Formats 16 random bytes as a lowercase RFC 4122 version-4 UUID (the schema's `uuid`
// pattern); sets the version and variant bits. `out` receives 36 characters + NUL.
void formatUuid4(const uint8_t random[16], char out[37]);
// "YYYY-MM-DDTHH:MM:SSZ" for a Unix time; `out` receives 20 characters + NUL.
void formatUtc(uint32_t unixTime, char out[21]);
inline bool timeIsValid(uint32_t unixTime) { return unixTime >= kMinValidUnixTime; }

// ---------------------------------------------------------------------------
// Serialization (explicit privacy whitelist: only these fields can ever be emitted)
// ---------------------------------------------------------------------------

struct StatusInput {
  const char *deviceId = "";
  const char *deviceName = "";
  const char *bootId = "";
  uint64_t sequence = 0;
  uint64_t uptimeMs = 0;
  uint32_t unixTime = 0;  // 0 when the clock is not synchronized

  const char *firmwareTarget = "xiao-s3";  // brooklyn-s3 | xiao-s3
  const char *firmwareVersion = "";
  const char *buildId = "";
  bool buildTypeBench = true;
  const char *activePartition = nullptr;
  OtaState ota = OtaState::Unknown;

  ResetReason reset = ResetReason::Unknown;
  bool hasBootCount = false;
  uint32_t bootCount = 0;
  uint32_t heapFreeBytes = 0;
  uint32_t heapMinFreeBytes = 0;
  bool hasPsram = false;
  uint32_t psramFreeBytes = 0;
  uint32_t psramMinFreeBytes = 0;

  bool wifiConnected = false;
  bool hasRssi = false;
  int rssiDbm = 0;
  DnsHealth dns = DnsHealth::Unknown;

  uint8_t feedSuccessCount = 0;
  uint8_t feedAttemptedCount = 0;
  bool hasFeedMask = false;
  uint8_t feedMask = 0;
  PollState poll = PollState::Unknown;
  bool hasCompleteAge = false;
  uint32_t completeAgeSeconds = 0;
  uint32_t vehiclesProcessed = 0;
  uint32_t vehiclesResolved = 0;
  uint16_t occupiedMapLeds = 0;
  bool hasCollisions = false;
  uint32_t collisionCount = 0;

  DisplayState display = DisplayState::WaitingForData;
  uint8_t logicalBrightness = 0;
  bool hasRouteMask = false;
  uint32_t routeVisibilityMask = 0;

  AppStatus appStatus = AppStatus::Unknown;
  CheckResult semanticCheck = CheckResult::Unknown;
  CheckResult ledMappingCheck = CheckResult::Unknown;

  bool hasConfigSchemaVersion = false;
  uint16_t configSchemaVersion = 0;
  Tristate configLoadValid = Tristate::Unknown;

  bool physicalOutputBuildAllowed = false;
  ControllerState controller = ControllerState::Unknown;
  ArmingState arming = ArmingState::Unknown;
  CurrentLimitState currentLimit = CurrentLimitState::Unknown;

  // Bounded x_ extensions (schema allows up to 8; four non-sensitive diagnostics are used).
  uint32_t extEventsDropped = 0;
  uint32_t extReconnects = 0;
  uint32_t extAcksMissed = 0;
  uint32_t extPublishFailures = 0;
};

// Returns the JSON length, or 0 when it does not fit in `capacity` (capacity includes the
// terminating NUL) or would exceed kMaxPayloadBytes. Never truncates.
size_t buildStatusJson(char *buffer, size_t capacity, const StatusInput &input);

struct EventRecord {
  EventType type = EventType::Boot;
  Severity severity = Severity::Info;
  uint64_t uptimeMs = 0;  // occurrence time on the monotonic clock
  ResetReason reset = ResetReason::Unknown;
  DisplayState previousDisplay = DisplayState::WaitingForData;
  DisplayState currentDisplay = DisplayState::WaitingForData;
  OtaState ota = OtaState::Unknown;
  uint32_t heapFreeBytes = 0;
  uint8_t coalescedCount = 1;
};

// device_time_utc is derived from `nowUnix` minus the time elapsed since occurrence, and
// time_synced reflects whether `nowUnix` is a trustworthy clock. Schema invariant:
// time_synced == true <=> device_time_utc is a UTC string.
size_t buildEventJson(char *buffer, size_t capacity, const char *deviceId, const char *bootId,
                      uint64_t sequence, const EventRecord &event, uint64_t nowUptimeMs,
                      uint32_t nowUnix);

EventRecord makeBootEvent(ResetReason reset, uint64_t uptimeMs);
EventRecord makeResetEvent(ResetReason reset, uint64_t uptimeMs, bool &emit);
EventRecord makeWifiReconnectedEvent(uint64_t uptimeMs);
// Maps a display transition to its schema event; returns false for transitions that are
// not significant (no change, or the initial WaitingForData -> Fresh).
bool makeDisplayEvent(DisplayState previous, DisplayState current, uint64_t uptimeMs,
                      EventRecord &out);

// ---------------------------------------------------------------------------
// Policy: backoff, cadence, readiness
// ---------------------------------------------------------------------------

// Ceiling after `streak` consecutive failures: 5 s, 10 s, 20 s ... capped at 15 minutes.
uint32_t backoffCeilingMs(uint32_t streak);
// Full jitter in [kBackoffFloorMs, ceiling] from a uniform 32-bit random value.
uint32_t backoffDelayMs(uint32_t streak, uint32_t random);
uint32_t statusIntervalMs(uint32_t random);  // [28500, 31500]
uint32_t startJitterMs(uint32_t random);     // [500, 3000]

struct Inputs {
  bool networkReady = false;  // NetworkService state == Ready (Wi-Fi + valid SNTP)
  bool wifiUp = false;        // live association
  bool timeValid = false;     // live clock >= kMinValidUnixTime
  bool credentials = false;   // identity and broker credential compiled/provisioned
};
inline bool connectionAllowed(const Inputs &in) {
  return in.networkReady && in.wifiUp && in.timeValid && in.credentials;
}

// ---------------------------------------------------------------------------
// Core state machine
// ---------------------------------------------------------------------------

enum class Phase : uint8_t { Idle, Connecting, Connected };
enum class ActionType : uint8_t { None, StartClient, StopClient, PublishOnline, PublishStatus, PublishEvent };
enum class PublishResult : uint8_t { Queued, Failed, TooLarge };
enum class Admit : uint8_t { Queued, QueuedEvictedOldest, Coalesced, Suppressed, Rejected };

struct Action {
  ActionType type = ActionType::None;
  uint64_t sequence = 0;  // PublishStatus / PublishEvent: logical message identity
  EventRecord event{};    // PublishEvent: the record to serialize
};

struct Counters {
  uint32_t connectAttempts = 0;
  uint32_t connects = 0;
  uint32_t reconnects = 0;
  uint32_t disconnects = 0;
  uint32_t stableConnections = 0;
  uint32_t onlineSent = 0;
  uint32_t statusSent = 0;
  uint32_t statusCoalesced = 0;
  uint32_t eventsQueued = 0;
  uint32_t eventsSent = 0;
  uint32_t eventsDropped = 0;
  uint32_t eventsCoalesced = 0;
  uint32_t publishesAcked = 0;
  uint32_t acksMissed = 0;
  uint32_t unmatchedAcks = 0;
  uint32_t publishFailures = 0;
  uint32_t payloadRejected = 0;
  uint32_t sequenceWraps = 0;
  uint32_t lastAckLatencyMs = 0;
  uint32_t maxAckLatencyMs = 0;
};

class Core {
 public:
  using RandomFn = uint32_t (*)(void *context);

  void begin(const char *bootId, RandomFn random, void *randomContext);

  // Asks for at most one action; call repeatedly until it returns None. The caller reports
  // each action's outcome through the methods below before asking again.
  Action next(uint64_t nowMs, const Inputs &inputs);

  void onClientStarted(bool ok, uint64_t nowMs);
  void onClientStopped(uint64_t nowMs);
  void onConnected(uint64_t nowMs);
  void onDisconnected(uint64_t nowMs);
  void onPublished(int32_t messageId, uint64_t nowMs);
  void publishResult(const Action &action, PublishResult result, int32_t messageId, uint64_t nowMs);

  Admit enqueueEvent(EventRecord event, uint64_t nowMs);

  const char *bootId() const { return bootId_; }
  Phase phase() const { return phase_; }
  bool stable() const { return stable_; }
  uint32_t failureStreak() const { return streak_; }
  uint64_t lastSequence() const { return sequence_; }
  size_t queueSize() const { return count_; }
  uint64_t queuedSequence(size_t index) const { return items_[index].sequence; }
  const EventRecord &queuedEvent(size_t index) const { return items_[index].event; }
  size_t inFlightCount() const { return inFlightCount_; }
  uint64_t nextConnectAtMs() const { return nextConnectMs_; }
  uint64_t nextStatusAtMs() const { return nextStatusMs_; }
  const Counters &counters() const { return counters_; }

  // Test hook: forces the sequence counter to exercise wraparound.
  void setSequenceForTest(uint64_t value) { sequence_ = value; }

 private:
  enum class Kind : uint8_t { Online, Status, Event };
  struct Item {
    EventRecord event;
    uint64_t sequence;
    bool inFlight;
  };
  struct InFlight {
    bool used;
    Kind kind;
    int32_t messageId;
    uint64_t sentMs;
    uint64_t sequence;
  };
  struct LastAdmit {
    bool valid;
    uint64_t atMs;
    DisplayState previous;
    DisplayState current;
  };

  uint32_t rand32() { return random_ != nullptr ? random_(randomContext_) : 0u; }
  uint64_t allocSequence();
  void addInFlight(Kind kind, int32_t messageId, uint64_t nowMs, uint64_t sequence);
  void removeItemBySequence(uint64_t sequence);
  void evictForSpace();
  bool eventSendAllowed(const Item &item, uint64_t nowMs) const;
  void recordEventSend(uint64_t nowMs);
  void markDead() { clientDead_ = true; stopIsFailure_ = true; }

  char bootId_[37] = {0};
  RandomFn random_ = nullptr;
  void *randomContext_ = nullptr;
  uint64_t sequence_ = 0;

  Phase phase_ = Phase::Idle;
  uint64_t phaseSinceMs_ = 0;
  bool gateOpen_ = false;
  bool clientDead_ = false;
  bool stopIsFailure_ = false;
  bool everConnected_ = false;
  bool stable_ = false;
  bool onlineSent_ = false;
  bool onlineAcked_ = false;
  bool statusSent_ = false;
  bool statusAcked_ = false;
  bool statusDeferredCounted_ = false;
  uint32_t streak_ = 0;
  uint64_t nextConnectMs_ = 0;
  uint64_t nextStatusMs_ = 0;

  Item items_[kEventQueueSlots] = {};
  size_t count_ = 0;
  LastAdmit lastAdmit_[static_cast<size_t>(EventType::Count)] = {};
  InFlight inFlight_[kMaxInFlight] = {};
  size_t inFlightCount_ = 0;

  bool anyEventSent_ = false;
  uint64_t lastEventSentMs_ = 0;
  uint64_t sentAt_[kEventsPerMinute] = {};
  size_t sentHead_ = 0;
  size_t sentCount_ = 0;

  Counters counters_{};
};

}  // namespace brooklyn::telemetry
