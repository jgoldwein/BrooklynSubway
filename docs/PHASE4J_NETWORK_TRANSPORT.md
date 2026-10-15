# Phase 4J — Network transport for GTFS-RT feeds

**Status: implemented and host-tested. NOT validated on hardware.**

> The ESP32-S3 was not connected during Phase 4J. Wi-Fi association, SNTP,
> certificate validation and HTTPS transfer have therefore not yet been
> validated on the physical board.

Networking is **disabled by default** (`BROOKLYN_NETWORK_ENABLED=0`). No LED
output exists in this phase (`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`); no GPIO,
`LED_ENABLE`, LED ordering or channel code was touched.

## Architecture

| Layer | Files | Notes |
|---|---|---|
| Config | `include/network_config.h` | Feed table, buffer size, intervals, defaults |
| Core (host-testable) | `include/feed_transport.h`, `src/feed_transport.cpp` | `FeedConnection` abstraction, `downloadFeed`, `runPollCycle`, `PollScheduler`, `PipelineSink` |
| ESP32 layer | `include/network_service.h`, `src/network_service.cpp` | Wi-Fi STA, SNTP, PSRAM buffer, `HttpsFeedConnection` (WiFiClientSecure + HTTPClient) |
| Tests | `tools/host_tests/feed_transport_test.cpp`, `tools/run_feed_transport_tests.py` | Scripted fake connections; optional replay of recorded feeds |
| Host tool | `tools/phase4j_fetch_check.py` | Fetches the 8 feeds and reports sizes |

Libraries: only those in the Arduino-ESP32 framework (WiFi, WiFiClientSecure,
HTTPClient). No new dependency.

`main.cpp` only constructs a `NetworkService`, calls `begin()` (prints the
network state) and `update(millis())` in `loop()`. The Phase 4I empty-cycle
self-check is unchanged.

## Feed buffer

