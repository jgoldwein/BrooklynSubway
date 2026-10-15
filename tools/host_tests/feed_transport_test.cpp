// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) tests of the Phase 4J transport core:
// feed buffer download, poll cycle, counters and scheduler. ESP32 Wi-Fi/TLS
// (WiFiClientSecure) is NOT tested here.
// Build/run with: python tools/run_feed_transport_tests.py [--replay DIR[@UNIX_NOW]]

#include "feed_transport.h"

#include "train_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

using namespace brooklyn::transport;
namespace net = brooklyn::network;
namespace pl = brooklyn::pipeline;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                          \
  do {                                                       \
    ++checks;                                                \
    if (!(cond)) {                                           \
      ++failures;                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                        \
  } while (0)

constexpr size_t kNever = static_cast<size_t>(-1);

// Scripted connection: one script per URL.
struct Script {
  OpenResult open = OpenResult::Ok;
  int32_t contentLength = kUnknownLength;
  std::vector<uint8_t> body;
  size_t chunk = 1460;
  size_t failAt = kNever;          // body offset at which read() fails
  int32_t failCode = kReadError;
  bool overReturn = false;         // broken adapter: returns more than asked
  uint32_t costMs = 0;             // simulated time consumed by this download
};

struct FakeClock : Clock {
  uint32_t now = 0;
  uint32_t nowMs() const override { return now; }
};

class FakeConnection : public FeedConnection {
 public:
  std::vector<const char *> urls;
  std::vector<Script> scripts;
  int opens = 0;
  int closes = 0;
  int reads = 0;
  bool opened = false;
  FakeClock *clock = nullptr;

  void add(const char *url, Script script) {
    urls.push_back(url);
    scripts.push_back(std::move(script));
  }

  OpenResult open(const char *url, int32_t &contentLength) override {
    ++opens;
    cur_ = nullptr;
    for (size_t i = 0; i < urls.size(); ++i) {
      if (strcmp(urls[i], url) == 0) cur_ = &scripts[i];
    }
    pos_ = 0;
    opened = true;
    if (cur_ == nullptr) return OpenResult::HttpFailure;
    if (clock != nullptr) clock->now += cur_->costMs;
    contentLength = cur_->contentLength;
    return cur_->open;
  }

  int32_t read(uint8_t *dest, size_t maxLen) override {
    ++reads;
    if (cur_ == nullptr) return kReadError;
    if (cur_->overReturn) return static_cast<int32_t>(maxLen + 1);
    if (pos_ == cur_->failAt) return cur_->failCode;
    if (pos_ >= cur_->body.size()) return kReadEof;
    size_t n = cur_->chunk < maxLen ? cur_->chunk : maxLen;
    if (n > cur_->body.size() - pos_) n = cur_->body.size() - pos_;
    if (cur_->failAt != kNever && cur_->failAt > pos_ && n > cur_->failAt - pos_) {
      n = cur_->failAt - pos_;
    }
    memcpy(dest, cur_->body.data() + pos_, n);
    pos_ += n;
    return static_cast<int32_t>(n);
  }

  void close() override {
    ++closes;
    opened = false;
  }

 private:
  Script *cur_ = nullptr;
  size_t pos_ = 0;
};

// Records exactly what the sink was given.
struct RecordingSink : FeedSink {
  int begins = 0, finishes = 0;
  std::vector<std::vector<uint8_t>> feeds;
  void beginCycle() override { ++begins; }
  void processFeed(const uint8_t *data, size_t length) override {
    feeds.emplace_back(data, data + length);
  }
  void finishCycle() override { ++finishes; }
};

std::vector<uint8_t> pattern(size_t n, uint8_t seed) {
  std::vector<uint8_t> v(n);
  for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(seed + i * 7 + (i >> 8));
  return v;
}

Script okScript(const std::vector<uint8_t> &body, bool withLength = true) {
  Script s;
  s.body = body;
  s.contentLength = withLength ? static_cast<int32_t>(body.size()) : kUnknownLength;
  return s;
}

// A buffer over a std::vector with a guard region after it, to catch overruns.
struct Buf {
  std::vector<uint8_t> mem;
  size_t cap;
  explicit Buf(size_t capacity) : mem(capacity + 16, 0xEE), cap(capacity) {}
  FeedBuffer fb() { return FeedBuffer{mem.data(), cap}; }
  bool guardIntact() const {
    for (size_t i = cap; i < cap + 16; ++i) {
      if (mem[i] != 0xEE) return false;
    }
    return true;
  }
};

DownloadStatus dl(FakeConnection &c, Buf &b, bool unknownOk, size_t &len) {
  return downloadFeed(c, "u", b.fb(), unknownOk, len);
}

// ---- Single-download tests ------------------------------------------------

void testExactAndZero() {
  FakeConnection c;
  Buf b(1000);
  size_t len = 99;

  auto body = pattern(700, 3);
  c.add("u", okScript(body));
  CHECK(dl(c, b, false, len) == DownloadStatus::Ok);
  CHECK(len == 700 && memcmp(b.mem.data(), body.data(), 700) == 0);
  CHECK(c.opens == 1 && c.closes == 1);

  c.scripts[0] = okScript({}, true);
  CHECK(dl(c, b, false, len) == DownloadStatus::ZeroLength && len == 0);
  c.scripts[0] = okScript({}, false);
  CHECK(dl(c, b, true, len) == DownloadStatus::ZeroLength && len == 0);
  CHECK(c.closes == 3);
  CHECK(b.guardIntact());
}

void testCapacityBoundary() {
  for (bool known : {true, false}) {
    FakeConnection c;
    Buf b(1000);
    size_t len = 0;

    auto exact = pattern(1000, 9);
    c.add("u", okScript(exact, known));
    CHECK(dl(c, b, true, len) == DownloadStatus::Ok);
    CHECK(len == 1000 && memcmp(b.mem.data(), exact.data(), 1000) == 0);

    auto over = pattern(1001, 9);
    c.scripts[0] = okScript(over, known);
    const int readsBefore = c.reads;
    CHECK(dl(c, b, true, len) == DownloadStatus::TooLarge && len == 0);
    if (known) CHECK(c.reads == readsBefore);  // rejected from Content-Length alone
    CHECK(b.guardIntact());

    auto under = pattern(999, 9);
    c.scripts[0] = okScript(under, known);
    CHECK(dl(c, b, true, len) == DownloadStatus::Ok && len == 999);
    CHECK(b.guardIntact());
  }
  // Huge unknown body is cut off at the buffer edge, never stored beyond it.
  FakeConnection c;
  Buf b(256);
  size_t len = 0;
  c.add("u", okScript(pattern(100000, 1), false));
  CHECK(dl(c, b, true, len) == DownloadStatus::TooLarge && len == 0);
  CHECK(b.guardIntact());
  // A lying Content-Length above capacity is rejected without reading.
  Script lie = okScript(pattern(10, 1));
  lie.contentLength = 5000000;
  c.scripts[0] = lie;
  const int r = c.reads;
  CHECK(dl(c, b, true, len) == DownloadStatus::TooLarge && c.reads == r);
}

void testPartialAndErrors() {
  FakeConnection c;
  Buf b(1000);
  size_t len = 0;

  // Closed early: Content-Length 500, only 300 delivered then clean EOF.
  Script early = okScript(pattern(300, 2));
  early.contentLength = 500;
  c.add("u", early);
  CHECK(dl(c, b, false, len) == DownloadStatus::Partial && len == 0);

  // Connection error mid-body.
  Script err = okScript(pattern(500, 2));
  err.failAt = 321;
  err.failCode = kReadError;
  c.scripts[0] = err;
  CHECK(dl(c, b, false, len) == DownloadStatus::Partial && len == 0);

  // Timeout mid-body.
  Script to = okScript(pattern(500, 2));
  to.failAt = 100;
  to.failCode = kReadTimeout;
  c.scripts[0] = to;
  CHECK(dl(c, b, false, len) == DownloadStatus::Timeout && len == 0);

  // Failure at the very first read.
  to.failAt = 0;
  c.scripts[0] = to;
  CHECK(dl(c, b, false, len) == DownloadStatus::Timeout);

  // Unknown length: an error is never mistaken for a clean end.
  Script uerr = okScript(pattern(500, 2), false);
  uerr.failAt = 400;
  c.scripts[0] = uerr;
  CHECK(dl(c, b, true, len) == DownloadStatus::Partial && len == 0);

  // Adapter returning more than requested is rejected.
  Script over = okScript(pattern(10, 2));
  over.overReturn = true;
  c.scripts[0] = over;
  CHECK(dl(c, b, false, len) == DownloadStatus::Partial);
  CHECK(b.guardIntact());

  // Every failure closed the connection.
  CHECK(c.opens == c.closes);
}

void testOpenFailures() {
  FakeConnection c;
  Buf b(1000);
  size_t len = 0;
  struct Case { OpenResult open; DownloadStatus expect; };
  const Case cases[] = {
      {OpenResult::WifiUnavailable, DownloadStatus::WifiUnavailable},
      {OpenResult::SecureConnectFailure, DownloadStatus::SecureConnectFailure},
      {OpenResult::HttpFailure, DownloadStatus::HttpFailure},
      {OpenResult::Timeout, DownloadStatus::Timeout},
  };
  c.add("u", Script{});
  for (const Case &k : cases) {
    Script s = okScript(pattern(10, 1));
    s.open = k.open;
    c.scripts[0] = s;
    const int reads = c.reads;
    CHECK(dl(c, b, false, len) == k.expect && len == 0);
    CHECK(c.reads == reads);  // no body read after a failed open
  }
  CHECK(c.opens == c.closes);

  // Unknown URL in the fake behaves as an HTTP failure.
  CHECK(downloadFeed(c, "nope", b.fb(), false, len) == DownloadStatus::HttpFailure);
}

void testUnknownLengthPolicy() {
  FakeConnection c;
  Buf b(1000);
  size_t len = 0;
  c.add("u", okScript(pattern(400, 5), false));
  CHECK(dl(c, b, false, len) == DownloadStatus::UnknownLength && len == 0);
  CHECK(dl(c, b, true, len) == DownloadStatus::Ok && len == 400);
}

void testBufferUnavailable() {
  FakeConnection c;
  size_t len = 0;
  c.add("u", okScript(pattern(10, 1)));
  CHECK(downloadFeed(c, "u", FeedBuffer{nullptr, 100}, false, len) == DownloadStatus::BufferUnavailable);
  uint8_t x;
  CHECK(downloadFeed(c, "u", FeedBuffer{&x, 0}, false, len) == DownloadStatus::BufferUnavailable);
  CHECK(c.opens == 0);  // nothing was requested without a buffer
}

void testChunkSizes() {
  // Same body through very different chunking.
  const auto body = pattern(5000, 11);
  for (size_t chunk : {size_t{1}, size_t{7}, size_t{1460}, size_t{4999}, size_t{5000}, size_t{99999}}) {
    for (bool known : {true, false}) {
      FakeConnection c;
      Buf b(8192);
      size_t len = 0;
      Script s = okScript(body, known);
      s.chunk = chunk;
      c.add("u", s);
      CHECK(dl(c, b, true, len) == DownloadStatus::Ok && len == 5000 &&
            memcmp(b.mem.data(), body.data(), 5000) == 0);
    }
  }
}

// ---- Poll cycle --------------------------------------------------------------

constexpr PollPreconditions kReady{true, true};

net::FeedSpec kTestFeeds[8] = {
    {"f0", "f0"}, {"f1", "f1"}, {"f2", "f2"}, {"f3", "f3"},
    {"f4", "f4"}, {"f5", "f5"}, {"f6", "f6"}, {"f7", "f7"},
};

void addEight(FakeConnection &c, std::vector<std::vector<uint8_t>> &bodies) {
  bodies.clear();
  for (int i = 0; i < 8; ++i) {
    bodies.push_back(pattern(100 + static_cast<size_t>(i) * 131 + (i == 3 ? 4000 : 0),
                             static_cast<uint8_t>(i * 17)));
    c.add(kTestFeeds[i].url, okScript(bodies.back()));
  }
}

void testEightSequential() {
  FakeConnection c;
  std::vector<std::vector<uint8_t>> bodies;
  addEight(c, bodies);
  RecordingSink sink;
  Buf b(8192);
  TransportCounters tc{};

  CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::Complete);
  CHECK(r.attempted == 8 && r.downloaded == 8 && r.processed == 8 && r.processedMask == 0xFF);
  CHECK(sink.begins == 1 && sink.finishes == 1 && sink.feeds.size() == 8);
  for (int i = 0; i < 8; ++i) CHECK(sink.feeds[static_cast<size_t>(i)] == bodies[static_cast<size_t>(i)]);
  CHECK(tc.pollCycles == 1 && tc.feedsAttempted == 8 && tc.feedsDownloaded == 8 &&
        tc.feedsProcessed == 8);
  uint64_t total = 0;
  for (auto &v : bodies) total += v.size();
  CHECK(tc.bytesDownloaded == total);
  CHECK(tc.partialCycles == 0 && tc.emptyCycles == 0);
  CHECK(c.opens == 8 && c.closes == 8);
  CHECK(b.guardIntact());

  // Second cycle reuses the same buffer; counters accumulate.
  r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::Complete && tc.pollCycles == 2 && tc.feedsProcessed == 16);
  CHECK(sink.feeds.size() == 16 && sink.feeds[8 + 3] == bodies[3]);
}

