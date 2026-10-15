// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4J: transport-independent feed download and poll-cycle core.
//
// No Arduino, Wi-Fi or TLS code lives here, so everything is host-testable. The
// ESP32 layer (network_service.cpp) supplies a FeedConnection backed by
// WiFiClientSecure/HTTPClient.
//
// Contract
//   * One caller-owned buffer is reused for every feed (never copied, no heap).
//   * A buffer is handed to the sink ONLY after a complete, successful download:
//     the whole body is present, it fits the buffer, and it is not empty.
//     Oversized, truncated, timed-out or failed transfers are never passed on
//     and never truncated; they only increment transport counters.
//   * Transport failures are separate from GTFS-RT decode failures; the latter
//     are counted by the pipeline.
//   * Feeds are processed sequentially. A failed feed does not stop the others.
//   * Partial cycle: every feed that downloaded is processed and the cycle is
//     finished, so trains of unavailable feeds are simply absent. The result
//     says so explicitly (CycleOutcome::Partial / NoFeeds); missing feeds are
//     never treated as complete data and nothing is cached between cycles.

#pragma once

#include "network_config.h"
#include "train_pipeline.h"

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::transport {

// ---- Connection abstraction -------------------------------------------------

enum class OpenResult : uint8_t { Ok, WifiUnavailable, SecureConnectFailure, HttpFailure, Timeout };

constexpr int32_t kUnknownLength = -1;  // no usable Content-Length

// read() return codes other than a positive byte count.
constexpr int32_t kReadEof = 0;       // clean end of the body
constexpr int32_t kReadTimeout = -1;  // no data within the idle/deadline limit
constexpr int32_t kReadError = -2;    // connection lost / TLS error

class FeedConnection {
 public:
  virtual ~FeedConnection() = default;
  // Starts a GET. On Ok, contentLength is the HTTP Content-Length or kUnknownLength.
  // Non-200 statuses, connect/TLS failures and timeouts are reported here.
  virtual OpenResult open(const char *url, int32_t &contentLength) = 0;
  // Returns >0 bytes read (never more than maxLen), kReadEof, kReadTimeout or
  // kReadError. 0 is ONLY a clean end of body, never "no data yet".
  virtual int32_t read(uint8_t *dest, size_t maxLen) = 0;
  virtual void close() = 0;
};

// ---- Counters -----------------------------------------------------------------

// Saturating. No strings.
struct TransportCounters {
  uint32_t pollCycles;        // cycles that began downloading feeds
  uint32_t feedsAttempted;
  uint32_t feedsDownloaded;   // complete bodies
  uint32_t feedsProcessed;    // handed to the sink
  uint32_t wifiUnavailable;   // cycle skipped, or feed failed because Wi-Fi was down
  uint32_t timeUnavailable;   // cycle skipped: clock not synchronized
  uint32_t secureConnectFailures;       // connect / handshake / certificate failure
  uint32_t httpFailures;      // non-200 status or other HTTP failure
  uint32_t timeouts;
  uint32_t zeroLength;
  uint32_t feedTooLarge;
  uint32_t partialDownload;   // connection closed early / lost mid-body
  uint32_t unknownLength;     // no Content-Length and unknown length not accepted
  uint32_t bufferUnavailable; // no feed buffer allocated
  uint32_t partialCycles;     // cycles with some, but not all, feeds processed
  uint32_t emptyCycles;       // cycles where no feed could be processed
  uint32_t cycleBudgetExceeded;   // cycles that ran out of time budget
  uint32_t feedsSkippedByBudget;  // feeds never started because the budget expired
  uint64_t bytesDownloaded;   // bytes of complete bodies only
};

// ---- Single feed download ---------------------------------------------------

struct FeedBuffer {
  uint8_t *data;
  size_t capacity;
};

enum class DownloadStatus : uint8_t {
  Ok,
  WifiUnavailable,
  SecureConnectFailure,
  HttpFailure,
  Timeout,
  ZeroLength,
  TooLarge,
  Partial,
  UnknownLength,
  BufferUnavailable,
};

// Downloads one complete body into `buffer`. `length` is meaningful only on Ok.
// The connection is always closed before returning.
DownloadStatus downloadFeed(FeedConnection &connection, const char *url,
                            const FeedBuffer &buffer, bool acceptUnknownLength,
                            size_t &length);

// Adds the counters implied by a non-Ok status.
void countDownloadFailure(DownloadStatus status, TransportCounters &counters);

// ---- Sink (the pipeline, or a test recorder) --------------------------------

class FeedSink {
 public:
  virtual ~FeedSink() = default;
  virtual void beginCycle() = 0;
  virtual void processFeed(const uint8_t *data, size_t length) = 0;
  virtual void finishCycle() = 0;
};

// Adapts the Phase 4I TrainPipeline. nowUnix is the UTC Unix time for the cycle.
class PipelineSink : public FeedSink {
 public:
  PipelineSink(pipeline::TrainPipeline &pipeline,
               const semantic::RenderConfig &config = semantic::RenderConfig{});
  void setNow(uint32_t nowUnix) { nowUnix_ = nowUnix; }
  void beginCycle() override;
  void processFeed(const uint8_t *data, size_t length) override;
  void finishCycle() override;

 private:
  pipeline::TrainPipeline &pipeline_;
  semantic::RenderConfig config_;
  uint32_t nowUnix_;
};

// ---- Poll cycle ---------------------------------------------------------------

enum class CycleOutcome : uint8_t {
  Complete,        // every feed processed
  Partial,         // some feeds missing; the frame omits their trains
  NoFeeds,         // cycle ran but no feed was processed (frame is empty)
  SkippedBuffer,   // nothing attempted; sink untouched
  SkippedWifi,     // nothing attempted; sink untouched
  SkippedTime,     // nothing attempted; sink untouched
};

struct PollPreconditions {
  bool wifiUp;
  bool timeValid;  // UTC clock synchronized
};

struct CycleResult {
  CycleOutcome outcome;
  uint8_t attempted;
  uint8_t downloaded;
  uint8_t processed;
  uint8_t skippedByBudget;  // feeds never started because the cycle budget expired
  uint16_t processedMask;   // bit i = feed i processed
};

class Clock {
 public:
  virtual ~Clock() = default;
  virtual uint32_t nowMs() const = 0;
};

// Overall time budget for one poll cycle. The budget is checked only BEFORE a
// feed download starts; a download already in progress is bounded by the
// per-feed timeouts, so a cycle can overrun the budget by at most one feed.
// clock == nullptr disables the budget (host tests of the plain cycle only).
struct CycleBudget {
  const Clock *clock;
  uint32_t budgetMs;
  CycleBudget() : clock(nullptr), budgetMs(0) {}
  CycleBudget(const Clock *c, uint32_t ms) : clock(c), budgetMs(ms) {}
};

// Runs one sequential poll cycle. If a precondition fails nothing is attempted
// and the sink (hence the previous pipeline result) is left untouched. Once the
// budget has expired no further feed is started; the cycle is finished with the
// feeds that succeeded (Partial or NoFeeds).
CycleResult runPollCycle(FeedSink &sink, FeedConnection &connection,
                         const FeedBuffer &buffer, const network::FeedSpec *feeds,
                         size_t feedCount, const PollPreconditions &pre,
                         bool acceptUnknownLength, TransportCounters &counters,
                         const CycleBudget &budget = CycleBudget{});

// ---- Scheduler ----------------------------------------------------------------

// Wrap-safe millisecond scheduler: the first cycle is due immediately; each next
// cycle is due `interval` after the previous one ENDED, so cycles never overlap
// and a failing network never produces a tight retry loop.
class PollScheduler {
 public:
  void configure(uint32_t intervalMs);
  bool due(uint32_t nowMs) const;
  void cycleFinished(uint32_t nowMs);
  uint32_t intervalMs() const { return interval_; }

 private:
  uint32_t interval_ = network::kDefaultPollIntervalMs;
  uint32_t lastEndMs_ = 0;
  bool started_ = false;
};

}  // namespace brooklyn::transport
