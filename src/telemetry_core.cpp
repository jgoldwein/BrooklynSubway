// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4AA: host-testable telemetry core. See telemetry_core.h.

#include "telemetry_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace brooklyn::telemetry {
namespace {

// Bounded JSON writer: never writes past `capacity`, and any overflow poisons the result.
class Writer {
 public:
  Writer(char *buffer, size_t capacity) : buf_(buffer), cap_(capacity) {
    if (buf_ != nullptr && cap_ > 0) buf_[0] = '\0';
  }
  bool ok() const { return ok_ && buf_ != nullptr && cap_ > 0; }
  size_t size() const { return pos_; }

  void raw(const char *text) {
    const size_t n = strlen(text);
    if (!ok_ || pos_ + n + 1 > cap_) {
      ok_ = false;
      return;
    }
    memcpy(buf_ + pos_, text, n + 1);
    pos_ += n;
  }
  void fmt(const char *format, ...) __attribute__((format(printf, 2, 3))) {
    if (!ok_) return;
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(buf_ + pos_, cap_ - pos_, format, args);
    va_end(args);
    if (n < 0 || static_cast<size_t>(n) + 1 > cap_ - pos_) {
      ok_ = false;
      return;
    }
    pos_ += static_cast<size_t>(n);
  }
  // JSON string with escaping; only printable ASCII passes through unescaped.
  void str(const char *text) {
    raw("\"");
    for (const char *p = text != nullptr ? text : ""; *p != '\0' && ok_; ++p) {
      const unsigned char c = static_cast<unsigned char>(*p);
      if (c == '"' || c == '\\') {
        char e[3] = {'\\', static_cast<char>(c), 0};
        raw(e);
      } else if (c < 0x20 || c >= 0x7f) {
        fmt("\\u%04x", static_cast<unsigned>(c));
      } else {
        char e[2] = {static_cast<char>(c), 0};
        raw(e);
      }
    }
    raw("\"");
  }
  void boolean(bool v) { raw(v ? "true" : "false"); }

 private:
  char *buf_;
  size_t cap_;
  size_t pos_ = 0;
  bool ok_ = true;
};

const char *dnsName(DnsHealth v) {
  switch (v) {
    case DnsHealth::Ok: return "ok";
    case DnsHealth::Failed: return "failed";
    case DnsHealth::NotTested: return "not_tested";
    default: return "unknown";
  }
}
const char *pollName(PollState v) {
  switch (v) {
    case PollState::Idle: return "idle";
    case PollState::Polling: return "polling";
    case PollState::Complete: return "complete";
    case PollState::Partial: return "partial";
    case PollState::NoFeeds: return "no_feeds";
    default: return "unknown";
  }
}
const char *otaName(OtaState v) {
  switch (v) {
    case OtaState::Idle: return "idle";
    case OtaState::PendingVerification: return "pending_verification";
    case OtaState::Confirmed: return "confirmed";
    case OtaState::Rollback: return "rollback";
    default: return "unknown";
  }
}
const char *appName(AppStatus v) {
  switch (v) {
    case AppStatus::ReportedOk: return "reported_ok";
    case AppStatus::ReportedDegraded: return "reported_degraded";
    default: return "unknown";
  }
}
const char *checkName(CheckResult v) {
  switch (v) {
    case CheckResult::Pass: return "pass";
    case CheckResult::Fail: return "fail";
    case CheckResult::Unsupported: return "unsupported";
    default: return "unknown";
  }
}
const char *controllerName(ControllerState v) {
  switch (v) {
    case ControllerState::NotPresent: return "not_present";
    case ControllerState::Disabled: return "disabled";
    case ControllerState::Safe: return "safe";
    case ControllerState::Armed: return "armed";
    case ControllerState::Fault: return "fault";
    case ControllerState::Unsupported: return "unsupported";
    default: return "unknown";
  }
}
const char *armingName(ArmingState v) {
  switch (v) {
    case ArmingState::NotPresent: return "not_present";
    case ArmingState::Disarmed: return "disarmed";
    case ArmingState::Armed: return "armed";
    case ArmingState::Fault: return "fault";
    case ArmingState::Unsupported: return "unsupported";
    default: return "unknown";
  }
}
const char *currentName(CurrentLimitState v) {
  switch (v) {
    case CurrentLimitState::NotPresent: return "not_present";
    case CurrentLimitState::WithinLimit: return "within_limit";
    case CurrentLimitState::OverLimit: return "over_limit";
    case CurrentLimitState::Unsupported: return "unsupported";
    default: return "unknown";
  }
}

void writeTime(Writer &w, uint32_t unixTime) {
  if (timeIsValid(unixTime)) {
    char utc[21];
    formatUtc(unixTime, utc);
    w.raw("\"time_synced\":true,\"device_time_utc\":");
    w.str(utc);
  } else {
    w.raw("\"time_synced\":false,\"device_time_utc\":null");
  }
}

uint32_t scaled(uint32_t random, uint32_t lo, uint32_t hi) {
  return lo + static_cast<uint32_t>((static_cast<uint64_t>(random) * (hi - lo + 1ull)) >> 32);
}

}  // namespace

