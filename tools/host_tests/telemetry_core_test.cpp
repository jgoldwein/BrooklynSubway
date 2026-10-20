// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host tests for the Phase 4AA telemetry core. Emits "SAMPLE <kind> <json>" lines that
// tools/run_telemetry_core_tests.py validates against the frozen JSON schemas.

#include <stdio.h>
#include <string.h>

#include "telemetry_core.h"

using namespace brooklyn::telemetry;

namespace {

// Minimal string: the host toolchain has no C++ standard library headers.
struct Str {
  char text[96] = {0};
  size_t len = 0;
  void operator+=(char c) {
    if (len + 1 < sizeof(text)) {
      text[len++] = c;
      text[len] = '\0';
    }
  }
  bool empty() const { return len == 0; }
  size_t size() const { return len; }
  bool operator==(const char *other) const { return strcmp(text, other) == 0; }
};
using string = Str;

int gChecks = 0;
int gFailures = 0;

#define CHECK(cond)                                                      \
  do {                                                                   \
    ++gChecks;                                                           \
    if (!(cond)) {                                                       \
      ++gFailures;                                                       \
      printf("FAIL line %d: %s\n", __LINE__, #cond);                     \
    }                                                                    \
  } while (0)

struct Rng {
  uint32_t state = 12345;
  uint32_t fixed = 0;
  bool useFixed = false;
};
uint32_t rngNext(void *ctx) {
  Rng *r = static_cast<Rng *>(ctx);
  if (r->useFixed) return r->fixed;
  r->state = r->state * 1664525u + 1013904223u;
  return r->state;
}

const char *kBoot = "0713e324-1111-4222-8333-444455556666";
const char *kDevice = "00000000-0000-4000-8000-000000000b01";
const Inputs kReady{true, true, true, true};

void sample(const char *kind, const char *json) { printf("SAMPLE %s %s\n", kind, json); }

StatusInput fullStatus() {
  StatusInput s;
  s.deviceId = kDevice;
  s.deviceName = "Brooklyn Map";
  s.bootId = kBoot;
  s.sequence = 5;
  s.uptimeMs = 123456;
  s.unixTime = 1780000000;
  s.firmwareTarget = "xiao-s3";
  s.firmwareVersion = "0.4.0";
  s.buildId = "abcdef1";
  s.activePartition = "app0";
  s.ota = OtaState::Idle;
  s.reset = ResetReason::PowerOn;
  s.heapFreeBytes = 180000;
  s.heapMinFreeBytes = 120000;
  s.hasPsram = true;
  s.psramFreeBytes = 7800000;
  s.psramMinFreeBytes = 7700000;
  s.wifiConnected = true;
  s.hasRssi = true;
  s.rssiDbm = -61;
  s.dns = DnsHealth::Unknown;
  s.feedSuccessCount = 7;
  s.feedAttemptedCount = 8;
  s.hasFeedMask = true;
  s.feedMask = 0x7f;
  s.poll = PollState::Partial;
  s.hasCompleteAge = true;
  s.completeAgeSeconds = 40;
  s.vehiclesProcessed = 300;
  s.vehiclesResolved = 250;
  s.occupiedMapLeds = 120;
  s.display = DisplayState::Fresh;
  s.logicalBrightness = 32;
  s.appStatus = AppStatus::ReportedOk;
  s.physicalOutputBuildAllowed = false;
  s.controller = ControllerState::Disabled;
  return s;
}

void testSerialization() {
  char buf[kMaxPayloadBytes + 1];
  StatusInput s = fullStatus();
  size_t n = buildStatusJson(buf, sizeof(buf), s);
  CHECK(n > 0 && n == strlen(buf) && n < 2400);
  sample("status", buf);
  CHECK(strstr(buf, "\"time_synced\":true,\"device_time_utc\":\"") != nullptr);

  s.unixTime = 0;
  n = buildStatusJson(buf, sizeof(buf), s);
  CHECK(n > 0 && strstr(buf, "\"time_synced\":false,\"device_time_utc\":null") != nullptr);
  sample("status", buf);

  // Too-small buffer fails instead of truncating.
  char small[200];
  CHECK(buildStatusJson(small, sizeof(small), s) == 0);
  CHECK(buildStatusJson(nullptr, 0, s) == 0);

  // Nullable optional members.
  StatusInput sparse = fullStatus();
  sparse.hasPsram = false;
  sparse.hasRssi = false;
  sparse.hasFeedMask = false;
  sparse.hasCompleteAge = false;
  sparse.activePartition = nullptr;
  sparse.wifiConnected = false;
  n = buildStatusJson(buf, sizeof(buf), sparse);
  CHECK(n > 0 && strstr(buf, "\"psram_free_bytes\":null") != nullptr);
  sample("status", buf);

  // Escaping keeps hostile text valid JSON and no field can carry a raw quote.
  StatusInput hostile = fullStatus();
  hostile.deviceName = "a\"b\\c\n";
  n = buildStatusJson(buf, sizeof(buf), hostile);
  CHECK(n > 0 && strstr(buf, "a\\\"b\\\\c\\u000a") != nullptr);

  // Privacy whitelist: none of these may ever appear.
  n = buildStatusJson(buf, sizeof(buf), fullStatus());
  const char *forbidden[] = {"ssid", "password", "ip_address", "\"ip\"", "mac", "trip_id", "vehicle_id",
                             "stop_id", "token", "latitude", "longitude", "url", "bssid"};
  for (const char *word : forbidden) CHECK(strstr(buf, word) == nullptr);
}

void testEvents() {
  char buf[1024];
  EventRecord boot = makeBootEvent(ResetReason::PowerOn, 2000);
  size_t n = buildEventJson(buf, sizeof(buf), kDevice, kBoot, 1, boot, 5000, 1780000000);
  CHECK(n > 0);
  CHECK(strstr(buf, "\"time_synced\":true,\"device_time_utc\":\"") != nullptr);
  CHECK(strstr(buf, "\"reset_reason\":\"power_on\"") != nullptr);
  sample("event", buf);

  // Time not synced: device_time_utc must be null (regression for the bench boot event).
  n = buildEventJson(buf, sizeof(buf), kDevice, kBoot, 1, boot, 5000, 0);
  CHECK(n > 0 && strstr(buf, "\"time_synced\":false,\"device_time_utc\":null") != nullptr);
  sample("event", buf);

  // Occurrence time is derived from age: 3 s before now.
  EventRecord e = makeBootEvent(ResetReason::Software, 2000);
  n = buildEventJson(buf, sizeof(buf), kDevice, kBoot, 2, e, 5000, 1780000003);
  CHECK(n > 0 && strstr(buf, "2026-05-28T20:26:40Z") != nullptr);  // 1780000000
  sample("event", buf);

  // Every event type produces a schema-valid event.
  for (uint8_t t = 0; t < static_cast<uint8_t>(EventType::Count); ++t) {
    EventRecord r;
    r.type = static_cast<EventType>(t);
    r.uptimeMs = 100;
    r.reset = r.type == EventType::Brownout ? ResetReason::Brownout
              : r.type == EventType::WatchdogReset ? ResetReason::Watchdog
              : ResetReason::Panic;
    r.previousDisplay = DisplayState::Fresh;
    r.currentDisplay = r.type == EventType::DisplayRecovered ? DisplayState::Fresh
                       : r.type == EventType::DisplayStale ? DisplayState::StaleHold
                       : r.type == EventType::DisplayNoData ? DisplayState::NoData
                                                            : DisplayState::DegradedHold;
    if (r.type == EventType::DisplayRecovered) r.previousDisplay = DisplayState::StaleHold;
    r.ota = OtaState::PendingVerification;
    r.heapFreeBytes = 9000;
    r.coalescedCount = 3;
    n = buildEventJson(buf, sizeof(buf), kDevice, kBoot, 10 + t, r, 5000, 1780000000);
    CHECK(n > 0);
    sample("event", buf);
  }

  char tiny[40];
  CHECK(buildEventJson(tiny, sizeof(tiny), kDevice, kBoot, 1, boot, 5000, 0) == 0);

  EventRecord ev;
  CHECK(!makeDisplayEvent(DisplayState::WaitingForData, DisplayState::Fresh, 1, ev));
  CHECK(!makeDisplayEvent(DisplayState::Fresh, DisplayState::Fresh, 1, ev));
  CHECK(makeDisplayEvent(DisplayState::Fresh, DisplayState::DegradedPartial, 1, ev) &&
        ev.type == EventType::DisplayDegraded);
  CHECK(makeDisplayEvent(DisplayState::DegradedHold, DisplayState::Fresh, 1, ev) &&
        ev.type == EventType::DisplayRecovered);
  CHECK(makeDisplayEvent(DisplayState::DegradedHold, DisplayState::StaleHold, 1, ev) &&
        ev.type == EventType::DisplayStale);
  CHECK(makeDisplayEvent(DisplayState::StaleHold, DisplayState::NoData, 1, ev) &&
        ev.type == EventType::DisplayNoData);

  bool emit = false;
  makeResetEvent(ResetReason::PowerOn, 1, emit);
  CHECK(!emit);
  CHECK(makeResetEvent(ResetReason::Panic, 1, emit).type == EventType::UnexpectedReset && emit);
  CHECK(makeResetEvent(ResetReason::Watchdog, 1, emit).type == EventType::WatchdogReset && emit);
  CHECK(makeResetEvent(ResetReason::Brownout, 1, emit).type == EventType::Brownout && emit);
}

void testIdentity() {
  uint8_t zero[16] = {0};
  char id[37];
  formatUuid4(zero, id);
  CHECK(strcmp(id, "00000000-0000-4000-8000-000000000000") == 0);
  uint8_t ff[16];
  memset(ff, 0xff, sizeof(ff));
  formatUuid4(ff, id);
  CHECK(strcmp(id, "ffffffff-ffff-4fff-bfff-ffffffffffff") == 0);

  char utc[21];
  formatUtc(1767225600u, utc);
  CHECK(strcmp(utc, "2026-01-01T00:00:00Z") == 0);
  formatUtc(1780000000u, utc);
  CHECK(strcmp(utc, "2026-05-28T20:26:40Z") == 0);
  formatUtc(1709164799u, utc);
  CHECK(strcmp(utc, "2024-02-28T23:59:59Z") == 0);
  formatUtc(1709164800u, utc);
  CHECK(strcmp(utc, "2024-02-29T00:00:00Z") == 0);
  CHECK(!timeIsValid(1000) && timeIsValid(kMinValidUnixTime));
}

void testBackoff() {
  CHECK(backoffCeilingMs(1) == 5000);
  CHECK(backoffCeilingMs(2) == 10000);
  CHECK(backoffCeilingMs(3) == 20000);
  CHECK(backoffCeilingMs(4) == 40000);
  CHECK(backoffCeilingMs(50) == kBackoffMaxMs);
  CHECK(backoffCeilingMs(0) == 5000);
  for (uint32_t streak = 1; streak < 30; ++streak) {
    CHECK(backoffDelayMs(streak, 0) == kBackoffFloorMs);
    CHECK(backoffDelayMs(streak, 0xffffffffu) == backoffCeilingMs(streak));
    CHECK(backoffDelayMs(streak, 0x80000000u) <= backoffCeilingMs(streak));
  }
  CHECK(statusIntervalMs(0) == 28500 && statusIntervalMs(0xffffffffu) == 31500);
  CHECK(startJitterMs(0) == 500 && startJitterMs(0xffffffffu) == 3000);
  uint32_t seen[4];
  Rng r;
  for (int i = 0; i < 4; ++i) seen[i] = backoffDelayMs(6, rngNext(&r));
  CHECK(seen[0] != seen[1] || seen[1] != seen[2]);  // jittered, not constant
}

struct Harness {
  Rng rng;
  Core core;
  uint64_t now = 0;
  int32_t nextMsg = 1;
  Inputs in = kReady;
  Harness() { core.begin(kBoot, rngNext, &rng); }

  // Drives next() to quiescence, accepting every publish. Returns executed publish types.
  string pump() {
    string out;
    for (int guard = 0; guard < 64; ++guard) {
      Action a = core.next(now, in);
      if (a.type == ActionType::None) break;
      switch (a.type) {
        case ActionType::StartClient: out += 'S'; core.onClientStarted(true, now); break;
        case ActionType::StopClient: out += 'X'; core.onClientStopped(now); break;
        case ActionType::PublishOnline: out += 'O'; core.publishResult(a, PublishResult::Queued, nextMsg++, now); break;
        case ActionType::PublishStatus: out += 'T'; core.publishResult(a, PublishResult::Queued, nextMsg++, now); break;
        case ActionType::PublishEvent: out += 'E'; core.publishResult(a, PublishResult::Queued, nextMsg++, now); break;
        default: break;
      }
    }
    return out;
  }
  void connect() {
    now += 5000;
    pump();
    now += 5000;
    pump();
    core.onConnected(now);
    pump();
  }
  void ackAll() {
    for (int32_t m = 1; m < nextMsg; ++m) core.onPublished(m, now);
  }
};

void testGateAndConnect() {
  Harness h;
  h.in = Inputs{false, true, true, true};
  CHECK(h.pump().empty());  // networkReady false: no connect
  h.in = Inputs{true, false, true, true};
  CHECK(h.pump().empty());
  h.in = Inputs{true, true, false, true};
  CHECK(h.pump().empty());
  h.in = Inputs{true, true, true, false};
  CHECK(h.pump().empty());
  CHECK(h.core.counters().connectAttempts == 0);

  h.in = kReady;
  CHECK(h.pump().empty());  // start jitter pending
  CHECK(h.core.nextConnectAtMs() >= 500 && h.core.nextConnectAtMs() <= 3000);
  h.now = 3001;
  CHECK(h.pump() == "S");
  CHECK(h.core.phase() == Phase::Connecting);
  h.core.onConnected(h.now);
  CHECK(h.pump() == "OT");  // online first, then retained status, no events queued yet
  CHECK(h.core.inFlightCount() == 2);
  h.ackAll();
  CHECK(h.core.stable());
  CHECK(h.core.failureStreak() == 0);
  CHECK(h.core.counters().stableConnections == 1);

  // Gate closing tears the client down.
  h.in.wifiUp = false;
  CHECK(h.pump() == "X");
  CHECK(h.core.phase() == Phase::Idle);
}

void testPublishOrderAndHeartbeat() {
  Harness h;
  h.now = 1;
  h.pump();
  h.now = 4000;
  h.pump();
  h.core.onConnected(h.now);
  h.core.enqueueEvent(makeBootEvent(ResetReason::PowerOn, 10), h.now);
  CHECK(h.pump() == "OTE");  // online, status, then queued events
  h.ackAll();
  CHECK(h.core.queueSize() == 0);

  // Heartbeat cadence is a pure function of the supplied clock: ~30 s, independent of
  // anything the caller does between calls (no dependence on a loop or MTA cycle).
  uint64_t last = h.core.nextStatusAtMs() - 0;
  uint64_t sends = 0;
  uint64_t lastSend = h.now;
  uint64_t minGap = UINT64_MAX, maxGap = 0;
  const uint64_t startT = h.now;
  for (uint64_t t = startT; t < startT + 600000; t += 1000) {
    h.now = t;
    Action a = h.core.next(h.now, h.in);
    if (a.type == ActionType::PublishStatus) {
      h.core.publishResult(a, PublishResult::Queued, h.nextMsg, h.now);
      h.core.onPublished(h.nextMsg++, h.now + 100);
      if (sends > 0) {
        const uint64_t gap = t - lastSend;
        if (gap < minGap) minGap = gap;
        if (gap > maxGap) maxGap = gap;
      }
      lastSend = t;
      ++sends;
    }
  }
  (void)last;
  CHECK(sends >= 18 && sends <= 22);
  CHECK(minGap >= 28000 && maxGap <= 33000);

  // A long stall in the caller (a 100 s blocking cycle) yields exactly one catch-up
  // status, never a backlog.
  uint32_t before = h.core.counters().statusSent;
  h.now += 100000;
  string r = h.pump();
  CHECK(r == "T");
  CHECK(h.core.counters().statusSent == before + 1);
  CHECK(h.core.next(h.now, h.in).type == ActionType::None);
}

void testBackoffReset() {
  Harness h;
  h.rng.useFixed = true;
  h.rng.fixed = 0xffffffffu;  // worst-case jitter: delay equals the ceiling
  h.now = 10;
  h.pump();
  uint32_t expected[] = {5000, 10000, 20000, 40000};
  for (int i = 0; i < 4; ++i) {
    h.now = h.core.nextConnectAtMs();
    CHECK(h.pump() == "S");
    const uint64_t failAt = h.now;
    h.core.onDisconnected(h.now);  // never reached CONNACK
    CHECK(h.core.next(h.now, h.in).type == ActionType::StopClient);
    h.core.onClientStopped(h.now);
    CHECK(h.core.failureStreak() == static_cast<uint32_t>(i + 1));
    CHECK(h.core.nextConnectAtMs() == failAt + expected[i]);
  }

  // Connect, but the failure streak is not reset until both PUBACKs arrive.
  h.now = h.core.nextConnectAtMs();
  h.pump();
  h.core.onConnected(h.now);
  h.pump();
  CHECK(h.core.failureStreak() == 4);
  h.core.onPublished(h.nextMsg - 2, h.now);
  CHECK(h.core.failureStreak() == 4);
  h.core.onPublished(h.nextMsg - 1, h.now);
  CHECK(h.core.failureStreak() == 0);

  // Connect timeout.
  Harness t;
  t.now = 10;
  t.pump();
  t.now = t.core.nextConnectAtMs();
  CHECK(t.pump() == "S");
  t.now += kConnectTimeoutMs;
  CHECK(t.pump() == "X");
  CHECK(t.core.failureStreak() == 1);

  // Client start failure counts as a failure.
  Harness f;
  f.now = 10;
  f.pump();
  f.now = f.core.nextConnectAtMs();
  Action a = f.core.next(f.now, f.in);
  CHECK(a.type == ActionType::StartClient);
  f.core.onClientStarted(false, f.now);
  CHECK(f.core.phase() == Phase::Idle && f.core.failureStreak() == 1);

  // Stable drop restarts the ceiling at 5 s.
  Harness d;
  d.rng.useFixed = true;
  d.rng.fixed = 0xffffffffu;
  d.connect();
  d.ackAll();
  CHECK(d.core.stable());
  d.core.onDisconnected(d.now);
  d.pump();
  CHECK(d.core.failureStreak() == 1);
}

void testAcks() {
  Harness h;
  h.connect();
  h.pump();
  CHECK(h.core.inFlightCount() == 2);
  // Delayed PUBACK well under the timeout is not a failure.
  h.now += 11000;
  h.pump();
  CHECK(h.core.phase() == Phase::Connected);
  h.ackAll();
  CHECK(h.core.counters().maxAckLatencyMs >= 11000);
  CHECK(h.core.counters().acksMissed == 0);
  h.core.onPublished(9999, h.now);
  CHECK(h.core.counters().unmatchedAcks == 1);

  // Missed ack forces a reconnect and the outstanding event is retried afterwards.
  Harness m;
  m.connect();
  m.pump();
  m.core.enqueueEvent(makeBootEvent(ResetReason::PowerOn, 1), m.now);
  m.now += 100;
  m.core.onPublished(1, m.now);  // online acked
  m.core.onPublished(2, m.now);  // status acked
  CHECK(m.pump() == "E");
  const uint64_t seq = m.core.queuedSequence(0);
  m.now += kAckTimeoutMs;
  CHECK(m.core.next(m.now, m.in).type == ActionType::StopClient);
  CHECK(m.core.counters().acksMissed >= 1);
  m.core.onClientStopped(m.now);
  CHECK(m.core.queueSize() == 1 && m.core.queuedSequence(0) == seq);  // identity survives
  m.now = m.core.nextConnectAtMs();
  m.pump();
  m.core.onConnected(m.now);
  CHECK(m.pump() == "OTE");

  // Disconnect with PUBACK outstanding: event retained, sequence unchanged.
  Harness x;
  x.connect();
  x.core.enqueueEvent(makeBootEvent(ResetReason::PowerOn, 1), x.now);
  x.pump();
  x.core.onDisconnected(x.now);
  x.pump();
  CHECK(x.core.queueSize() == 1);

  // Publish failure: reconnect.
  Harness p;
  p.now += 5000;
  p.pump();
  p.now += 5000;
  p.pump();
  p.core.onConnected(p.now);
  Action a = p.core.next(p.now, p.in);
  CHECK(a.type == ActionType::PublishOnline);
  p.core.publishResult(a, PublishResult::Failed, -1, p.now);
  CHECK(p.core.counters().publishFailures == 1);
  CHECK(p.core.next(p.now, p.in).type == ActionType::StopClient);

  // Oversized payload is rejected without reconnect; the event is not retried forever.
  Harness big;
  big.connect();
  big.pump();
  big.core.enqueueEvent(makeBootEvent(ResetReason::PowerOn, 1), big.now);
  Action e = big.core.next(big.now, big.in);
  CHECK(e.type == ActionType::PublishEvent);
  big.core.publishResult(e, PublishResult::TooLarge, 0, big.now);
  CHECK(big.core.counters().payloadRejected == 1 && big.core.queueSize() == 0);
  CHECK(big.core.phase() == Phase::Connected);
}

EventRecord ev(EventType type, Severity sev = Severity::Info) {
  EventRecord e;
  e.type = type;
  e.severity = sev;
  return e;
}

void testQueue() {
  Harness h;
  // Coalescing: identical transitions within 60 s.
  CHECK(h.core.enqueueEvent(ev(EventType::DisplayStale), 1000) == Admit::Queued);
  CHECK(h.core.enqueueEvent(ev(EventType::DisplayStale), 5000) == Admit::Coalesced);
  CHECK(h.core.enqueueEvent(ev(EventType::DisplayStale), 9000) == Admit::Coalesced);
  CHECK(h.core.queueSize() == 1 && h.core.queuedEvent(0).coalescedCount == 3);
  CHECK(h.core.counters().eventsCoalesced == 2);
  // After the window, a new entry.
  CHECK(h.core.enqueueEvent(ev(EventType::DisplayStale), 70000) == Admit::Queued);
  CHECK(h.core.queueSize() == 2);

  // Sequences are monotonic and assigned at enqueue.
  CHECK(h.core.queuedSequence(1) == h.core.queuedSequence(0) + 1);

  // Overflow: 16 slots; oldest ordinary dropped first, critical retained.
  Harness o;
  o.core.enqueueEvent(ev(EventType::PhysicalSafetyFault, Severity::Critical), 0);
  for (int i = 0; i < 40; ++i) {
    const EventType types[] = {EventType::DisplayDegraded, EventType::DisplayNoData, EventType::WifiReconnected,
                               EventType::DisplayRecovered, EventType::MonitoringEnabled};
    o.core.enqueueEvent(ev(types[i % 5]), 1000 + static_cast<uint64_t>(i) * 100000);
  }
  CHECK(o.core.queueSize() == kEventQueueSlots);
  CHECK(o.core.counters().eventsDropped > 0);
  CHECK(o.core.queuedEvent(0).type == EventType::PhysicalSafetyFault);
  for (size_t i = 1; i < o.core.queueSize(); ++i) CHECK(o.core.queuedSequence(i) > o.core.queuedSequence(i - 1));

  // All critical: oldest overall dropped.
  Harness c;
  for (int i = 0; i < 20; ++i) {
    c.core.enqueueEvent(ev(static_cast<EventType>(11 + (i % 6)), Severity::Critical), static_cast<uint64_t>(i) * 100000);
  }
  CHECK(c.core.queueSize() == kEventQueueSlots);

  // Spacing: ordinary events 10 s apart; critical bypasses spacing.
  Harness s;
  s.connect();
  s.pump();
  s.ackAll();
  s.core.enqueueEvent(ev(EventType::DisplayDegraded), s.now);
  s.core.enqueueEvent(ev(EventType::DisplayNoData), s.now);
  CHECK(s.pump() == "E");
  s.now += 5000;
  CHECK(s.pump().empty());
  s.core.enqueueEvent(ev(EventType::Brownout, Severity::Critical), s.now);
  CHECK(s.pump() == "E");  // critical bypassed the spacing
  s.now += 10000;
  CHECK(s.pump() == "E");  // ordinary resumes after 10 s

  // 12 events per minute cap, critical included.
  Harness r;
  r.connect();
  r.pump();
  r.ackAll();
  int sent = 0;
  for (int i = 0; i < 14; ++i) {
    r.core.enqueueEvent(ev(EventType::PhysicalSafetyFault, Severity::Critical), r.now + static_cast<uint64_t>(i) * 70000);
  }
  r.now = r.now;
  for (int i = 0; i < 30; ++i) {
    r.core.enqueueEvent(ev(EventType::HeapCritical, Severity::Critical), r.now + 61000 * 5);
  }
  Harness q;
  q.connect();
  q.pump();
  q.ackAll();
  for (size_t i = 0; i < kEventQueueSlots; ++i) {
    q.core.enqueueEvent(ev(static_cast<EventType>(11 + (i % 6)), Severity::Critical), q.now + i * 100000);
  }
  for (int i = 0; i < 20; ++i) {
    string out = q.pump();
    sent += static_cast<int>(out.size());
    q.ackAll();
    q.now += 1000;
  }
  CHECK(sent <= static_cast<int>(kEventsPerMinute));
  CHECK(sent == static_cast<int>(kEventsPerMinute));
}

void testSequence() {
  Harness h;
  h.core.setSequenceForTest(kMaxSequence - 1);
  h.core.enqueueEvent(ev(EventType::DisplayDegraded), 0);
  CHECK(h.core.lastSequence() == kMaxSequence);
  h.core.enqueueEvent(ev(EventType::DisplayNoData), 0);
  CHECK(h.core.lastSequence() == 1);
  CHECK(h.core.counters().sequenceWraps == 1);

  Harness s;
  s.connect();
  s.pump();
  CHECK(s.core.lastSequence() == 1);  // first status
  CHECK(s.core.counters().statusSent == 1);
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  testSerialization();
  testEvents();
  testIdentity();
  testBackoff();
  testGateAndConnect();
  testPublishOrderAndHeartbeat();
  testBackoffReset();
  testAcks();
  testQueue();
  testSequence();
  printf("telemetry core checks=%d failures=%d\n", gChecks, gFailures);
  return gFailures == 0 ? 0 : 1;
}