void testEachFeedFailing() {
  // For each feed position and each failure kind, exactly that feed is lost and
  // the other seven are still delivered intact and in order.
  enum Kind { kHttp, kTls, kTimeoutOpen, kTooLarge, kPartial, kZero, kUnknownLen, kWifi, kTimeoutBody };
  for (int bad = 0; bad < 8; ++bad) {
    for (int kind = kHttp; kind <= kTimeoutBody; ++kind) {
      FakeConnection c;
      std::vector<std::vector<uint8_t>> bodies;
      addEight(c, bodies);
      Script &s = c.scripts[static_cast<size_t>(bad)];
      switch (kind) {
        case kHttp: s.open = OpenResult::HttpFailure; break;
        case kTls: s.open = OpenResult::SecureConnectFailure; break;
        case kTimeoutOpen: s.open = OpenResult::Timeout; break;
        case kTooLarge: s.contentLength = 9000; break;
        case kPartial: s.failAt = s.body.size() / 2; break;
        case kZero: s.body.clear(); s.contentLength = 0; break;
        case kUnknownLen: s.contentLength = kUnknownLength; break;
        case kWifi: s.open = OpenResult::WifiUnavailable; break;
        case kTimeoutBody: s.failAt = 5; s.failCode = kReadTimeout; break;
      }
      RecordingSink sink;
      Buf b(8192);
      TransportCounters tc{};
      CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
      CHECK(r.outcome == CycleOutcome::Partial);
      CHECK(r.attempted == 8 && r.downloaded == 7 && r.processed == 7);
      CHECK(r.processedMask == static_cast<uint16_t>(0xFF & ~(1u << bad)));
      CHECK(sink.feeds.size() == 7 && sink.begins == 1 && sink.finishes == 1);
      size_t k = 0;
      for (int i = 0; i < 8; ++i) {
        if (i == bad) continue;
        CHECK(sink.feeds[k++] == bodies[static_cast<size_t>(i)]);
      }
      CHECK(tc.partialCycles == 1 && tc.emptyCycles == 0 && tc.feedsDownloaded == 7);
      const uint32_t sum = tc.wifiUnavailable + tc.secureConnectFailures + tc.httpFailures +
                           tc.timeouts + tc.zeroLength + tc.feedTooLarge +
                           tc.partialDownload + tc.unknownLength + tc.bufferUnavailable;
      CHECK(sum == 1);  // exactly one failure counter moved, by exactly one
      CHECK(c.opens == c.closes);
    }
  }
}

