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
#include "rgb_renderer.h"
#include "train_frame.h"

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
  switch (s) {
    case PreviousSource::ExactTrip: return "exact";
    case PreviousSource::RouteFallback: return "fallback";
    case PreviousSource::Realtime: return "realtime";
    case PreviousSource::StaticTopology: return "topology";
    case PreviousSource::StaticEquivalent: return "topology-equiv";
    default: return "";
  }
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

// Phase 4F: host static pattern index vs firmware-capable topology table.
struct Compare {
  unsigned compared = 0, agreeExact = 0, agreeEquivalent = 0, agreeUnknownDest = 0;
  unsigned ambiguousContainsHost = 0, topologyOnly = 0, bothUnresolved = 0, topologyUnresolved = 0;
  unsigned conflicts = 0, topoSegments = 0, hostSegments = 0, realtimeExact = 0;
};

bool sameOutcome(const Observation &a, const Observation &b) {
  if (a.category != b.category) return false;
  if (a.adapted && b.adapted && a.result.resolved.status == sem::ResolveStatus::Match)
    return a.result.resolved.globalIndex == b.result.resolved.globalIndex;
  return true;
}

void compareTrain(const RtTrain &t, uint32_t feedTs, const BuilderConfig &hostCfg,
                  const BuilderConfig &topoCfg, Compare &c) {
  Observation h, o;
  evaluateTrain(t, feedTs, hostCfg, h);
  evaluateTrain(t, feedTs, topoCfg, o);
  const bool reachedProvider =
      t.hasVehicle && t.status != VehicleStatus::StoppedAt && t.status != VehicleStatus::Absent &&
      h.category != Category::UnsupportedRoute && h.category != Category::OperationalStop &&
      h.category != Category::Invalid && !(h.category == Category::Skipped && h.reason != Reason::NoPreviousStop);
  if (!reachedProvider) return;
  ++c.compared;
  if (h.category == Category::ResolvedSegment) ++c.hostSegments;
  if (o.category == Category::ResolvedSegment) ++c.topoSegments;
  const bool hostHas = h.previous.source != PreviousSource::None;
  const bool topoHas = o.previous.source != PreviousSource::None;
  if (o.previous.source == PreviousSource::Realtime) ++c.realtimeExact;
  if (hostHas && topoHas) {
    if (strcmp(h.previous.stopId, o.previous.stopId) == 0) {
      ++c.agreeExact;
    } else if (o.previous.source == PreviousSource::StaticEquivalent && sameOutcome(h, o)) {
      ++c.agreeEquivalent;
    } else {
      ++c.conflicts;
      printf("CONFLICT %s %s stop=%s host=%s topo=%s\n", t.routeId, t.tripId, t.vehicleStopId,
             h.previous.stopId, o.previous.stopId);
    }
  } else if (hostHas && o.category == Category::UnknownStop &&
             o.previous.failure == PreviousFailure::UnknownDestination) {
    if (h.category == Category::UnknownStop) {
      ++c.agreeUnknownDest;
    } else {
      ++c.conflicts;
      printf("CONFLICT %s %s stop=%s host resolved %s, topo UnknownDestination\n", t.routeId,
             t.tripId, t.vehicleStopId, categoryName(h.category));
    }
  } else if (hostHas && o.previous.failure == PreviousFailure::Ambiguous) {
    bool in = false;
    for (uint8_t i = 0; i < o.previous.candidateCount; ++i)
      if (strcmp(o.previous.candidates[i], h.previous.stopId) == 0) in = true;
    if (in) {
      ++c.ambiguousContainsHost;
    } else {
      ++c.conflicts;
      printf("CONFLICT %s %s stop=%s host=%s not in ambiguous candidates\n", t.routeId, t.tripId,
             t.vehicleStopId, h.previous.stopId);
    }
  } else if (hostHas) {
    ++c.topologyUnresolved;
  } else if (topoHas) {
    ++c.topologyOnly;
  } else {
    ++c.bothUnresolved;
  }
}

void printCompare(const Compare &c) {
  printf("\n== Phase 4F: host static pattern index vs firmware topology table ==\n");
  printf("in-transit trains compared:             %u\n", c.compared);
  printf("  agree, identical previous stop:       %u\n", c.agreeExact);
  printf("  agree, ambiguous but same outcome:    %u\n", c.agreeEquivalent);
  printf("  agree, destination off-map (Unknown): %u\n", c.agreeUnknownDest);
  printf("  firmware ambiguous (host exact in set): %u\n", c.ambiguousContainsHost);
  printf("  firmware unresolved, host resolved:   %u\n", c.topologyUnresolved);
  printf("  firmware resolved, host not:          %u\n", c.topologyOnly);
  printf("  both unresolved:                      %u\n", c.bothUnresolved);
  printf("  CONFLICTS:                            %u\n", c.conflicts);
  printf("  realtime-exact previous stops:        %u\n", c.realtimeExact);
  printf("ResolvedSegment: host index %u, firmware topology %u\n", c.hostSegments, c.topoSegments);
}

