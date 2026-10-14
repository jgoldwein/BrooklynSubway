// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-only previous-stop provider backed by the TSV written by
// tools/phase4e_build_pattern_index.py (static GTFS stop patterns).
// Not part of the firmware build.

#pragma once

#include "gtfs_rt_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace hostidx {

constexpr int kMaxPatterns = 600;
constexpr int kMaxStops = 96;

struct Pattern {
  char shape[16];
  char route[4];
  char dir;
  uint8_t count;
  char stops[kMaxStops][4];
};

struct Index {
  Pattern *patterns;
  int count;
};

inline bool load(Index &idx, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  idx.patterns = static_cast<Pattern *>(calloc(kMaxPatterns, sizeof(Pattern)));
  idx.count = 0;
  static char line[4096];
  while (fgets(line, sizeof line, f) && idx.count < kMaxPatterns) {
    Pattern &p = idx.patterns[idx.count];
    char *tab1 = strchr(line, '\t');
    if (!tab1) continue;
    char *tab2 = strchr(tab1 + 1, '\t');
    if (!tab2) continue;
    char *tab3 = strchr(tab2 + 1, '\t');
    if (!tab3) continue;
    *tab1 = *tab2 = *tab3 = '\0';
    snprintf(p.shape, sizeof p.shape, "%s", line);
    snprintf(p.route, sizeof p.route, "%s", tab1 + 1);
    p.dir = tab2[1];
    for (char *s = strtok(tab3 + 1, ",\r\n"); s && p.count < kMaxStops; s = strtok(nullptr, ",\r\n"))
      snprintf(p.stops[p.count++], 4, "%s", s);
    ++idx.count;
  }
  fclose(f);
  return true;
}

struct Provider {
  Index index;
  int exact = 0, fallback = 0, none = 0;
};

// Collects the distinct predecessors of `stop` over the given patterns.
inline int predecessors(const Pattern *p, const char *stop, char preds[][4], int cap, int n) {
  for (int i = 1; i < p->count; ++i) {
    if (strcmp(p->stops[i], stop) != 0) continue;
    bool seen = false;
    for (int k = 0; k < n; ++k)
      if (strcmp(preds[k], p->stops[i - 1]) == 0) seen = true;
    if (!seen && n < cap) snprintf(preds[n++], 4, "%s", p->stops[i - 1]);
  }
  return n;
}

inline bool previousStop(void *ctx, const brooklyn::gtfsrt::RtTrain &t, const char *stopId,
                         brooklyn::gtfsrt::PreviousStop &out) {
  Provider &pv = *static_cast<Provider *>(ctx);
  if (strlen(stopId) < 3) return false;
  char parent[4] = {stopId[0], stopId[1], stopId[2], '\0'};
  const char dir = brooklyn::gtfsrt::travelDirection(t);
  const char *us = strchr(t.tripId, '_');
  const char *shape = us ? us + 1 : t.tripId;

  char preds[16][4];
  int n = 0;
  for (int i = 0; i < pv.index.count; ++i)
    if (strcmp(pv.index.patterns[i].shape, shape) == 0)
      n = predecessors(&pv.index.patterns[i], parent, preds, 16, n);
  if (n == 1) {
    snprintf(out.stopId, sizeof out.stopId, "%s", preds[0]);
    out.source = brooklyn::gtfsrt::PreviousSource::ExactTrip;
    ++pv.exact;
    return true;
  }
  if (n == 0 && (dir == 'N' || dir == 'S')) {
    for (int i = 0; i < pv.index.count; ++i) {
      const Pattern &p = pv.index.patterns[i];
      if (p.dir == dir && strcmp(p.route, t.routeId) == 0)
        n = predecessors(&p, parent, preds, 16, n);
    }
    if (n == 1) {
      snprintf(out.stopId, sizeof out.stopId, "%s", preds[0]);
      out.source = brooklyn::gtfsrt::PreviousSource::RouteFallback;
      ++pv.fallback;
      return true;
    }
  }
  ++pv.none;
  return false;
}

}  // namespace hostidx