void testSpecificCounters() {
  FakeConnection c;
  std::vector<std::vector<uint8_t>> bodies;
  addEight(c, bodies);
  c.scripts[0].open = OpenResult::SecureConnectFailure;
  c.scripts[1].open = OpenResult::HttpFailure;
  c.scripts[2].open = OpenResult::Timeout;
  c.scripts[3].contentLength = 9000;                  // too large for 8192
  c.scripts[4].failAt = 10;                           // partial
  c.scripts[5].body.clear();
  c.scripts[5].contentLength = 0;                     // zero length
  c.scripts[6].contentLength = kUnknownLength;        // unknown length rejected
  c.scripts[7].open = OpenResult::WifiUnavailable;
  RecordingSink sink;
  Buf b(8192);
  TransportCounters tc{};
  CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::NoFeeds && r.processed == 0 && r.processedMask == 0);
  CHECK(sink.begins == 1 && sink.finishes == 1 && sink.feeds.empty());
  CHECK(tc.secureConnectFailures == 1 && tc.httpFailures == 1 && tc.timeouts == 1 &&
        tc.feedTooLarge == 1 && tc.partialDownload == 1 && tc.zeroLength == 1 &&
        tc.unknownLength == 1 && tc.wifiUnavailable == 1);
  CHECK(tc.feedsAttempted == 8 && tc.feedsDownloaded == 0 && tc.feedsProcessed == 0);
  CHECK(tc.emptyCycles == 1 && tc.partialCycles == 0 && tc.bytesDownloaded == 0);
}

