// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) validation of Phase 4F previous-stop
// resolution. Build/run with: python tools/run_previous_stop_tests.py
//
// 1. API contract tests (statuses, direction rules, realtime-first, disagreement).
// 2. Builder integration through the Phase 4D adapter / Phase 4C resolver.
// 3. Exhaustive comparison with the static GTFS stop-pattern index
//    (optional TSV argument; skipped loudly when it is absent).

#include "generated/gtfs_topology_table.h"
#include "gtfs_rt_state.h"
#include "pattern_index.h"
#include "previous_stop.h"

#include <stdio.h>
#include <string.h>

using namespace brooklyn::gtfs_topology;
using brooklyn::gtfsrt::BuilderConfig;
using brooklyn::gtfsrt::Category;
using brooklyn::gtfsrt::Observation;
using brooklyn::gtfsrt::PreviousFailure;
using brooklyn::gtfsrt::PreviousSource;
using brooklyn::gtfsrt::Reason;
using brooklyn::gtfsrt::RtTrain;
using brooklyn::gtfsrt::VehicleStatus;
namespace sem = brooklyn::semantic;

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

bool hasCandidate(const PreviousStopResult &r, const char *id) {
  for (uint8_t i = 0; i < r.count; ++i)
    if (strcmp(r.candidates[i], id) == 0) return true;
  return false;
}

// ---- 1. API contract -------------------------------------------------------