* ONE buffer, reused for the eight feeds sequentially; no copy, no heap churn.
* Capacity **512 KiB (524,288 B)**, allocated once, only when networking is
  enabled and credentials exist, with
  `heap_caps_malloc(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.
* The pointer is verified with `esp_ptr_external_ram()`. There is **no fallback
  to internal RAM**. `CONFIG_SPIRAM_IGNORE_NOTFOUND=1` makes a missing PSRAM
  silent at boot, so `psramFound()` is also checked. On failure the cycle is
  skipped and `bufferUnavailable` is counted.
* Both targets build with PSRAM enabled: brooklyn-s3 (`qio_opi`, 16 MB flash,
  8 MB OPI PSRAM) and xiao-s3 (board JSON `qio_opi`, 8 MB flash, 8 MB OPI PSRAM).
* PlatformIO's "RAM" figure **excludes PSRAM**. Static PSRAM use is 0 B; the
  dynamic allocation is 524,288 B (about 6% of 8 MB) only when enabled.

### Sizes (host fetch 2026-10-14 16:49:57–16:50:42 America/New_York)

All eight: HTTP 200, `Content-Length` present, no `Transfer-Encoding`.

| Feed | Bytes |
|---|---|
| `nyct/gtfs` | 251,427 |
| `gtfs-ace` | 130,535 |
| `gtfs-bdfm` | 200,404 |
| `gtfs-g` | 18,769 |
| `gtfs-jz` | 30,568 |
| `gtfs-nqrw` | 147,721 |
| `gtfs-l` | 34,040 |
| `gtfs-si` | 10,861 |

Total 824,325 B; largest 251,427 B (new maximum; previous recorded maximum
239,678 B); smallest 10,861 B (historical smallest 5,147 B over 80 files).
512 KiB is 2.09× the largest observed feed.

### Overflow behaviour

A known `Content-Length` above capacity is rejected before any body byte is
read. For an unknown length (only if explicitly accepted) the download stops at
capacity and probes one more byte; if more data exists the feed is `TooLarge`.
Oversized feeds are never truncated and never decoded; `feedTooLarge` is
counted and the cycle is `Partial`.

## HTTPS and certificates

* `WiFiClientSecure::setCACertBundle()` with the framework's embedded CA bundle
  (`_binary_x509_crt_bundle_start`, verified linked, ~63 KB flash). **`setInsecure()`
  is never used.**
* Host-observed chain for `api-endpoint.mta.info`: `*.mta.info` ←
  Amazon RSA 2048 M04 ← **Amazon Root CA 1** (self-signed), TLS 1.3. The bundle
  contains Amazon Root CA 1. This is a maintained CA bundle, not a pinned leaf.
* HTTP/1.0 is requested (no chunked bodies), one connection per feed.
* HTTPClient does not distinguish connect refusal from TLS/certificate failure;
  both are reported as `secureConnectFailures`.
* TLS heap usage was **not measured** (no hardware). mbedTLS buffers stay in
  internal RAM (`MBEDTLS_INTERNAL_MEM_ALLOC=1`).

## Time (SNTP)

`configTime(0, 0, "pool.ntp.org", "time.google.com")`: UTC, no zone/DST offsets.
The clock is valid once `time(nullptr) >= 1767225600` (2026-01-01 UTC). HTTPS
polling happens only after Wi-Fi is up and the clock is valid. The pipeline
works only with Unix time.

## Secrets

`include/secrets.h` (git-ignored) defines `BROOKLYN_WIFI_SSID` and
`BROOKLYN_WIFI_PASSWORD`; start from the tracked `include/secrets.example.h`.
The repository builds without it; with no SSID the state is `NO CREDENTIALS`
and nothing network-related runs. No API key is needed for these feeds.

## Enabling

Default off. To opt in for a bench build add `-DBROOKLYN_NETWORK_ENABLED=1` to
the build flags and provide `secrets.h`. `platformio.ini` is unchanged.

## Polling and cycle semantics

* Default interval 30 s, measured from the **end** of the previous cycle (no
  overlap, no tight loop); minimum clamp 15 s; wrap-safe `millis()` handling.
* Wi-Fi association retries are spaced by `wifiRetryMs` (30 s).
* Preconditions are checked in order buffer → Wi-Fi → time. A skipped cycle
  leaves the pipeline and the previous result untouched.
* Feeds download and process sequentially. A failed feed never stops the others.
* **Partial cycle:** feeds that downloaded completely are processed; trains of
  unavailable feeds are absent. The outcome is reported explicitly
  (`COMPLETE`, `PARTIAL`, `NO FEEDS`); partial data is never presented as complete,
  and there is no stale-feed cache.
* A buffer reaches the decoder only after a complete download (exact
  `Content-Length`, non-zero, fits capacity). No partial protobuf is ever decoded.
* Timeouts: connect/handshake 10 s, idle read 8 s, per-feed deadline 30 s.

## Counters

`TransportCounters` (72 B, saturating): pollCycles, feedsAttempted,
feedsDownloaded, feedsProcessed, wifiUnavailable, timeUnavailable, secureConnectFailures,
httpFailures, timeouts, zeroLength, feedTooLarge, partialDownload,
unknownLength, bufferUnavailable, partialCycles, emptyCycles, bytesDownloaded
(complete bodies only). Transport failures are separate from the pipeline's GTFS
decode failures.

## Limitations

* `update()` blocks `loop()` during a poll (worst case roughly 8 × 30 s). A
  FreeRTOS task is a possible later improvement.
* Chunked responses are unsupported (HTTP/1.0 requested).
* A transfer cut off exactly at a boundary matching `Content-Length` cannot be
  detected, but a truncated body cannot satisfy `Content-Length`.
* Nothing in the network path has run on hardware.

## Validation

* Host transport tests: `python tools/run_feed_transport_tests.py [--replay DIR@UNIX]`
  (fake connections for success, zero-length, exact capacity ±1, partial,
  timeout, error, every open failure, unknown length, each of 8 feeds failing in
  9 ways, counters/saturation, skipped cycles, buffer reuse, scheduler wrap).
* Replay: the eight recorded feeds served in 1,460-byte chunks through the
  transport into the real pipeline produce a `TrainFrame` and `RgbFrame`
  identical to feeding the files directly. Fresh live feeds (129 resolved trains,
  109 occupied LEDs, 17 collision LEDs, 0 status LEDs lit) were also identical
  to the reversed-order result.

## Hardening pass: cycle budget, counter rename, enabled builds

**Timeouts (unoptimized estimates, not measured on hardware):** connect 8 s, read-idle 8 s,
body deadline 12 s (from request start, checked in the read loop), cycle budget 60 s.

**Cycle budget.** The budget is checked only *before* a feed starts. Once it has expired no new
download begins; remaining feeds are skipped and counted, and the cycle ends as Partial (some
feeds succeeded) or NoFeeds. Partial protobuf data is never processed. Because the check is
between feeds, a cycle can overrun the budget by at most one feed's worst case (roughly 24-30 s,
an estimate). Phase 4J is still synchronous: `NetworkService::update()` can block `loop()` during
HTTPS. A later runtime phase may move polling off the render path.

**Counters.** New: `cycleBudgetExceeded`, `feedsSkippedByBudget`, `CycleResult.skippedByBudget`.
Skipped feeds are not "attempted" (`feedsAttempted + feedsSkippedByBudget` = feed count).
`tlsFailures` was renamed `secureConnectFailures`: HTTPClient returns only a generic negative
code, so TCP-connect failures and TLS handshake/certificate failures share this bucket. Read
timeouts remain separately classified. `sizeof(TransportCounters)` = 80 B.

**Host results.** Transport tests: 1,209 checks (1,230 with replay), 0 failures, including budget
tests (expiry, skip counts, Partial/NoFeeds, fresh budget next cycle, clock wraparound,
budget 0, partial body never forwarded).
Fresh fetch 2026-10-14 17:41:42-17:41:55 America/New_York: all eight feeds HTTP 200; total
801,067 B; largest 240,824 B (feed.pb), smallest 11,813 B; all fit in 524,288 B.
Replay of that snapshot: 770 vehicles, 581 supported, 133 resolved trains, 113 occupied LEDs,
18 collision LEDs, 0 status LEDs lit, 0 duplicates; frames identical to the direct pipeline and
to reversed feed order.

**Builds.** Default: brooklyn-s3 RAM 34,240 B / flash 306,473 B; xiao-s3 RAM 39,764 B / flash
284,689 B.
Versus Phase 4I (brooklyn-s3 34,120 B / 306,121 B; xiao-s3 39,644 B / 284,329 B): brooklyn-s3
+120 B RAM, +352 B flash; xiao-s3 +120 B RAM, +360 B flash. Versus the pre-hardening Phase 4J
build (34,232 / 306,473 and 39,756 / 284,689): +8 B RAM and +0 B flash on both targets.
Budget estimate: the 24-30 s worst-case one-feed overrun is not hardware-measured.
With `-DBROOKLYN_NETWORK_ENABLED=1`: brooklyn-s3 61,440 B / 973,457 B; xiao-s3
66,992 B / 952,797 B. Both enabled builds link `_binary_x509_crt_bundle_start`; this is a link
check only.

The ESP32-S3 was not connected during Phase 4J. Wi-Fi association, SNTP, certificate validation
and HTTPS transfer have therefore not yet been validated on the physical board.