void testSkippedCycles() {
  FakeConnection c;
  std::vector<std::vector<uint8_t>> bodies;
  addEight(c, bodies);
  RecordingSink sink;
  Buf b(8192);
  TransportCounters tc{};

  CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, PollPreconditions{false, true}, false, tc);
  CHECK(r.outcome == CycleOutcome::SkippedWifi && tc.wifiUnavailable == 1);
  r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, PollPreconditions{true, false}, false, tc);
  CHECK(r.outcome == CycleOutcome::SkippedTime && tc.timeUnavailable == 1);
  r = runPollCycle(sink, c, FeedBuffer{nullptr, 0}, kTestFeeds, 8, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::SkippedBuffer && tc.bufferUnavailable == 1);
  // Skipped cycles touch neither the connection nor the sink (previous result kept).
  CHECK(c.opens == 0 && sink.begins == 0 && sink.finishes == 0 && sink.feeds.empty());
  CHECK(tc.pollCycles == 0 && tc.feedsAttempted == 0);
  // Wi-Fi is checked before time.
  r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, PollPreconditions{false, false}, false, tc);
  CHECK(r.outcome == CycleOutcome::SkippedWifi);
}

void testFailedCycleDoesNotAffectNext() {
  FakeConnection c;
  std::vector<std::vector<uint8_t>> bodies;
  addEight(c, bodies);
  RecordingSink sink;
  Buf b(8192);
  TransportCounters tc{};

  for (auto &s : c.scripts) s.open = OpenResult::SecureConnectFailure;
  CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::NoFeeds);
  for (auto &s : c.scripts) s.open = OpenResult::Ok;
  r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::Complete && sink.feeds.size() == 8);
  CHECK(sink.begins == 2 && sink.finishes == 2);
  for (int i = 0; i < 8; ++i) CHECK(sink.feeds[static_cast<size_t>(i)] == bodies[static_cast<size_t>(i)]);
}

