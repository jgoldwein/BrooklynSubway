// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "feed_transport.h"

namespace brooklyn::transport {

namespace {

void bump(uint32_t &counter) {
  if (counter != 0xFFFFFFFFu) ++counter;
}

void addBytes(uint64_t &counter, uint64_t n) {
  counter = (0xFFFFFFFFFFFFFFFFull - counter < n) ? 0xFFFFFFFFFFFFFFFFull : counter + n;
}

DownloadStatus fromOpen(OpenResult result) {
  switch (result) {
    case OpenResult::Ok: return DownloadStatus::Ok;
    case OpenResult::WifiUnavailable: return DownloadStatus::WifiUnavailable;
    case OpenResult::SecureConnectFailure: return DownloadStatus::SecureConnectFailure;
    case OpenResult::Timeout: return DownloadStatus::Timeout;
    case OpenResult::HttpFailure: break;
  }
  return DownloadStatus::HttpFailure;
}

DownloadStatus fromRead(int32_t code) {
  return code == kReadTimeout ? DownloadStatus::Timeout : DownloadStatus::Partial;
}

// Body download proper; the caller closes the connection.
DownloadStatus readBody(FeedConnection &connection, const FeedBuffer &buffer,
                        int32_t contentLength, size_t &length) {
  const bool known = contentLength >= 0;
  if (known && contentLength == 0) return DownloadStatus::ZeroLength;
  if (known && static_cast<size_t>(contentLength) > buffer.capacity) {
    return DownloadStatus::TooLarge;  // rejected before any body byte is read
  }
  const size_t target = known ? static_cast<size_t>(contentLength) : buffer.capacity;

  size_t total = 0;
  while (total < target) {
    const size_t want = target - total;
    const int32_t got = connection.read(buffer.data + total, want);
    if (got > 0) {
      if (static_cast<size_t>(got) > want) return DownloadStatus::Partial;  // broken adapter
      total += static_cast<size_t>(got);
    } else if (got == kReadEof) {
      break;
    } else {
      return fromRead(got);
    }
  }

  if (known) {
    if (total != target) return DownloadStatus::Partial;  // closed early
  } else {
    if (total == buffer.capacity) {
      // Buffer is full: any further byte means the body does not fit.
      uint8_t probe = 0;
      const int32_t got = connection.read(&probe, 1);
      if (got > 0) return DownloadStatus::TooLarge;
      if (got != kReadEof) return fromRead(got);
    }
    if (total == 0) return DownloadStatus::ZeroLength;
  }
  length = total;
  return DownloadStatus::Ok;
}

}  // namespace

DownloadStatus downloadFeed(FeedConnection &connection, const char *url,
                            const FeedBuffer &buffer, bool acceptUnknownLength,
                            size_t &length) {
  length = 0;
  if (buffer.data == nullptr || buffer.capacity == 0) return DownloadStatus::BufferUnavailable;

  int32_t contentLength = kUnknownLength;
  const OpenResult opened = connection.open(url, contentLength);
  if (opened != OpenResult::Ok) {
    connection.close();
    return fromOpen(opened);
  }

  DownloadStatus status;
  if (contentLength < 0 && !acceptUnknownLength) {
    status = DownloadStatus::UnknownLength;
  } else {
    status = readBody(connection, buffer, contentLength, length);
  }
  connection.close();
  if (status != DownloadStatus::Ok) length = 0;
  return status;
}

void countDownloadFailure(DownloadStatus status, TransportCounters &counters) {
  switch (status) {
    case DownloadStatus::Ok: break;
    case DownloadStatus::WifiUnavailable: bump(counters.wifiUnavailable); break;
    case DownloadStatus::SecureConnectFailure: bump(counters.secureConnectFailures); break;
    case DownloadStatus::HttpFailure: bump(counters.httpFailures); break;
    case DownloadStatus::Timeout: bump(counters.timeouts); break;
    case DownloadStatus::ZeroLength: bump(counters.zeroLength); break;
    case DownloadStatus::TooLarge: bump(counters.feedTooLarge); break;
    case DownloadStatus::Partial: bump(counters.partialDownload); break;
    case DownloadStatus::UnknownLength: bump(counters.unknownLength); break;
    case DownloadStatus::BufferUnavailable: bump(counters.bufferUnavailable); break;
  }
}

PipelineSink::PipelineSink(pipeline::TrainPipeline &pipeline,
                           const semantic::RenderConfig &config)
    : pipeline_(pipeline), config_(config), nowUnix_(0) {}

void PipelineSink::beginCycle() { pipeline_.beginCycle(nowUnix_, config_); }

void PipelineSink::processFeed(const uint8_t *data, size_t length) {
  (void)pipeline_.processFeed(data, length);  // decode status is counted by the pipeline
}

void PipelineSink::finishCycle() { pipeline_.finishCycle(); }

CycleResult runPollCycle(FeedSink &sink, FeedConnection &connection,
                         const FeedBuffer &buffer, const network::FeedSpec *feeds,
                         size_t feedCount, const PollPreconditions &pre,
                         bool acceptUnknownLength, TransportCounters &counters,
                         const CycleBudget &budget) {
  CycleResult result{CycleOutcome::NoFeeds, 0, 0, 0, 0, 0};

  if (buffer.data == nullptr || buffer.capacity == 0) {
    bump(counters.bufferUnavailable);
    result.outcome = CycleOutcome::SkippedBuffer;
    return result;
  }
  if (!pre.wifiUp) {
    bump(counters.wifiUnavailable);
    result.outcome = CycleOutcome::SkippedWifi;
    return result;
  }
  if (!pre.timeValid) {
    bump(counters.timeUnavailable);
    result.outcome = CycleOutcome::SkippedTime;
    return result;
  }

  bump(counters.pollCycles);
  sink.beginCycle();
  const uint32_t cycleStartMs = budget.clock != nullptr ? budget.clock->nowMs() : 0;
  bool budgetExpired = false;
  for (size_t i = 0; i < feedCount; ++i) {
    if (budget.clock != nullptr &&
        static_cast<uint32_t>(budget.clock->nowMs() - cycleStartMs) >= budget.budgetMs) {
      // Budget gone: start no further downloads; count every remaining feed.
      if (!budgetExpired) bump(counters.cycleBudgetExceeded);
      budgetExpired = true;
      bump(counters.feedsSkippedByBudget);
      if (result.skippedByBudget != 0xFF) ++result.skippedByBudget;
      continue;
    }
    bump(counters.feedsAttempted);
    if (result.attempted != 0xFF) ++result.attempted;

    size_t length = 0;
    const DownloadStatus status =
        downloadFeed(connection, feeds[i].url, buffer, acceptUnknownLength, length);
    if (status != DownloadStatus::Ok) {
      countDownloadFailure(status, counters);
      continue;
    }
    bump(counters.feedsDownloaded);
    addBytes(counters.bytesDownloaded, length);
    if (result.downloaded != 0xFF) ++result.downloaded;

    sink.processFeed(buffer.data, length);
    bump(counters.feedsProcessed);
    if (result.processed != 0xFF) ++result.processed;
    if (i < 16) result.processedMask = static_cast<uint16_t>(result.processedMask | (1u << i));
  }
  sink.finishCycle();

  if (result.processed == feedCount && feedCount != 0) {
    result.outcome = CycleOutcome::Complete;
  } else if (result.processed == 0) {
    result.outcome = CycleOutcome::NoFeeds;
    bump(counters.emptyCycles);
  } else {
    result.outcome = CycleOutcome::Partial;
    bump(counters.partialCycles);
  }
  return result;
}

void PollScheduler::configure(uint32_t intervalMs) {
  interval_ = intervalMs < network::kMinPollIntervalMs ? network::kMinPollIntervalMs : intervalMs;
}

bool PollScheduler::due(uint32_t nowMs) const {
  if (!started_) return true;
  return static_cast<uint32_t>(nowMs - lastEndMs_) >= interval_;
}

void PollScheduler::cycleFinished(uint32_t nowMs) {
  lastEndMs_ = nowMs;
  started_ = true;
}

}  // namespace brooklyn::transport
