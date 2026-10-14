// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "previous_stop.h"

#include "generated/gtfs_topology_table.h"

#include <string.h>

namespace brooklyn::gtfs_topology {

namespace {

bool validChar(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'); }

int b36Digit(char c) { return c <= '9' ? c - '0' : c - 'A' + 10; }

// Parses a parent ("A41") or platform ("A41N"/"A41S") id. Returns false for
// anything else. `suffix` is 'N', 'S' or 0.
bool parseStop(const char *id, char parent[4], char &suffix) {
  if (id == nullptr) return false;
  const size_t n = strnlen(id, 8);
  if (n != 3 && n != 4) return false;
  for (int i = 0; i < 3; ++i)
    if (!validChar(id[i])) return false;
  suffix = 0;
  if (n == 4) {
    if (id[3] != 'N' && id[3] != 'S') return false;
    suffix = id[3];
  }
  memcpy(parent, id, 3);
  parent[3] = '\0';
  return true;
}

uint16_t pack(const char *p) {
  return static_cast<uint16_t>((b36Digit(p[0]) * 36 + b36Digit(p[1])) * 36 + b36Digit(p[2]));
}

void unpack(uint16_t v, char out[4]) {
  static const char kDigits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  out[2] = kDigits[v % 36];
  out[1] = kDigits[(v / 36) % 36];
  out[0] = kDigits[v / 1296];
  out[3] = '\0';
}

int routeIndex(const char *routeId) {
  if (routeId == nullptr) return -1;
  for (uint8_t i = 0; i < kTopologyRouteCount; ++i)
    if (strcmp(kTopologyRoutes[i], routeId) == 0) return i;
  return -1;
}

const TopologyEntry *findEntry(uint8_t routeDir, uint16_t dest) {
  int lo = 0, hi = static_cast<int>(kTopologyEntryCount) - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const TopologyEntry &e = kTopology[mid];
    if (e.routeDir == routeDir && e.dest == dest) return &e;
    if (e.routeDir < routeDir || (e.routeDir == routeDir && e.dest < dest))
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return nullptr;
}

PreviousStopResult make(PrevStatus s) {
  PreviousStopResult r;
  memset(&r, 0, sizeof r);
  r.status = s;
  r.source = PrevSource::None;
  return r;
}

struct Query {
  PrevStatus error;  // Match when the query is well-formed
  int route;
  char parent[4];
  char dir;
};

Query parseQuery(const char *routeId, const char *stopId, char direction) {
  Query q{};
  q.error = PrevStatus::Invalid;
  if (routeId == nullptr || routeId[0] == '\0') return q;
  char suffix = 0;
  if (!parseStop(stopId, q.parent, suffix)) return q;
  if (direction != 0 && direction != 'N' && direction != 'S') return q;
  if (direction != 0 && suffix != 0 && direction != suffix) return q;
  q.dir = direction != 0 ? direction : suffix;
  if (q.dir == 0) return q;
  q.route = routeIndex(routeId);
  q.error = q.route < 0 ? PrevStatus::UnsupportedRoute : PrevStatus::Match;
  return q;
}

}  // namespace

PreviousStopResult previousStopFromTopology(const char *routeId, const char *stopId,
                                            char direction) {
  const Query q = parseQuery(routeId, stopId, direction);
  if (q.error != PrevStatus::Match) return make(q.error);

  const TopologyEntry *e = findEntry(static_cast<uint8_t>(q.route * 2 + (q.dir == 'S' ? 1 : 0)),
                                     pack(q.parent));
  if (e == nullptr) return make(PrevStatus::UnknownStop);

  PreviousStopResult r = make(PrevStatus::NoPreviousStop);
  r.originPossible = (e->flags & kTopologyOriginFlag) != 0;
  const uint8_t n = e->flags & kTopologyCountMask;
  if (n == 0) return r;
  if (n > kMaxCandidates) return make(PrevStatus::Invalid);
  r.source = PrevSource::StaticTopology;
  r.count = n;
  for (uint8_t i = 0; i < n; ++i) unpack(kTopologyPredecessors[e->predOff + i], r.candidates[i]);
  r.status = n == 1 ? PrevStatus::Match : PrevStatus::Ambiguous;
  return r;
}

PreviousStopResult previousStopForRealtimeState(const char *routeId, const char *stopId,
                                                char direction,
                                                const char *realtimePrevStopId) {
  PreviousStopResult st = previousStopFromTopology(routeId, stopId, direction);
  if (realtimePrevStopId == nullptr || realtimePrevStopId[0] == '\0') return st;
  if (st.status == PrevStatus::Invalid || st.status == PrevStatus::UnsupportedRoute) return st;

  char rt[4];
  char suffix = 0;
  if (!parseStop(realtimePrevStopId, rt, suffix)) return make(PrevStatus::Invalid);

  PreviousStopResult r = make(PrevStatus::Match);
  r.source = PrevSource::Realtime;
  r.count = 1;
  memcpy(r.candidates[0], rt, 4);
  r.originPossible = st.originPossible;
  if (st.count > 0) {
    bool listed = false;
    for (uint8_t i = 0; i < st.count; ++i)
      if (strcmp(st.candidates[i], rt) == 0) listed = true;
    r.disagreesWithStatic = !listed;
  }
  return r;
}

const char *prevStatusName(PrevStatus s) {
  switch (s) {
    case PrevStatus::Match: return "Match";
    case PrevStatus::NoPreviousStop: return "NoPreviousStop";
    case PrevStatus::Ambiguous: return "Ambiguous";
    case PrevStatus::UnknownStop: return "UnknownStop";
    case PrevStatus::UnsupportedRoute: return "UnsupportedRoute";
    case PrevStatus::Invalid: return "Invalid";
  }
  return "?";
}

}  // namespace brooklyn::gtfs_topology