const char *resetReasonName(ResetReason v) {
  switch (v) {
    case ResetReason::PowerOn: return "power_on";
    case ResetReason::Software: return "software";
    case ResetReason::Panic: return "panic";
    case ResetReason::Watchdog: return "watchdog";
    case ResetReason::Brownout: return "brownout";
    case ResetReason::DeepSleep: return "deep_sleep";
    case ResetReason::Other: return "other";
    default: return "unknown";
  }
}

const char *eventTypeName(EventType t) {
  switch (t) {
    case EventType::Boot: return "boot";
    case EventType::MonitoringEnabled: return "monitoring_enabled";
    case EventType::EnrollmentCompleted: return "enrollment_completed";
    case EventType::WifiReconnected: return "wifi_reconnected";
    case EventType::DisplayDegraded: return "display_degraded";
    case EventType::DisplayRecovered: return "display_recovered";
    case EventType::DisplayStale: return "display_stale";
    case EventType::DisplayNoData: return "display_no_data";
    case EventType::OtaPendingVerification: return "ota_pending_verification";
    case EventType::OtaConfirmed: return "ota_confirmed";
    case EventType::OtaRollback: return "ota_rollback";
    case EventType::UnexpectedReset: return "unexpected_reset";
    case EventType::Brownout: return "brownout";
    case EventType::WatchdogReset: return "watchdog_reset";
    case EventType::ConfigurationInvalid: return "configuration_invalid";
    case EventType::HeapCritical: return "heap_critical";
    case EventType::PhysicalSafetyFault: return "physical_safety_fault";
    default: return "boot";
  }
}

const char *severityName(Severity s) {
  switch (s) {
    case Severity::Warning: return "warning";
    case Severity::Critical: return "critical";
    default: return "info";
  }
}

const char *displayStateName(DisplayState s) {
  switch (s) {
    case DisplayState::WaitingForData: return "WaitingForData";
    case DisplayState::Fresh: return "Fresh";
    case DisplayState::DegradedHold: return "DegradedHold";
    case DisplayState::DegradedPartial: return "DegradedPartial";
    case DisplayState::StaleHold: return "StaleHold";
    default: return "NoData";
  }
}

void formatUuid4(const uint8_t random[16], char out[37]) {
  uint8_t b[16];
  memcpy(b, random, 16);
  b[6] = static_cast<uint8_t>((b[6] & 0x0f) | 0x40);
  b[8] = static_cast<uint8_t>((b[8] & 0x3f) | 0x80);
  static const char hex[] = "0123456789abcdef";
  size_t o = 0;
  for (size_t i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) out[o++] = '-';
    out[o++] = hex[b[i] >> 4];
    out[o++] = hex[b[i] & 0x0f];
  }
  out[o] = '\0';
}

void formatUtc(uint32_t unixTime, char out[21]) {
  const uint32_t days = unixTime / 86400u;
  const uint32_t secs = unixTime % 86400u;
  // Howard Hinnant's civil_from_days.
  const int64_t z = static_cast<int64_t>(days) + 719468;
  const int64_t era = z / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  const int64_t d = doy - (153 * mp + 2) / 5 + 1;
  const int64_t m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) ++y;
  snprintf(out, 21, "%04d-%02d-%02dT%02u:%02u:%02uZ", static_cast<int>(y), static_cast<int>(m),
           static_cast<int>(d), static_cast<unsigned>(secs / 3600u),
           static_cast<unsigned>((secs / 60u) % 60u), static_cast<unsigned>(secs % 60u));
}