void testBufferReuseNoStaleData() {
  // A short feed after a long one must expose only its own bytes.
  FakeConnection c;
  c.add("f0", okScript(pattern(3000, 1)));
  c.add("f1", okScript(pattern(10, 200)));
  net::FeedSpec feeds[2] = {{"a", "f0"}, {"b", "f1"}};
  RecordingSink sink;
  Buf b(4096);
  TransportCounters tc{};
  runPollCycle(sink, c, b.fb(), feeds, 2, kReady, false, tc);
  CHECK(sink.feeds.size() == 2 && sink.feeds[1].size() == 10 && sink.feeds[1] == pattern(10, 200));

  // A feed that fails after partially filling the buffer is never processed,
  // and the next good feed is unaffected by the debris.
  FakeConnection d;
  Script half = okScript(pattern(3000, 1));
  half.failAt = 2000;
  d.add("f0", half);
  d.add("f1", okScript(pattern(10, 200)));
  RecordingSink sink2;
  runPollCycle(sink2, d, b.fb(), feeds, 2, kReady, false, tc);
  CHECK(sink2.feeds.size() == 1 && sink2.feeds[0] == pattern(10, 200));
}

// ---- Cycle budget -------------------------------------------------------------

constexpr uint32_t kBudget = 60000;

void testBudget() {
  struct Case { uint32_t cost[8]; bool fail[8]; };
  std::vector<std::vector<uint8_t>> bodies;

  // 1-2: first feed succeeds, later feeds fail slowly and eat the budget.
  {
    FakeConnection c;
    FakeClock clk;
    c.clock = &clk;
    addEight(c, bodies);
    for (int i = 0; i < 8; ++i) c.scripts[static_cast<size_t>(i)].costMs = 1000;
    for (int i = 1; i < 8; ++i) {
      c.scripts[static_cast<size_t>(i)].open = OpenResult::Timeout;
      c.scripts[static_cast<size_t>(i)].costMs = 25000;
    }
    RecordingSink sink;
    Buf b(8192);
    TransportCounters tc{};
    CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc,
                                 CycleBudget{&clk, kBudget});
    // t: f0 ->1s, f1 ->26s, f2 ->51s, f3 starts at 51s (<60) ->76s, f4.. skipped.
    CHECK(r.outcome == CycleOutcome::Partial);
    CHECK(r.attempted == 4 && r.processed == 1 && r.processedMask == 0x01);
    CHECK(r.skippedByBudget == 4);
    CHECK(c.opens == 4 && c.closes == 4);  // no new feed after budget expiry
    CHECK(tc.feedsSkippedByBudget == 4 && tc.cycleBudgetExceeded == 1);
    CHECK(tc.feedsAttempted == 4 && tc.timeouts == 3);
    CHECK(tc.partialCycles == 1 && tc.emptyCycles == 0);
    CHECK(sink.begins == 1 && sink.finishes == 1);
    CHECK(sink.feeds.size() == 1 && sink.feeds[0] == bodies[0]);  // earlier success kept intact
    CHECK(b.guardIntact());
  }
  // 3: zero successes -> NoFeeds; the failure counters and skips add up to 8.
  {
    FakeConnection c;
    FakeClock clk;
    c.clock = &clk;
    addEight(c, bodies);
    for (auto &s : c.scripts) { s.open = OpenResult::SecureConnectFailure; s.costMs = 30000; }
    RecordingSink sink;
    Buf b(8192);
    TransportCounters tc{};
    CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc,
                                 CycleBudget{&clk, kBudget});
    CHECK(r.outcome == CycleOutcome::NoFeeds && r.processed == 0);
    CHECK(r.attempted == 2 && r.skippedByBudget == 6);
    CHECK(tc.secureConnectFailures == 2 && tc.feedsSkippedByBudget == 6);
    CHECK(tc.feedsAttempted + tc.feedsSkippedByBudget == 8);
    CHECK(tc.emptyCycles == 1 && tc.cycleBudgetExceeded == 1);
    CHECK(sink.begins == 1 && sink.finishes == 1 && sink.feeds.empty());
    CHECK(c.opens == 2);

    // 4-5: the next cycle starts with a fresh budget and fully recovers.
    for (auto &s : c.scripts) { s.open = OpenResult::Ok; s.costMs = 1000; }
    r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc, CycleBudget{&clk, kBudget});
    CHECK(r.outcome == CycleOutcome::Complete && r.processed == 8 && r.skippedByBudget == 0);
    CHECK(tc.cycleBudgetExceeded == 1 && tc.feedsSkippedByBudget == 6);  // unchanged
    CHECK(sink.feeds.size() == 8);
    for (int i = 0; i < 8; ++i) CHECK(sink.feeds[static_cast<size_t>(i)] == bodies[static_cast<size_t>(i)]);
  }
  // Budget that expires exactly at a boundary; wrap-safe clock; budget 0 skips all.
  {
    FakeConnection c;
    FakeClock clk;
    clk.now = 0xFFFFFFFFu - 5000;  // clock wraps during the cycle
    c.clock = &clk;
    addEight(c, bodies);
    for (auto &s : c.scripts) s.costMs = 20000;
    RecordingSink sink;
    Buf b(8192);
    TransportCounters tc{};
    CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc, CycleBudget{&clk, kBudget});
    CHECK(r.attempted == 3 && r.skippedByBudget == 5 && r.outcome == CycleOutcome::Partial);
    CHECK(r.processedMask == 0x07);  // 0,20,40 s start; 60 s start is not < 60 s

    TransportCounters tc0{};
    RecordingSink sink0;
    r = runPollCycle(sink0, c, b.fb(), kTestFeeds, 8, kReady, false, tc0, CycleBudget{&clk, 0});
    CHECK(r.outcome == CycleOutcome::NoFeeds && r.attempted == 0 && r.skippedByBudget == 8);
  }
  // Healthy cycle within budget never trips it; a partial body is never forwarded
  // even when the budget is what ends the cycle.
  {
    FakeConnection c;
    FakeClock clk;
    c.clock = &clk;
    addEight(c, bodies);
    for (auto &s : c.scripts) s.costMs = 500;
    c.scripts[1].failAt = 50;
    c.scripts[1].costMs = 70000;
    RecordingSink sink;
    Buf b(8192);
    TransportCounters tc{};
    CycleResult r = runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc, CycleBudget{&clk, kBudget});
    CHECK(r.outcome == CycleOutcome::Partial && r.processedMask == 0x01);
    CHECK(sink.feeds.size() == 1 && sink.feeds[0] == bodies[0]);
    CHECK(tc.partialDownload == 1 && tc.feedsSkippedByBudget == 6);
    CHECK(tc.cycleBudgetExceeded == 1);

    FakeConnection d;
    FakeClock clk2;
    d.clock = &clk2;
    addEight(d, bodies);
    for (auto &s : d.scripts) s.costMs = 5000;
    TransportCounters tc2{};
    RecordingSink sink2;
    r = runPollCycle(sink2, d, b.fb(), kTestFeeds, 8, kReady, false, tc2, CycleBudget{&clk2, kBudget});
    CHECK(r.outcome == CycleOutcome::Complete && tc2.cycleBudgetExceeded == 0 && tc2.feedsSkippedByBudget == 0);
  }
  // Skipped (precondition) cycles never touch budget counters.
  {
    FakeConnection c;
    FakeClock clk;
    RecordingSink sink;
    Buf b(8192);
    TransportCounters tc{};
    runPollCycle(sink, c, b.fb(), kTestFeeds, 8, PollPreconditions{false, true}, false, tc, CycleBudget{&clk, kBudget});
    CHECK(tc.cycleBudgetExceeded == 0 && tc.feedsSkippedByBudget == 0);
  }
  CHECK(net::kDefaultNetworkConfig.cycleBudgetMs == 60000);
  CHECK(net::kDefaultNetworkConfig.feedDeadlineMs == 12000);
  CHECK(net::kDefaultNetworkConfig.cycleBudgetMs > net::kDefaultNetworkConfig.feedDeadlineMs);
}

