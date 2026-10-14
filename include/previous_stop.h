// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

// Phase 4F: firmware-self-contained previous-stop resolution.
//
// GTFS-RT never sends past stops, so the stop a train just left comes from
// static topology compiled into generated/gtfs_topology_table.cpp
// ((route, direction, destination) -> distinct predecessor parent stops).
// A realtime-supplied earlier stop, when present, takes precedence.
//
// Pure software: no heap, no LED/semantic knowledge, no GPIO. Ambiguity is
// reported, never silently resolved.
namespace brooklyn::gtfs_topology {

enum class PrevStatus : uint8_t {
  Match,             // exactly one previous stop
  NoPreviousStop,    // destination only ever starts patterns on this route/direction
  Ambiguous,         // several static predecessors; candidates are returned
  UnknownStop,       // (route, direction, destination) not in the compiled table
  UnsupportedRoute,  // route id not covered by the table
  Invalid,           // null/malformed id, bad direction, or contradictory direction
};

enum class PrevSource : uint8_t { None, Realtime, StaticTopology };

constexpr uint8_t kMaxCandidates = 4;

struct PreviousStopResult {
  PrevStatus status;
  PrevSource source;
  uint8_t count;                  // Match: 1; Ambiguous: >= 2; otherwise 0
  char candidates[kMaxCandidates][4];  // 3-char parent ids, NUL terminated, sorted
  bool originPossible;            // destination can also be a pattern origin
  bool disagreesWithStatic;       // realtime stop absent from the static predecessors
};

// routeId       GTFS route id as sent by the feed ("A", "FS", "H", ...)
// stopId        stop the train is heading to; parent ("A41") or platform ("A41N")
// direction     'N' or 'S'; 0 derives it from the platform suffix of stopId
// realtimePrevStopId
//               nullable parent/platform id of a stop the realtime feed says the
//               train already left. A well-formed value wins over the static
//               table; if the table lists predecessors and excludes it,
//               disagreesWithStatic is set.
PreviousStopResult previousStopForRealtimeState(const char *routeId, const char *stopId,
                                                char direction,
                                                const char *realtimePrevStopId);

// Static-topology-only lookup (realtime stop absent).
PreviousStopResult previousStopFromTopology(const char *routeId, const char *stopId,
                                            char direction);

const char *prevStatusName(PrevStatus s);

}  // namespace brooklyn::gtfs_topology