size_t buildStatusJson(char *buffer, size_t capacity, const StatusInput &in) {
  const size_t cap = capacity < kMaxPayloadBytes + 1 ? capacity : kMaxPayloadBytes + 1;
  Writer w(buffer, cap);
  w.raw("{\"protocol\":\"brooklyn-telemetry\",\"version\":1,\"kind\":\"heartbeat\",\"device_id\":");
  w.str(in.deviceId);
  w.raw(",\"device_name\":");
  w.str(in.deviceName);
  w.raw(",\"boot_id\":");
  w.str(in.bootId);
  w.fmt(",\"sequence\":%llu,\"uptime_ms\":%llu,", static_cast<unsigned long long>(in.sequence),
        static_cast<unsigned long long>(in.uptimeMs));
  writeTime(w, in.unixTime);

  w.raw(",\"firmware\":{\"target\":");
  w.str(in.firmwareTarget);
  w.raw(",\"version\":");
  w.str(in.firmwareVersion);
  w.raw(",\"build_id\":");
  w.str(in.buildId);
  w.raw(",\"build_type\":");
  w.str(in.buildTypeBench ? "bench" : "production");
  w.raw(",\"image_sha256\":null,\"active_partition\":");
  if (in.activePartition != nullptr) w.str(in.activePartition); else w.raw("null");
  w.fmt(",\"ota_state\":\"%s\"}", otaName(in.ota));

  w.fmt(",\"system\":{\"reset_reason\":\"%s\",\"boot_count\":", resetReasonName(in.reset));
  if (in.hasBootCount) w.fmt("%lu", static_cast<unsigned long>(in.bootCount)); else w.raw("null");
  w.fmt(",\"heap_free_bytes\":%lu,\"heap_min_free_bytes\":%lu,\"psram_free_bytes\":",
        static_cast<unsigned long>(in.heapFreeBytes), static_cast<unsigned long>(in.heapMinFreeBytes));
  if (in.hasPsram) w.fmt("%lu", static_cast<unsigned long>(in.psramFreeBytes)); else w.raw("null");
  w.raw(",\"psram_min_free_bytes\":");
  if (in.hasPsram) w.fmt("%lu", static_cast<unsigned long>(in.psramMinFreeBytes)); else w.raw("null");
  w.raw("}");

  w.raw(",\"network\":{\"wifi_connected\":");
  w.boolean(in.wifiConnected);
  w.raw(",\"wifi_rssi_dbm\":");
  if (in.hasRssi) w.fmt("%d", in.rssiDbm); else w.raw("null");
  w.fmt(",\"dns_health\":\"%s\"}", dnsName(in.dns));

  w.fmt(",\"mta\":{\"feed_success_count\":%u,\"feed_attempted_count\":%u,\"feed_success_mask\":",
        static_cast<unsigned>(in.feedSuccessCount), static_cast<unsigned>(in.feedAttemptedCount));
  if (in.hasFeedMask) w.fmt("%u", static_cast<unsigned>(in.feedMask)); else w.raw("null");
  w.fmt(",\"poll_state\":\"%s\",\"last_complete_age_s\":", pollName(in.poll));
  if (in.hasCompleteAge) w.fmt("%lu", static_cast<unsigned long>(in.completeAgeSeconds)); else w.raw("null");
  w.fmt(",\"vehicles_processed\":%lu,\"vehicles_resolved\":%lu,\"occupied_map_led_count\":%u,"
        "\"collision_count\":",
        static_cast<unsigned long>(in.vehiclesProcessed), static_cast<unsigned long>(in.vehiclesResolved),
        static_cast<unsigned>(in.occupiedMapLeds));
  if (in.hasCollisions) w.fmt("%lu", static_cast<unsigned long>(in.collisionCount)); else w.raw("null");
  w.raw("}");

  w.fmt(",\"display\":{\"state\":\"%s\",\"logical_brightness\":%u,\"route_visibility_mask\":",
        displayStateName(in.display), static_cast<unsigned>(in.logicalBrightness));
  if (in.hasRouteMask) w.fmt("%lu", static_cast<unsigned long>(in.routeVisibilityMask)); else w.raw("null");
  w.raw("}");

  w.fmt(",\"integrity\":{\"application_reported_status\":\"%s\",\"semantic_check\":\"%s\","
        "\"led_mapping_check\":\"%s\"}",
        appName(in.appStatus), checkName(in.semanticCheck), checkName(in.ledMappingCheck));

  w.raw(",\"configuration\":{\"schema_version\":");
  if (in.hasConfigSchemaVersion) w.fmt("%u", static_cast<unsigned>(in.configSchemaVersion)); else w.raw("null");
  w.raw(",\"load_valid\":");
  if (in.configLoadValid == Tristate::Unknown) w.raw("null"); else w.boolean(in.configLoadValid == Tristate::True);
  w.raw("}");

  w.raw(",\"safety\":{\"physical_output_build_allowed\":");
  w.boolean(in.physicalOutputBuildAllowed);
  w.fmt(",\"controller_state\":\"%s\",\"arming_state\":\"%s\",\"current_limit_state\":\"%s\"}",
        controllerName(in.controller), armingName(in.arming), currentName(in.currentLimit));

  w.fmt(",\"extensions\":{\"x_ev_dropped\":%lu,\"x_reconnects\":%lu,\"x_acks_missed\":%lu,"
        "\"x_pub_failures\":%lu}}",
        static_cast<unsigned long>(in.extEventsDropped), static_cast<unsigned long>(in.extReconnects),
        static_cast<unsigned long>(in.extAcksMissed), static_cast<unsigned long>(in.extPublishFailures));
  return w.ok() ? w.size() : 0;
}