void testSaturation() {
  FakeConnection c;
  std::vector<std::vector<uint8_t>> bodies;
  addEight(c, bodies);
  RecordingSink sink;
  Buf b(8192);
  TransportCounters tc{};
  tc.pollCycles = 0xFFFFFFFFu;
  tc.feedsProcessed = 0xFFFFFFFEu;
  tc.bytesDownloaded = 0xFFFFFFFFFFFFFF00ull;
  runPollCycle(sink, c, b.fb(), kTestFeeds, 8, kReady, false, tc);
  CHECK(tc.pollCycles == 0xFFFFFFFFu);
  CHECK(tc.feedsProcessed == 0xFFFFFFFFu);
  CHECK(tc.bytesDownloaded == 0xFFFFFFFFFFFFFFFFull);
}

void testScheduler() {
  PollScheduler s;
  s.configure(30000);
  CHECK(s.due(0) && s.due(123456));  // first cycle immediately
  s.cycleFinished(1000);
  CHECK(!s.due(1000) && !s.due(30999) && s.due(31000) && s.due(40000));
  // Interval counts from the END of the previous cycle: no overlap/tight loop.
  s.cycleFinished(45000);
  CHECK(!s.due(45001) && !s.due(74999) && s.due(75000));
  // Wrap-around of the 32-bit millisecond counter.
  s.cycleFinished(0xFFFFFF00u);
  CHECK(!s.due(0xFFFFFF00u) && !s.due(29999 - 0x100u) && s.due(0xFFFFFF00u + 30000u));
  CHECK(!s.due(0x1000u) && s.due(0x8000u));
  // Minimum interval clamp.
  s.configure(1);
  CHECK(s.intervalMs() == net::kMinPollIntervalMs);
  s.configure(60000);
  CHECK(s.intervalMs() == 60000);
}