void testApi() {
  PreviousStopResult r = previousStopFromTopology("2", "232N", 0);
  CHECK(r.status == PrevStatus::Match && r.count == 1 && r.source == PrevSource::StaticTopology);
  CHECK(strcmp(r.candidates[0], "233") == 0);  // Hoyt St -> Borough Hall northbound
  r = previousStopFromTopology("2", "232", 'N');
  CHECK(r.status == PrevStatus::Match && strcmp(r.candidates[0], "233") == 0);
  r = previousStopFromTopology("2", "232S", 0);
  CHECK(r.status == PrevStatus::Match && strcmp(r.candidates[0], "231") == 0);

  // Express/local merge: more than one static predecessor is reported, not chosen.
  r = previousStopFromTopology("A", "A48", 'N');
  CHECK(r.status == PrevStatus::Ambiguous && r.count == 2);
  CHECK(hasCandidate(r, "A49") && hasCandidate(r, "A51"));
  CHECK(strcmp(r.candidates[0], r.candidates[1]) < 0);  // sorted

  // Shuttles keep their own GTFS route ids.
  r = previousStopFromTopology("H", "H14S", 0);
  CHECK(r.status == PrevStatus::Match || r.status == PrevStatus::Ambiguous ||
        r.status == PrevStatus::NoPreviousStop);
  r = previousStopFromTopology("H", "H19N", 0);  // realtime-only stop, not in static data
  CHECK(r.status == PrevStatus::UnknownStop);

  CHECK(previousStopFromTopology("W", "R30N", 0).status == PrevStatus::UnsupportedRoute);
  CHECK(previousStopFromTopology("GS", "901N", 0).status == PrevStatus::UnsupportedRoute);
  CHECK(previousStopFromTopology("1", "101N", 0).status == PrevStatus::UnsupportedRoute);
  CHECK(previousStopFromTopology("A", "ZZZ", 'N').status == PrevStatus::UnknownStop);

  CHECK(previousStopFromTopology(nullptr, "A48", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("", "A48", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", nullptr, 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "A0", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "A48X", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "A48NN", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "a09", 'N').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "A48", 0).status == PrevStatus::Invalid);   // no direction
  CHECK(previousStopFromTopology("A", "A48", 'E').status == PrevStatus::Invalid);
  CHECK(previousStopFromTopology("A", "A48N", 'S').status == PrevStatus::Invalid);  // contradictory
  CHECK(previousStopFromTopology("A", "A48N", 'N').status == PrevStatus::Ambiguous);

  // A key that only starts patterns has no previous stop.
  int origins = 0;
  for (uint16_t i = 0; i < kTopologyEntryCount; ++i) {
    const TopologyEntry &e = kTopology[i];
    if ((e.flags & kTopologyCountMask) != 0) continue;
    char stop[5] = {};
    uint16_t v = e.dest;
    static const char d[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    stop[2] = d[v % 36]; stop[1] = d[(v / 36) % 36]; stop[0] = d[v / 1296];
    r = previousStopFromTopology(kTopologyRoutes[e.routeDir >> 1], stop, (e.routeDir & 1) ? 'S' : 'N');
    CHECK(r.status == PrevStatus::NoPreviousStop && r.count == 0 && r.originPossible);
    ++origins;
  }
  CHECK(origins > 0);

  // Realtime-first: a well-formed realtime stop wins and is flagged when the
  // static table lists predecessors that exclude it.
  r = previousStopForRealtimeState("A", "A48N", 'N', "A49");
  CHECK(r.status == PrevStatus::Match && r.source == PrevSource::Realtime && r.count == 1);
  CHECK(strcmp(r.candidates[0], "A49") == 0 && !r.disagreesWithStatic);  // resolves the ambiguity
  r = previousStopForRealtimeState("A", "A48N", 'N', "A49N");            // platform form accepted
  CHECK(r.status == PrevStatus::Match && strcmp(r.candidates[0], "A49") == 0);
  r = previousStopForRealtimeState("A", "A48N", 'N', "A55");
  CHECK(r.status == PrevStatus::Match && r.source == PrevSource::Realtime && r.disagreesWithStatic);
  r = previousStopForRealtimeState("2", "232N", 'N', "233");
  CHECK(r.status == PrevStatus::Match && !r.disagreesWithStatic);
  r = previousStopForRealtimeState("A", "A48N", 'N', nullptr);  // absent -> static
  CHECK(r.status == PrevStatus::Ambiguous && r.source == PrevSource::StaticTopology);
  r = previousStopForRealtimeState("A", "A48N", 'N', "");
  CHECK(r.status == PrevStatus::Ambiguous);
  r = previousStopForRealtimeState("A", "A48N", 'N', "??");  // malformed realtime stop
  CHECK(r.status == PrevStatus::Invalid);
  r = previousStopForRealtimeState("W", "R30N", 'N', "R31");  // route errors are not overridden
  CHECK(r.status == PrevStatus::UnsupportedRoute);
  r = previousStopForRealtimeState("A", "A48", 0, "A49");     // still needs a direction
  CHECK(r.status == PrevStatus::Invalid);
  r = previousStopForRealtimeState("A", "ZZZ", 'N', "A49");   // realtime works for an uncompiled key
  CHECK(r.status == PrevStatus::Match && r.source == PrevSource::Realtime);

  CHECK(strcmp(prevStatusName(PrevStatus::Ambiguous), "Ambiguous") == 0);
}

// ---- 2. Builder integration ------------------------------------------------

RtTrain train(const char *route, const char *stop, VehicleStatus status) {
  RtTrain t;
  memset(&t, 0, sizeof t);
  snprintf(t.routeId, sizeof t.routeId, "%s", route);
  snprintf(t.tripId, sizeof t.tripId, "%s", "000000_X..N00R");
  snprintf(t.vehicleStopId, sizeof t.vehicleStopId, "%s", stop);
  t.hasVehicle = true;
  t.status = status;
  t.vehicleTimestamp = 1000;
  return t;
}

Observation eval(const RtTrain &t) {
  BuilderConfig c{};
  c.now = 1000;
  c.maxVehicleAgeSec = 300;
  c.previousStop = brooklyn::gtfsrt::topologyPreviousStop;
  Observation o;
  brooklyn::gtfsrt::evaluateTrain(t, 1000, c, o);
  return o;
}

void testBuilder(unsigned &uniqueSeg, unsigned &equivalent, unsigned &differing,
                 unsigned &unknownDest) {
  // Stopped trains never need a previous stop.
  Observation o = eval(train("2", "232N", VehicleStatus::StoppedAt));
  CHECK(o.category == Category::ResolvedStation);

  // Unique static predecessor feeds the adapter.
  o = eval(train("2", "232N", VehicleStatus::InTransitTo));
  CHECK(o.previous.source == PreviousSource::StaticTopology);
  CHECK(strcmp(o.previous.stopId, "233") == 0);
  CHECK(o.adapted);

  // Unknown destination (off the map) is an UnknownStop, never a silent skip.
  o = eval(train("A", "ZZZN", VehicleStatus::InTransitTo));
  CHECK(o.category == Category::UnknownStop && !o.adapted);
  CHECK(o.previous.failure == PreviousFailure::UnknownDestination);

  // H19 stays an operational stop (decided before any previous-stop lookup).
  o = eval(train("H", "H19N", VehicleStatus::InTransitTo));
  CHECK(o.category == Category::OperationalStop);

  // Unsupported route is reported before previous-stop resolution.
  o = eval(train("W", "R30N", VehicleStatus::InTransitTo));
  CHECK(o.category == Category::UnsupportedRoute);

  // Ambiguous keys: every candidate is run through the real adapter.
  RtTrain t = train("A", "A48N", VehicleStatus::InTransitTo);
  o = eval(t);
  CHECK(o.previous.failure == PreviousFailure::Ambiguous || o.previous.source == PreviousSource::StaticEquivalent);

  // Realtime stop list resolving the ambiguity (first update already in the past).
  t.hasTripUpdate = true;
  t.stopTimeUpdateCount = 3;
  snprintf(t.nextStopId, sizeof t.nextStopId, "A49N");
  snprintf(t.secondStopId, sizeof t.secondStopId, "A48N");
  t.nextStopDeparture = 990;
  t.tripUpdateTimestamp = 1000;
  o = eval(t);
  CHECK(o.previous.source == PreviousSource::Realtime && strcmp(o.previous.stopId, "A49") == 0);
  t.nextStopDeparture = 1500;  // future event: not a stop already left -> ignored
  o = eval(t);
  CHECK(o.previous.source != PreviousSource::Realtime);
  t.nextStopDeparture = 990;
  snprintf(t.secondStopId, sizeof t.secondStopId, "A47N");  // second stop is not the vehicle stop
  o = eval(t);
  CHECK(o.previous.source != PreviousSource::Realtime);
  snprintf(t.secondStopId, sizeof t.secondStopId, "A48N");
  t.stopTimeUpdateCount = 1;
  o = eval(t);
  CHECK(o.previous.source != PreviousSource::Realtime);

  // Data-driven sweep over every compiled (route, direction, destination).
  for (uint16_t i = 0; i < kTopologyEntryCount; ++i) {
    const TopologyEntry &e = kTopology[i];
    static const char d[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char stop[6] = {d[e.dest / 1296], d[(e.dest / 36) % 36], d[e.dest % 36], (e.routeDir & 1) ? 'S' : 'N', 0};
    const char *route = kTopologyRoutes[e.routeDir >> 1];
    const Observation ob = eval(train(route, stop, VehicleStatus::InTransitTo));
    const uint8_t n = e.flags & kTopologyCountMask;
    CHECK(ob.category != Category::Invalid);
    if (n == 0) {
      CHECK(ob.category == Category::Skipped && ob.reason == Reason::NoPreviousStop);
    } else if (n == 1) {
      CHECK(ob.previous.source == PreviousSource::StaticTopology);
      if (ob.category == Category::ResolvedSegment) ++uniqueSeg;
    } else if (ob.previous.source == PreviousSource::StaticEquivalent) {
      CHECK(ob.adapted);
      ++equivalent;
    } else {
      CHECK(ob.category == Category::Skipped && ob.reason == Reason::PreviousStopAmbiguous);
      CHECK(ob.previous.failure == PreviousFailure::Ambiguous && !ob.adapted);
      ++differing;
    }
    if (ob.category == Category::UnknownStop) ++unknownDest;
  }
}

// ---- 3. Exhaustive static comparison --------------------------------------

struct StaticStats {
  int patterns = 0, transitions = 0, exact = 0, ambiguous = 0, origins = 0;
  int offMap = 0, unresolved = 0, completenessGaps = 0;
};

bool isMapped(const char *parent) {
  char id[4] = {parent[0], parent[1], parent[2], 0};
  return sem::normalizeGtfsStop(id).status == sem::StopLookupStatus::Ok;
}

void testAgainstStaticPatterns(const hostidx::Index &idx, StaticStats &st) {
  for (int p = 0; p < idx.count; ++p) {
    const hostidx::Pattern &pat = idx.patterns[p];
    if (sem::routeFromGtfsRouteId(pat.route) == sem::Route::None) continue;
    ++st.patterns;
    const char *prev = nullptr;
    for (int i = 0; i < pat.count; ++i) {
      const char *stop = pat.stops[i];
      if (prev && strcmp(prev, stop) == 0) continue;  // collapsed duplicate parents
      char id[5] = {stop[0], stop[1], stop[2], pat.dir, 0};
      const PreviousStopResult r = previousStopFromTopology(pat.route, id, pat.dir);
      if (!isMapped(stop)) {
        CHECK(r.status == PrevStatus::UnknownStop);
        ++st.offMap;
      } else if (prev == nullptr) {
        CHECK(r.originPossible);
        ++st.origins;
      } else {
        ++st.transitions;
        if (r.status == PrevStatus::Match) {
          CHECK(strcmp(r.candidates[0], prev) == 0);
          ++st.exact;
        } else if (r.status == PrevStatus::Ambiguous) {
          CHECK(hasCandidate(r, prev));
          ++st.ambiguous;
        } else {
          CHECK(false);
          ++st.unresolved;
        }
      }
      prev = stop;
    }
  }
  // Completeness: every compiled predecessor really occurs in some static pattern.
  for (uint16_t i = 0; i < kTopologyEntryCount; ++i) {
    const TopologyEntry &e = kTopology[i];
    static const char d[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char stop[4] = {d[e.dest / 1296], d[(e.dest / 36) % 36], d[e.dest % 36], 0};
    const char *route = kTopologyRoutes[e.routeDir >> 1];
    const char dir = (e.routeDir & 1) ? 'S' : 'N';
    const PreviousStopResult r = previousStopFromTopology(route, stop, dir);
    for (uint8_t c = 0; c < r.count; ++c) {
      bool found = false;
      for (int p = 0; p < idx.count && !found; ++p) {
        const hostidx::Pattern &pat = idx.patterns[p];
        if (pat.dir != dir || strcmp(pat.route, route) != 0) continue;
        for (int k = 1; k < pat.count; ++k)
          if (strcmp(pat.stops[k], stop) == 0 && strcmp(pat.stops[k - 1], r.candidates[c]) == 0)
            found = true;
      }
      CHECK(found);
      if (!found) ++st.completenessGaps;
    }
  }
}

}  // namespace

int main(int argc, char **argv) {
  testApi();
  unsigned uniqueSeg = 0, equivalent = 0, differing = 0, unknownDest = 0;
  testBuilder(uniqueSeg, equivalent, differing, unknownDest);
  CHECK(uniqueSeg > 0);
  printf("builder sweep: %u entries; unique-predecessor ResolvedSegment %u; ambiguous but "
         "outcome-equivalent %u; ambiguous and skipped %u; off-map destination %u\n",
         static_cast<unsigned>(kTopologyEntryCount), uniqueSeg, equivalent, differing, unknownDest);

  StaticStats st;
  bool ranStatic = false;
  if (argc > 1) {
    hostidx::Index idx{};
    if (hostidx::load(idx, argv[1])) {
      testAgainstStaticPatterns(idx, st);
      ranStatic = true;
    }
  }
  if (ranStatic) {
    printf("static comparison: %d supported patterns, %d on-map transitions, %d exact (unique), "
           "%d ambiguous (contain the true predecessor), %d unresolved, %d pattern origins, "
           "%d off-map stops (UnknownStop), %d predecessors without static evidence\n",
           st.patterns, st.transitions, st.exact, st.ambiguous, st.unresolved, st.origins,
           st.offMap, st.completenessGaps);
  } else {
    printf("static comparison: SKIPPED (no pattern index; build it with "
           "tools/phase4e_build_pattern_index.py)\n");
  }
  printf("Phase 4F previous-stop host tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
