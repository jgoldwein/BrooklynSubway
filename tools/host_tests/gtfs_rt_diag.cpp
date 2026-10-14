// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-only Phase 4E diagnostic: decode GTFS-RT snapshots, normalize each
// train, run it through the Phase 4D adapter / Phase 4C resolver and print a
// concise table plus a summary. Never touches hardware or LED output.
// Run via: python tools/phase4e_diag.py [options] feed*.pb

#include "pattern_index.h"

#include "gtfs_rt_state.h"
#include "semantic_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace brooklyn::gtfsrt;
namespace sem = brooklyn::semantic;

namespace {

constexpr size_t kMaxTrips = 4000;

uint8_t *readFile(const char *path, size_t &n) {
  FILE *f = fopen(path, "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *buf = static_cast<uint8_t *>(malloc(size > 0 ? size : 1));
  n = fread(buf, 1, size, f);
  fclose(f);
  return buf;
}

const char *gtfsStateName(sem::GtfsVehicleStatus s) {
  switch (s) {
    case sem::GtfsVehicleStatus::StoppedAt: return "STOPPED_AT";
    case sem::GtfsVehicleStatus::IncomingAt: return "INCOMING_AT";
    case sem::GtfsVehicleStatus::InTransitTo: return "IN_TRANSIT_TO";
    default: return "-";
  }
}

const char *srcName(PreviousSource s) {
  return s == PreviousSource::ExactTrip ? "exact" : s == PreviousSource::RouteFallback ? "fallback" : "";
}

const char *adapterName(const Observation &o) {
  if (!o.adapted) return "-";
  switch (o.result.status) {
    case sem::GtfsMapStatus::Match: return "Match";
    case sem::GtfsMapStatus::Unrepresented: return "Unrepresented";
    case sem::GtfsMapStatus::InsufficientPosition: return "InsufficientPosition";
    case sem::GtfsMapStatus::UnsupportedRoute: return "UnsupportedRoute";
    case sem::GtfsMapStatus::UnknownStop: return "UnknownStop";
    case sem::GtfsMapStatus::Invalid: return "Invalid";
  }
  return "?";
}

constexpr int kCategories = 9;

}  // namespace

int main(int argc, char **argv) {
  const char *patterns = nullptr;
  const char *routeFilter = nullptr;
  const char *catFilter = nullptr;
  bool verbose = false, all = false, quiet = false;
  uint32_t maxAge = kDefaultMaxVehicleAgeSec, now = 0;
  long limit = -1;
  const char *files[32];
  int nfiles = 0;

  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if (!strcmp(a, "--patterns") && i + 1 < argc) patterns = argv[++i];
    else if (!strcmp(a, "--route") && i + 1 < argc) routeFilter = argv[++i];
    else if (!strcmp(a, "--category") && i + 1 < argc) catFilter = argv[++i];
    else if (!strcmp(a, "--max-age") && i + 1 < argc) maxAge = static_cast<uint32_t>(atol(argv[++i]));
    else if (!strcmp(a, "--now") && i + 1 < argc) now = static_cast<uint32_t>(atol(argv[++i]));
    else if (!strcmp(a, "--limit") && i + 1 < argc) limit = atol(argv[++i]);
    else if (!strcmp(a, "--verbose")) verbose = true;
    else if (!strcmp(a, "--all")) all = true;
    else if (!strcmp(a, "--summary-only")) quiet = true;
    else if (nfiles < 32) files[nfiles++] = a;
  }
  if (nfiles == 0) {
    fprintf(stderr, "usage: diag [--patterns tsv] [--route R] [--category C] [--max-age s] [--now t]\n"
                    "            [--limit n] [--all] [--verbose] [--summary-only] feed.pb...\n");
    return 2;
  }

  hostidx::Provider provider{};
  if (patterns && !hostidx::load(provider.index, patterns)) {
    fprintf(stderr, "cannot read pattern index %s\n", patterns);
    return 2;
  }
  BuilderConfig cfg{};
  cfg.now = now;
  cfg.maxVehicleAgeSec = maxAge;
  cfg.previousStop = patterns ? hostidx::previousStop : nullptr;
  cfg.previousStopContext = &provider;

  static RtTrain trains[kMaxTrips];
  unsigned counts[kCategories] = {};
  unsigned reasons[16] = {};
  unsigned long entities = 0, tripUpdates = 0, vehicles = 0, malformed = 0, deleted = 0;
  unsigned total = 0, supported = 0, decodeErrors = 0, noVehicle = 0;
  unsigned stationLeds = 0;
  static unsigned ledHits[1024];
  if (!quiet)
    printf("%-3s %-17s %s %-5s %-5s %-13s %-20s %-34s %-6s %-6s %s\n", "rt", "trip_id", "d", "prev",
           "stop", "gtfs_state", "adapter", "semantic", "led", "ch/idx", "status/reason");