size_t buildEventJson(char *buffer, size_t capacity, const char *deviceId, const char *bootId,
                      uint64_t sequence, const EventRecord &e, uint64_t nowUptimeMs,
                      uint32_t nowUnix) {
  const size_t cap = capacity < kMaxPayloadBytes + 1 ? capacity : kMaxPayloadBytes + 1;
  Writer w(buffer, cap);
  w.raw("{\"protocol\":\"brooklyn-telemetry\",\"version\":1,\"kind\":\"event\",\"device_id\":");
  w.str(deviceId);
  w.raw(",\"boot_id\":");
  w.str(bootId);
  w.fmt(",\"sequence\":%llu,\"uptime_ms\":%llu,", static_cast<unsigned long long>(sequence),
        static_cast<unsigned long long>(e.uptimeMs));
  // Occurrence time = now - age; only trustworthy when the clock is valid and the result is too.
  uint32_t eventUnix = 0;
  if (timeIsValid(nowUnix) && nowUptimeMs >= e.uptimeMs) {
    const uint64_t ageS = (nowUptimeMs - e.uptimeMs) / 1000u;
    if (ageS < nowUnix && timeIsValid(static_cast<uint32_t>(nowUnix - ageS))) {
      eventUnix = static_cast<uint32_t>(nowUnix - ageS);
    }
  }
  writeTime(w, eventUnix);
  w.fmt(",\"event_type\":\"%s\",\"severity\":\"%s\",\"attributes\":{", eventTypeName(e.type),
        severityName(e.severity));

  bool first = true;
  auto comma = [&]() {
    if (!first) w.raw(",");
    first = false;
  };
  switch (e.type) {
    case EventType::Boot:
    case EventType::UnexpectedReset:
    case EventType::Brownout:
    case EventType::WatchdogReset:
      comma();
      w.fmt("\"reset_reason\":\"%s\"", resetReasonName(e.reset));
      break;
    case EventType::DisplayDegraded:
    case EventType::DisplayRecovered:
    case EventType::DisplayStale:
    case EventType::DisplayNoData:
      comma();
      w.fmt("\"previous_display_state\":\"%s\",\"current_display_state\":\"%s\"",
            displayStateName(e.previousDisplay), displayStateName(e.currentDisplay));
      break;
    case EventType::OtaPendingVerification:
    case EventType::OtaConfirmed:
    case EventType::OtaRollback:
      comma();
      w.fmt("\"ota_state\":\"%s\"", otaName(e.ota));
      break;
    case EventType::ConfigurationInvalid:
      comma();
      w.raw("\"configuration_valid\":false");
      break;
    case EventType::HeapCritical:
      comma();
      w.fmt("\"heap_free_bytes\":%lu", static_cast<unsigned long>(e.heapFreeBytes));
      break;
    case EventType::PhysicalSafetyFault:
      comma();
      w.raw("\"safety_state\":\"fault\"");
      break;
    default:
      break;
  }
  if (e.coalescedCount > 1) {
    comma();
    w.fmt("\"coalesced_transition_count\":%u", static_cast<unsigned>(e.coalescedCount));
  }
  w.raw("}}");
  return w.ok() ? w.size() : 0;
}