// ---- Phase 4G: logical frame diagnostics (host layer keeps trip ids) -------

struct TrainFrameHost {
  sem::TrainFrame frame;
  static constexpr int kIds = 8;
  char trip[sem::kFrameLedCount][kIds][kTripIdMax];
  char route[sem::kFrameLedCount][kIds][4];
  uint8_t stored[sem::kFrameLedCount];
  unsigned fed, added, notAdded;
  unsigned duplicateTrips;
  char seen[4000][kTripIdMax];
  unsigned seenCount;
};

void frameAdd(TrainFrameHost &h, const RtTrain &t, const Observation &o) {
  ++h.fed;
  bool dup = false;
  for (unsigned i = 0; i < h.seenCount; ++i)
    if (strcmp(h.seen[i], t.tripId) == 0) { dup = true; break; }
  if (dup) ++h.duplicateTrips;
  else if (h.seenCount < 4000) snprintf(h.seen[h.seenCount++], kTripIdMax, "%s", t.tripId);
  if (sem::addGtfsResult(h.frame, o.result) != sem::FrameAdd::Added) { ++h.notAdded; return; }
  ++h.added;
  const unsigned g = o.result.resolved.globalIndex;
  if (h.stored[g] < TrainFrameHost::kIds) {
    snprintf(h.trip[g][h.stored[g]], kTripIdMax, "%s", t.tripId);
    snprintf(h.route[g][h.stored[g]], 4, "%s", t.routeId);
    ++h.stored[g];
  }
}

void printFrame(const TrainFrameHost &h) {
  const sem::TrainFrame &f = h.frame;
  unsigned st = 0, sg = 0, statusOcc = 0, nonMap = 0;
  for (unsigned i = 0; i < sem::kFrameLedCount; ++i) {
    if (!f.occupancy[i]) continue;
    const sem::MapLed *m = sem::semanticForLed(i);
    if (!m) { ++statusOcc; continue; }
    (m->role == sem::Role::Station ? st : sg)++;
  }
  nonMap = statusOcc;
  printf("\n== Phase 4G logical frame ==\n");
  printf("resolved trains offered: %u   trains added to frame: %u   not added: %u\n", h.fed, h.added,
         h.notAdded);
  printf("duplicate trip_ids among offered: %u (upstream merge contract check)\n", h.duplicateTrips);
  printf("resolved trains in frame:   %u\n", f.resolvedTrains);
  printf("unique occupied LEDs:       %u (station %u, segment %u)\n", f.occupiedLeds, st, sg);
  printf("collision LEDs:             %u   excess trains: %u   max occupancy: %u\n", f.collisionLeds,
         f.excessTrains, f.maxOccupancy);
  printf("status LEDs occupied:       %u   non-map occupied: %u   rejected: %u\n", statusOcc, nonMap,
         f.rejectedTrains);
  for (unsigned i = 0; i < sem::kFrameLedCount; ++i) {
    if (f.occupancy[i] < 2) continue;
    const sem::MapLed *m = sem::semanticForLed(i);
    char where[80];
    if (m->role == sem::Role::Station)
      snprintf(where, sizeof where, "STATION %u %s", m->stationId, sem::stationName(m->stationId));
    else
      snprintf(where, sizeof where, "SEG %u-%u slot %u/%u", m->endpointA, m->endpointB, m->segmentSlot,
               m->segmentSlotCount);
    printf("  collision LED%u (global %u, ch%u/%u) route %s %s occupancy %u trips:", m->designator, i,
           m->channel, m->channelIndex, sem::routeName(m->route), where, f.occupancy[i]);
    for (unsigned k = 0; k < h.stored[i]; ++k) printf(" %s[%s]", h.trip[i][k], h.route[i][k]);
    printf("\n");
  }
}

// ---- Phase 4H: logical RGB frame diagnostics (in-memory only) --------------