  long printedRows = 0;
  for (int fi = 0; fi < nfiles; ++fi) {
    size_t len = 0;
    uint8_t *buf = readFile(files[fi], len);
    if (!buf) {
      fprintf(stderr, "cannot read %s\n", files[fi]);
      ++decodeErrors;
      continue;
    }
    size_t count = 0;
    FeedStats st;
    DecodeStatus ds = decodeFeed(buf, len, trains, kMaxTrips, count, st);
    if (ds != DecodeStatus::Ok) {
      ++decodeErrors;
      fprintf(stderr, "decode %s: status %d (kept %zu trips)\n", files[fi], static_cast<int>(ds), count);
    }
    entities += st.entities; tripUpdates += st.tripUpdates; vehicles += st.vehicles;
    malformed += st.malformedEntities; deleted += st.deleted;

    for (size_t i = 0; i < count; ++i) {
      const RtTrain &t = trains[i];
      Observation o;
      evaluateTrain(t, st.headerTimestamp, cfg, o);
      ++total;
      ++counts[static_cast<int>(o.category)];
      if (o.category == Category::Skipped || o.category == Category::Invalid) ++reasons[static_cast<int>(o.reason)];
      if (o.category != Category::UnsupportedRoute) ++supported;
      if (!t.hasVehicle) ++noVehicle;
      if (o.adapted && o.result.resolved.status == sem::ResolveStatus::Match && o.result.resolved.semantic) {
        ++stationLeds;
        const unsigned d = o.result.resolved.semantic->designator;
        if (d < 1024) ++ledHits[d];
      }

      if (quiet) continue;
      if (!all && o.category == Category::UnsupportedRoute) continue;
      if (routeFilter && strcmp(routeFilter, t.routeId) != 0) continue;
      if (catFilter && strcmp(catFilter, categoryName(o.category)) != 0) continue;
      if (limit >= 0 && printedRows >= limit) continue;
      ++printedRows;

      char semText[96] = "-", led[8] = "-", chIdx[12] = "-", status[64];
      if (o.adapted) {
        const sem::TrainMapPosition &p = o.result.position;
        if (p.kind == sem::PositionKind::Station) {
          snprintf(semText, sizeof semText, "STATION %u %s", p.stationId, sem::stationName(p.stationId));
        } else if (p.kind == sem::PositionKind::Segment) {
          snprintf(semText, sizeof semText, "SEG %u-%u px=%u", p.endpointA, p.endpointB,
                   sem::segmentPixelCount(p.route, p.endpointA, p.endpointB));
        }
        const sem::ResolveResult &r = o.result.resolved;
        if (r.status == sem::ResolveStatus::Match && r.semantic) {
          snprintf(led, sizeof led, "LED%u", r.semantic->designator);
          snprintf(chIdx, sizeof chIdx, "%u/%u", r.channel, r.channelIndex);
        }
      }
      snprintf(status, sizeof status, "%s%s%s", categoryName(o.category),
               o.reason != Reason::None ? ":" : "", reasonName(o.reason));
      const char dir[2] = {travelDirection(t), '\0'};
      printf("%-3s %-17s %s %-5s %-5s %-13s %-20s %-34s %-6s %-6s %s%s%s\n", t.routeId, t.tripId, dir,
             o.previous.stopId, t.vehicleStopId[0] ? t.vehicleStopId : t.nextStopId,
             gtfsStateName(o.gtfsStatus), adapterName(o), semText, led, chIdx, status,
             o.previous.source != PreviousSource::None ? " prev=" : "", srcName(o.previous.source));
      if (verbose)
        printf("      vehicle=%s train=%s vts=%u tuts=%u next=%s arr=%u dep=%u stus=%u seq=%u%s sched=%u\n",
               t.vehicleId, t.trainId, t.vehicleTimestamp, t.tripUpdateTimestamp, t.nextStopId,
               t.nextStopArrival, t.nextStopDeparture, t.stopTimeUpdateCount, t.currentStopSequence,
               t.hasCurrentStopSequence ? "" : "(none)", t.scheduleRelationship);
    }
    free(buf);
  }

  unsigned shared = 0, maxShared = 0;
  for (unsigned d = 0; d < 1024; ++d) {
    if (ledHits[d] > 1) ++shared;
    if (ledHits[d] > maxShared) maxShared = ledHits[d];
  }

  printf("\n== summary (%d feed file(s)) ==\n", nfiles);
  printf("feed entities decoded:   %lu (trip_update %lu, vehicle %lu, deleted %lu)\n", entities,
         tripUpdates, vehicles, deleted);
  printf("malformed entities:      %lu   decode errors: %u\n", malformed, decodeErrors);
  printf("trips (merged by id):    %u   supported-route trips: %u   trips without vehicle: %u\n", total,
         supported, noVehicle);
  for (int c = 0; c < kCategories; ++c)
    printf("  %-22s %u\n", categoryName(static_cast<Category>(c)), counts[c]);
  printf("skip/invalid reasons:");
  for (int r = 1; r < 16; ++r)
    if (reasons[r]) printf(" %s=%u", reasonName(static_cast<Reason>(r)), reasons[r]);
  printf("\n");
  if (patterns)
    printf("previous stop source:    exact=%d fallback=%d none=%d\n", provider.exact, provider.fallback,
           provider.none);
  printf("resolved to LED:         %u trains on %u shared LEDs (max %u trains on one LED)\n", stationLeds,
         shared, maxShared);
  return decodeErrors ? 1 : 0;
}