EventRecord makeBootEvent(ResetReason reset, uint64_t uptimeMs) {
  EventRecord e;
  e.type = EventType::Boot;
  e.severity = Severity::Info;
  e.uptimeMs = uptimeMs;
  e.reset = reset;
  return e;
}

EventRecord makeResetEvent(ResetReason reset, uint64_t uptimeMs, bool &emit) {
  EventRecord e;
  e.uptimeMs = uptimeMs;
  e.reset = reset;
  e.severity = Severity::Critical;
  emit = true;
  switch (reset) {
    case ResetReason::Panic: e.type = EventType::UnexpectedReset; break;
    case ResetReason::Watchdog: e.type = EventType::WatchdogReset; break;
    case ResetReason::Brownout: e.type = EventType::Brownout; break;
    default: emit = false; break;
  }
  return e;
}

EventRecord makeWifiReconnectedEvent(uint64_t uptimeMs) {
  EventRecord e;
  e.type = EventType::WifiReconnected;
  e.severity = Severity::Info;
  e.uptimeMs = uptimeMs;
  return e;
}

bool makeDisplayEvent(DisplayState previous, DisplayState current, uint64_t uptimeMs,
                      EventRecord &out) {
  if (previous == current) return false;
  EventRecord e;
  e.uptimeMs = uptimeMs;
  e.previousDisplay = previous;
  e.currentDisplay = current;
  switch (current) {
    case DisplayState::Fresh:
      if (previous == DisplayState::WaitingForData) return false;
      e.type = EventType::DisplayRecovered;
      e.severity = Severity::Info;
      break;
    case DisplayState::DegradedHold:
    case DisplayState::DegradedPartial:
      e.type = EventType::DisplayDegraded;
      e.severity = Severity::Warning;
      break;
    case DisplayState::StaleHold:
      e.type = EventType::DisplayStale;
      e.severity = Severity::Warning;
      break;
    case DisplayState::NoData:
      e.type = EventType::DisplayNoData;
      e.severity = Severity::Warning;
      break;
    default:
      return false;
  }
  out = e;
  return true;
}

uint32_t backoffCeilingMs(uint32_t streak) {
  uint64_t ceiling = kBackoffInitialCeilingMs;
  for (uint32_t i = 1; i < streak && ceiling < kBackoffMaxMs; ++i) ceiling *= 2;
  return ceiling > kBackoffMaxMs ? kBackoffMaxMs : static_cast<uint32_t>(ceiling);
}

uint32_t backoffDelayMs(uint32_t streak, uint32_t random) {
  return scaled(random, kBackoffFloorMs, backoffCeilingMs(streak));
}

uint32_t statusIntervalMs(uint32_t random) {
  return scaled(random, kStatusIntervalMs - kStatusJitterMs / 2, kStatusIntervalMs + kStatusJitterMs / 2);
}

uint32_t startJitterMs(uint32_t random) { return scaled(random, kStartJitterMinMs, kStartJitterMaxMs); }

// ---------------------------------------------------------------------------
// Core
// ---------------------------------------------------------------------------

void Core::begin(const char *bootId, RandomFn random, void *randomContext) {
  *this = Core();
  strncpy(bootId_, bootId != nullptr ? bootId : "", sizeof(bootId_) - 1);
  random_ = random;
  randomContext_ = randomContext;
}

uint64_t Core::allocSequence() {
  if (sequence_ >= kMaxSequence) {
    sequence_ = 0;
    ++counters_.sequenceWraps;
  }
  return ++sequence_;
}

void Core::addInFlight(Kind kind, int32_t messageId, uint64_t nowMs, uint64_t sequence) {
  for (InFlight &f : inFlight_) {
    if (!f.used) {
      f = {true, kind, messageId, nowMs, sequence};
      ++inFlightCount_;
      return;
    }
  }
}

void Core::removeItemBySequence(uint64_t sequence) {
  for (size_t i = 0; i < count_; ++i) {
    if (items_[i].sequence == sequence) {
      for (size_t j = i + 1; j < count_; ++j) items_[j - 1] = items_[j];
      --count_;
      return;
    }
  }
}

void Core::evictForSpace() {
  size_t victim = count_;
  for (size_t i = 0; i < count_ && victim == count_; ++i) {
    if (!items_[i].inFlight && items_[i].event.severity != Severity::Critical) victim = i;
  }
  for (size_t i = 0; i < count_ && victim == count_; ++i) {
    if (!items_[i].inFlight) victim = i;
  }
  if (victim == count_) victim = 0;
  removeItemBySequence(items_[victim].sequence);
  ++counters_.eventsDropped;
}

