// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4J: network/transport configuration. Pure constants; no hardware.
//
// The transport knows only URLs and a short diagnostic name. It has no knowledge
// of routes, stations, LEDs or train positions (those live in the pipeline).

#pragma once

#include <stddef.h>
#include <stdint.h>

// Live polling is OFF unless a build explicitly passes -DBROOKLYN_NETWORK_ENABLED=1.
#ifndef BROOKLYN_NETWORK_ENABLED
#define BROOKLYN_NETWORK_ENABLED 0
#endif

namespace brooklyn::network {

struct FeedSpec {
  const char *name;  // short diagnostic label only
  const char *url;
};

// The eight official MTA subway GTFS-RT feeds (no API key required), the same
// set validated in Phase 4E. Keep identical to tools/phase4j_fetch_check.py.
constexpr FeedSpec kFeeds[] = {
    {"1-7,S", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs"},
    {"ACE", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace"},
    {"BDFM", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-bdfm"},
    {"G", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-g"},
    {"JZ", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-jz"},
    {"NQRW", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-nqrw"},
    {"L", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-l"},
    {"SI", "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-si"},
};
constexpr size_t kFeedCount = sizeof(kFeeds) / sizeof(kFeeds[0]);

// Raw-feed buffer: ONE buffer, reused for the eight feeds in sequence.
// Largest feed seen so far: 251,427 B (host fetch 2026-10-14 16:49 America/New_York;
// earlier recordings peaked at 239,678 B). 512 KiB is 2.09x that peak.
constexpr size_t kFeedBufferCapacity = 512u * 1024u;

// MTA feeds refresh roughly every 30 s; polling faster only repeats data.
constexpr uint32_t kDefaultPollIntervalMs = 30000;
constexpr uint32_t kMinPollIntervalMs = 15000;

// Unix time before which the system clock is considered unset (2026-01-01 UTC).
constexpr uint32_t kMinValidUnixTime = 1767225600u;

struct NetworkConfig {
  bool enabled;                // false: never touches Wi-Fi, SNTP or the buffer
  uint32_t pollIntervalMs;     // measured from the end of one cycle to the next start
  // A body without Content-Length cannot be proven complete, so it is rejected
  // unless this is true.
  bool acceptUnknownLength;
  uint32_t connectTimeoutMs;   // TCP connect and, separately, TLS handshake
  uint32_t readIdleTimeoutMs;  // max wait with no new body bytes
  uint32_t feedDeadlineMs;     // per-feed limit on the body transfer, measured from request start
  uint32_t cycleBudgetMs;      // overall budget: no new feed starts once it has elapsed
  uint32_t wifiRetryMs;        // minimum spacing of Wi-Fi association attempts
  uint32_t timeSyncTimeoutMs;  // SNTP wait before the attempt is retried
};

constexpr NetworkConfig kDefaultNetworkConfig = {
    BROOKLYN_NETWORK_ENABLED != 0,
    kDefaultPollIntervalMs,
    false,
    8000,
    8000,
    12000,
    60000,
    30000,
    20000,
};

}  // namespace brooklyn::network