void testConfig() {
  CHECK(net::kFeedCount == 8);
  for (size_t i = 0; i < net::kFeedCount; ++i) {
    CHECK(strncmp(net::kFeeds[i].url, "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs", 66) == 0);
    CHECK(net::kFeeds[i].name != nullptr && strlen(net::kFeeds[i].name) <= 8);
    for (size_t j = i + 1; j < net::kFeedCount; ++j) CHECK(strcmp(net::kFeeds[i].url, net::kFeeds[j].url) != 0);
  }
  CHECK(net::kFeedBufferCapacity == 524288);
  CHECK(net::kFeedBufferCapacity > 239678u * 2);
  CHECK(net::kDefaultPollIntervalMs == 30000);
  CHECK(!net::kDefaultNetworkConfig.enabled);  // network OFF by default
  CHECK(!net::kDefaultNetworkConfig.acceptUnknownLength);
  CHECK(net::kMinValidUnixTime == 1767225600u);
}

// ---- Replay of recorded raw feeds through the real pipeline -------------------

std::vector<uint8_t> readFile(const char *path) {
  std::vector<uint8_t> v;
  FILE *f = fopen(path, "rb");
  if (!f) return v;
  uint8_t tmp[65536];
  size_t n;
  while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) v.insert(v.end(), tmp, tmp + n);
  fclose(f);
  return v;
}

bool sameFrames(const pl::TrainPipeline &a, const pl::TrainPipeline &b) {
  return memcmp(a.frame().occupancy, b.frame().occupancy, sizeof a.frame().occupancy) == 0 &&
         memcmp(a.rgb().pixels, b.rgb().pixels, sizeof a.rgb().pixels) == 0;
}

pl::TrainPipeline directPipe, transportPipe, partialPipe, expectedPartial;