Admit Core::enqueueEvent(EventRecord event, uint64_t nowMs) {
  const size_t t = static_cast<size_t>(event.type);
  if (t >= static_cast<size_t>(EventType::Count)) return Admit::Rejected;
  if (event.coalescedCount == 0) event.coalescedCount = 1;

  LastAdmit &last = lastAdmit_[t];
  if (last.valid && nowMs - last.atMs < kEventCoalesceWindowMs) {
    for (size_t i = 0; i < count_; ++i) {
      Item &item = items_[i];
      if (item.event.type == event.type && !item.inFlight) {
        if (item.event.coalescedCount < 255) ++item.event.coalescedCount;
        item.event.currentDisplay = event.currentDisplay;
        ++counters_.eventsCoalesced;
        return Admit::Coalesced;
      }
    }
  }

  Admit result = Admit::Queued;
  if (count_ >= kEventQueueSlots) {
    evictForSpace();
    result = Admit::QueuedEvictedOldest;
  }
  items_[count_++] = {event, allocSequence(), false};
  last = {true, nowMs, event.previousDisplay, event.currentDisplay};
  ++counters_.eventsQueued;
  return result;
}

bool Core::eventSendAllowed(const Item &item, uint64_t nowMs) const {
  size_t recent = 0;
  for (size_t i = 0; i < sentCount_; ++i) {
    if (nowMs - sentAt_[i] < 60000u) ++recent;
  }
  if (recent >= kEventsPerMinute) return false;
  if (item.event.severity == Severity::Critical) return true;
  return !anyEventSent_ || nowMs - lastEventSentMs_ >= kEventSpacingMs;
}

void Core::recordEventSend(uint64_t nowMs) {
  anyEventSent_ = true;
  lastEventSentMs_ = nowMs;
  const size_t slot = (sentHead_ + sentCount_) % kEventsPerMinute;
  if (sentCount_ == kEventsPerMinute) {
    sentAt_[sentHead_] = nowMs;
    sentHead_ = (sentHead_ + 1) % kEventsPerMinute;
  } else {
    sentAt_[slot] = nowMs;
    ++sentCount_;
  }
}

Action Core::next(uint64_t nowMs, const Inputs &inputs) {
  Action none;
  const bool gate = connectionAllowed(inputs);

  if (!gate) {
    gateOpen_ = false;
    if (phase_ != Phase::Idle) {
      Action a;
      a.type = ActionType::StopClient;
      return a;
    }
    return none;
  }
  if (!gateOpen_) {
    gateOpen_ = true;
    const uint64_t candidate = nowMs + startJitterMs(rand32());
    if (nextConnectMs_ < candidate) nextConnectMs_ = candidate;
  }

  Action a;
  switch (phase_) {
    case Phase::Idle:
      if (nowMs < nextConnectMs_) return none;
      ++counters_.connectAttempts;
      phase_ = Phase::Connecting;
      phaseSinceMs_ = nowMs;
      clientDead_ = false;
      stopIsFailure_ = false;
      a.type = ActionType::StartClient;
      return a;

    case Phase::Connecting:
      if (clientDead_ || nowMs - phaseSinceMs_ >= kConnectTimeoutMs) {
        markDead();
        a.type = ActionType::StopClient;
        return a;
      }
      return none;

    case Phase::Connected:
      break;
  }

  if (clientDead_) {
    a.type = ActionType::StopClient;
    return a;
  }
  for (const InFlight &f : inFlight_) {
    if (f.used && nowMs - f.sentMs >= kAckTimeoutMs) {
      ++counters_.acksMissed;
      markDead();
      a.type = ActionType::StopClient;
      return a;
    }
  }
  if (inFlightCount_ >= kMaxInFlight) return none;

  if (!onlineSent_) {
    a.type = ActionType::PublishOnline;
    return a;
  }
  if (!statusSent_) {
    a.type = ActionType::PublishStatus;
    a.sequence = allocSequence();
    return a;
  }
  if (nowMs >= nextStatusMs_) {
    bool statusPending = false;
    for (const InFlight &f : inFlight_) statusPending = statusPending || (f.used && f.kind == Kind::Status);
    if (!statusPending) {
      a.type = ActionType::PublishStatus;
      a.sequence = allocSequence();
      return a;
    }
    if (!statusDeferredCounted_) {
      statusDeferredCounted_ = true;
      ++counters_.statusCoalesced;
    }
  }
  for (size_t i = 0; i < count_; ++i) {
    if (!items_[i].inFlight && eventSendAllowed(items_[i], nowMs)) {
      a.type = ActionType::PublishEvent;
      a.sequence = items_[i].sequence;
      a.event = items_[i].event;
      return a;
    }
  }
  return none;
}