void printRgb(const TrainFrameHost &h, uint8_t brightness) {
  static sem::RgbFrame rgb;
  sem::RenderConfig cfg;
  cfg.brightness = brightness;
  const sem::RenderResult r = sem::renderTrainFrame(h.frame, rgb, cfg);
  unsigned nonBlack = 0, statusNonBlack = 0;
  for (unsigned i = 0; i < sem::kFrameLedCount; ++i) {
    const bool lit = rgb.pixels[i].red || rgb.pixels[i].green || rgb.pixels[i].blue;
    if (lit) ++nonBlack;
    if (lit && !sem::semanticForLed(i)) ++statusNonBlack;
  }
  printf("\n== Phase 4H RGB frame (brightness %u/255, in-memory only) ==\n", brightness);
  printf("resolved trains in frame:      %u\n", h.frame.resolvedTrains);
  printf("occupied logical map LEDs:     %u\n", h.frame.occupiedLeds);
  printf("rendered LEDs:                 %u (station %u, segment %u)\n", r.renderedLeds, r.stationLeds,
         r.segmentLeds);
  printf("collision LEDs: %u   max occupancy: %u\n", h.frame.collisionLeds, h.frame.maxOccupancy);
  printf("non-black RGB pixels:          %u (expected == occupied when brightness > 0)\n", nonBlack);
  printf("status LEDs non-black:         %u\n", statusNonBlack);
  printf("renderer rejected entries:     %u (occupied status LEDs: %u)\n", r.rejectedLeds,
         r.occupiedStatusLeds);

  uint32_t seen[16];
  unsigned nseen = 0;
  bool collisionShown = false;
  for (unsigned i = 0; i < sem::kFrameLedCount; ++i) {
    const sem::MapLed *m = sem::semanticForLed(i);
    if (!m || !h.frame.occupancy[i]) continue;
    const sem::Rgb nom = sem::nominalRouteColor(m->route);
    const uint32_t key = (nom.red << 16) | (nom.green << 8) | nom.blue;
    bool fresh = true;
    for (unsigned k = 0; k < nseen; ++k) if (seen[k] == key) fresh = false;
    const bool showCollision = !collisionShown && h.frame.occupancy[i] > 1;
    if (!fresh && !showCollision) continue;
    if (fresh && nseen < 16) seen[nseen++] = key;
    if (showCollision) collisionShown = true;
    char where[80];
    if (m->role == sem::Role::Station)
      snprintf(where, sizeof where, "STATION %u %s", m->stationId, sem::stationName(m->stationId));
    else
      snprintf(where, sizeof where, "SEG %u-%u slot %u/%u", m->endpointA, m->endpointB, m->segmentSlot,
               m->segmentSlotCount);
    const sem::Rgb sc = rgb.pixels[i];
    printf("  LED%u route %s %s occ %u nominal #%02X%02X%02X scaled (%u,%u,%u)%s\n", m->designator,
           sem::routeName(m->route), where, h.frame.occupancy[i], nom.red, nom.green, nom.blue, sc.red,
           sc.green, sc.blue, h.frame.occupancy[i] > 1 ? " [collision]" : "");
  }
}
}  // namespace

int main(int argc, char **argv) {
  const char *patterns = nullptr;
  const char *routeFilter = nullptr;
  const char *catFilter = nullptr;
  bool verbose = false, all = false, quiet = false, useTopology = false, compare = false, frameMode = false, rgbMode = false;
  unsigned brightness = sem::kDefaultRenderBrightness;
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
    else if (!strcmp(a, "--topology")) useTopology = true;
    else if (!strcmp(a, "--compare")) compare = true;
    else if (!strcmp(a, "--frame")) frameMode = true;
    else if (!strcmp(a, "--rgb")) { rgbMode = true; frameMode = true; }
    else if (!strcmp(a, "--brightness") && i + 1 < argc) brightness = static_cast<unsigned>(atol(argv[++i]) & 255);
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
  cfg.previousStop = useTopology ? topologyPreviousStop
                                 : (patterns ? hostidx::previousStop : nullptr);
  cfg.previousStopContext = &provider;
  if (compare && !patterns) {
    fprintf(stderr, "--compare needs --patterns\n");
    return 2;
  }
  BuilderConfig hostCfg = cfg, topoCfg = cfg;
  hostCfg.previousStop = hostidx::previousStop;
  topoCfg.previousStop = topologyPreviousStop;
  Compare cmp{};

  static TrainFrameHost fh;
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
      if (compare) compareTrain(t, st.headerTimestamp, hostCfg, topoCfg, cmp);
      if (frameMode && (o.category == Category::ResolvedStation || o.category == Category::ResolvedSegment)) frameAdd(fh, t, o);
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
  if (patterns && !useTopology)
    printf("previous stop source:    exact=%d fallback=%d none=%d\n", provider.exact, provider.fallback,
           provider.none);
  if (compare) printCompare(cmp);
  if (frameMode) printFrame(fh);
  if (rgbMode) printRgb(fh, static_cast<uint8_t>(brightness));
  printf("resolved to LED:         %u trains on %u shared LEDs (max %u trains on one LED)\n", stationLeds,
         shared, maxShared);
  return decodeErrors ? 1 : 0;
}