void testReplay(uint32_t nowUnix, int fileCount, char **files) {
  std::vector<std::vector<uint8_t>> raw;
  size_t largest = 0;
  uint64_t total = 0;
  for (int i = 0; i < fileCount; ++i) {
    raw.push_back(readFile(files[i]));
    CHECK(!raw.back().empty());
    if (raw.back().size() > largest) largest = raw.back().size();
    total += raw.back().size();
  }
  CHECK(raw.size() == net::kFeedCount);
  if (raw.size() != net::kFeedCount) return;

  // Reference: feed the files straight into a pipeline.
  directPipe.beginCycle(nowUnix);
  for (auto &r : raw) (void)directPipe.processFeed(r.data(), r.size());
  directPipe.finishCycle();

  // Through the transport: eight URLs, 1460 B reads, one 512 KiB buffer.
  FakeConnection c;
  for (size_t i = 0; i < raw.size(); ++i) c.add(net::kFeeds[i].url, okScript(raw[i]));
  std::vector<uint8_t> mem(net::kFeedBufferCapacity);
  TransportCounters tc{};
  PipelineSink sink(transportPipe);
  sink.setNow(nowUnix);
  CycleResult r = runPollCycle(sink, c, FeedBuffer{mem.data(), mem.size()}, net::kFeeds,
                               net::kFeedCount, kReady, false, tc);
  CHECK(r.outcome == CycleOutcome::Complete && r.processed == 8);
  CHECK(tc.bytesDownloaded == total);
  CHECK(sameFrames(directPipe, transportPipe));
  CHECK(transportPipe.counters().feedsProcessed == 8);
  CHECK(transportPipe.counters().vehiclesSeen == directPipe.counters().vehiclesSeen);
  CHECK(transportPipe.counters().feedDecodeFailures == 0);
  printf("REPLAY via transport: feeds=8 bytes=%llu largest=%zu capacity=%zu resolvedTrains=%u "
         "occupiedLeds=%u collisionLeds=%u -> frames IDENTICAL to direct pipeline\n",
         static_cast<unsigned long long>(total), largest, mem.size(),
         static_cast<unsigned>(transportPipe.counters().resolvedTrains),
         static_cast<unsigned>(transportPipe.counters().occupiedLeds),
         static_cast<unsigned>(transportPipe.counters().collisionLeds));

  // Buffer too small for the largest feed: that feed is rejected whole (never
  // truncated, never decoded) and everything else still matches a reference
  // built from the remaining feeds only.
  const size_t small = largest - 1;
  std::vector<uint8_t> smallMem(small);
  TransportCounters tc2{};
  PipelineSink psink(partialPipe);
  psink.setNow(nowUnix);
  CycleResult pr = runPollCycle(psink, c, FeedBuffer{smallMem.data(), small}, net::kFeeds,
                                net::kFeedCount, kReady, false, tc2);
  CHECK(pr.outcome == CycleOutcome::Partial && pr.processed == 7);
  CHECK(tc2.feedTooLarge == 1);
  CHECK(partialPipe.counters().feedDecodeFailures == 0);  // no partial protobuf reached the decoder
  expectedPartial.beginCycle(nowUnix);
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i].size() == largest) continue;
    (void)expectedPartial.processFeed(raw[i].data(), raw[i].size());
  }
  expectedPartial.finishCycle();
  CHECK(sameFrames(expectedPartial, partialPipe));
  CHECK(partialPipe.counters().feedsProcessed == 7);

  // Exactly-fitting buffer succeeds.
  std::vector<uint8_t> exactMem(largest);
  TransportCounters tc3{};
  PipelineSink esink(transportPipe);
  esink.setNow(nowUnix);
  CycleResult er = runPollCycle(esink, c, FeedBuffer{exactMem.data(), largest}, net::kFeeds,
                                net::kFeedCount, kReady, false, tc3);
  CHECK(er.outcome == CycleOutcome::Complete && sameFrames(directPipe, transportPipe));
}

}  // namespace

int main(int argc, char **argv) {
  testConfig();
  testExactAndZero();
  testCapacityBoundary();
  testPartialAndErrors();
  testOpenFailures();
  testUnknownLengthPolicy();
  testBufferUnavailable();
  testChunkSizes();
  testEightSequential();
  testEachFeedFailing();
  testSpecificCounters();
  testSkippedCycles();
  testFailedCycleDoesNotAffectNext();
  testBufferReuseNoStaleData();
  testSaturation();
  testBudget();
  testScheduler();
  if (argc >= 3) {
    testReplay(static_cast<uint32_t>(strtoul(argv[1], nullptr, 10)), argc - 2, argv + 2);
  }
  printf("sizeof(TransportCounters)=%zu sizeof(PollScheduler)=%zu sizeof(CycleResult)=%zu\n",
         sizeof(TransportCounters), sizeof(PollScheduler), sizeof(CycleResult));
  printf("Phase 4J transport host tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