void Core::onClientStarted(bool ok, uint64_t nowMs) {
  if (ok) return;
  markDead();
  onClientStopped(nowMs);
}

void Core::onClientStopped(uint64_t nowMs) {
  if (stopIsFailure_ || clientDead_) {
    streak_ = stable_ ? 1u : streak_ + 1u;
    nextConnectMs_ = nowMs + backoffDelayMs(streak_, rand32());
  }
  stable_ = false;
  phase_ = Phase::Idle;
  phaseSinceMs_ = nowMs;
  clientDead_ = false;
  stopIsFailure_ = false;
  onlineSent_ = onlineAcked_ = statusSent_ = statusAcked_ = statusDeferredCounted_ = false;
  for (InFlight &f : inFlight_) f.used = false;
  inFlightCount_ = 0;
  for (size_t i = 0; i < count_; ++i) items_[i].inFlight = false;
}

void Core::onConnected(uint64_t nowMs) {
  if (phase_ != Phase::Connecting) return;
  phase_ = Phase::Connected;
  phaseSinceMs_ = nowMs;
  ++counters_.connects;
  if (everConnected_) ++counters_.reconnects;
  everConnected_ = true;
  nextStatusMs_ = nowMs;
}

void Core::onDisconnected(uint64_t nowMs) {
  (void)nowMs;
  if (phase_ == Phase::Idle || clientDead_) return;
  ++counters_.disconnects;
  markDead();
}

void Core::onPublished(int32_t messageId, uint64_t nowMs) {
  for (InFlight &f : inFlight_) {
    if (!f.used || f.messageId != messageId) continue;
    const uint32_t latency = static_cast<uint32_t>(nowMs - f.sentMs);
    counters_.lastAckLatencyMs = latency;
    if (latency > counters_.maxAckLatencyMs) counters_.maxAckLatencyMs = latency;
    ++counters_.publishesAcked;
    f.used = false;
    --inFlightCount_;
    if (f.kind == Kind::Online) onlineAcked_ = true;
    if (f.kind == Kind::Status) statusAcked_ = true;
    if (f.kind == Kind::Event) removeItemBySequence(f.sequence);
    if (onlineAcked_ && statusAcked_ && !stable_ && phase_ == Phase::Connected) {
      stable_ = true;
      streak_ = 0;
      ++counters_.stableConnections;
    }
    return;
  }
  ++counters_.unmatchedAcks;
}

void Core::publishResult(const Action &action, PublishResult result, int32_t messageId,
                         uint64_t nowMs) {
  Kind kind = Kind::Online;
  switch (action.type) {
    case ActionType::PublishOnline: kind = Kind::Online; break;
    case ActionType::PublishStatus: kind = Kind::Status; break;
    case ActionType::PublishEvent: kind = Kind::Event; break;
    default: return;
  }
  if (result == PublishResult::TooLarge) {
    ++counters_.payloadRejected;
    if (kind == Kind::Event) removeItemBySequence(action.sequence);
    if (kind == Kind::Status) {
      statusSent_ = true;
      nextStatusMs_ = nowMs + statusIntervalMs(rand32());
    }
    return;
  }
  if (result == PublishResult::Failed || messageId <= 0) {
    ++counters_.publishFailures;
    markDead();
    return;
  }
  addInFlight(kind, messageId, nowMs, action.sequence);
  switch (kind) {
    case Kind::Online:
      onlineSent_ = true;
      ++counters_.onlineSent;
      break;
    case Kind::Status:
      statusSent_ = true;
      statusDeferredCounted_ = false;
      ++counters_.statusSent;
      nextStatusMs_ = nowMs + statusIntervalMs(rand32());
      break;
    case Kind::Event:
      for (size_t i = 0; i < count_; ++i) {
        if (items_[i].sequence == action.sequence) items_[i].inFlight = true;
      }
      recordEventSend(nowMs);
      ++counters_.eventsSent;
      break;
  }
}

}  // namespace brooklyn::telemetry
