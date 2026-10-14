// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) validation of the Phase 4D GTFS adapter.
// Build/run with: python tools/run_gtfs_adapter_tests.py

#include "gtfs_adapter.h"

#include "generated/gtfs_stop_table.h"
#include "led_map.h"
#include "semantic_api.h"

#include <stdio.h>
#include <string.h>

using namespace brooklyn::semantic;
namespace ledmap = brooklyn::led::map;

// Array-literal helper: the host toolchain may lack <initializer_list>.
#define EACH(T, ...) (T[]){__VA_ARGS__}

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

const MapLed *byDesignator(uint16_t designator) {
  for (const MapLed &l : kMapLeds)
    if (l.designator == designator) return &l;
  return nullptr;
}

bool physicalOk(const ResolveResult &r) {
  if (r.status != ResolveStatus::Match || r.semantic == nullptr) return false;
  brooklyn::led::PhysicalAddress address{};
  if (!ledmap::toPhysicalAddress(brooklyn::led::GlobalPhysicalIndex{r.globalIndex},
                                 address))
    return false;
  return static_cast<uint8_t>(address.channel) == r.channel &&
         address.index == r.channelIndex &&
         r.globalIndex == r.semantic->globalOrder;
}

const char *routeIdOf(Route r) { return kRouteNames[static_cast<uint8_t>(r)]; }

constexpr uint8_t kMaxStops = 8;
struct StopList {
  char ids[kMaxStops][4];
  uint8_t count;
};

StopList stopsFor(uint16_t stationId) {
  StopList list{};
  for (const GtfsStop &s : kGtfsStops) {
    if (s.stationId == stationId && list.count < kMaxStops) {
      memcpy(list.ids[list.count++], s.stopId, 4);
    }
  }
  return list;
}

GtfsTrainState stopped(const char *route, const char *stop) {
  return GtfsTrainState{route, GtfsVehicleStatus::StoppedAt, stop, nullptr,
                        kProgressUnknown};
}

GtfsTrainState moving(const char *route, const char *from, const char *to,
                      uint16_t progress = kProgressUnknown,
                      GtfsVehicleStatus status = GtfsVehicleStatus::InTransitTo) {
  return GtfsTrainState{route, status, to, from, progress};
}

uint16_t stationNamed(const char *prefix) {
  for (const Station &s : kStations)
    if (strncmp(s.name, prefix, strlen(prefix)) == 0) return s.id;
  return 0;
}

// Progress value in the middle of the requested slot.
uint16_t progressForSlot(uint8_t slot, uint8_t count) {
  return static_cast<uint16_t>((2 * slot + 1) * (kProgressMax / 2) / count);
}

void testRoutes() {
  struct { const char *id; Route route; } supported[] = {
      {"2", Route::Route2}, {"3", Route::Route3}, {"4", Route::Route4},
      {"5", Route::Route5}, {"A", Route::RouteA}, {"B", Route::RouteB},
      {"C", Route::RouteC}, {"D", Route::RouteD}, {"F", Route::RouteF},
      {"G", Route::RouteG}, {"J", Route::RouteJ}, {"L", Route::RouteL},
      {"M", Route::RouteM}, {"N", Route::RouteN}, {"Q", Route::RouteQ},
      {"R", Route::RouteR}, {"S", Route::RouteS}, {"Z", Route::RouteZ},
      {"FS", Route::RouteS}, {"H", Route::RouteS},
  };
  bool seen[kRouteCount + 1] = {};
  for (const auto &s : supported) {
    CHECK(routeFromGtfsRouteId(s.id) == s.route);
    seen[static_cast<uint8_t>(s.route)] = true;
  }
  for (uint8_t i = 1; i <= kRouteCount; ++i) CHECK(seen[i]);
  // Every semantic route name round-trips as a GTFS id.
  for (uint8_t i = 1; i <= kRouteCount; ++i)
    CHECK(routeFromGtfsRouteId(kRouteNames[i]) == static_cast<Route>(i));

  const char *unsupported[] = {"GS", "W", "E", "1", "6", "6X", "7", "7X", "5X",
                               "FX", "SI", "SIR", "a", "b", "AA", "2 ", " 2",
                               "X", "?"};
  for (const char *id : unsupported) CHECK(routeFromGtfsRouteId(id) == Route::None);
  CHECK(routeFromGtfsRouteId("") == Route::None);
  CHECK(routeFromGtfsRouteId(nullptr) == Route::None);
}

void testStopTable() {
  CHECK(kGtfsStopCount == 197);
  for (uint16_t i = 0; i < kGtfsStopCount; ++i) {
    const GtfsStop &s = kGtfsStops[i];
    CHECK(strlen(s.stopId) == 3);
    CHECK(s.stopId[2] != 'N' && s.stopId[2] != 'S');
    CHECK(stationById(s.stationId) != nullptr);
    if (i > 0) CHECK(strcmp(kGtfsStops[i - 1].stopId, s.stopId) < 0);
  }

  // Every GTFS parent ID in the frozen registry maps to exactly one entity,
  // and that entity is the registry row that carries it.
  int registryIds = 0;
  for (const Station &st : kStations) {
    if (st.gtfsParentStopId == nullptr) continue;
    ++registryIds;
    int rows = 0;
    for (const GtfsStop &s : kGtfsStops)
      if (strcmp(s.stopId, st.gtfsParentStopId) == 0) {
        ++rows;
        CHECK(s.stationId == st.id);
      }
    CHECK(rows == 1);
    const StopLookup l = normalizeGtfsStop(st.gtfsParentStopId);
    CHECK(l.status == StopLookupStatus::Ok && l.stationId == st.id &&
          l.platform == PlatformDirection::None);
  }
  CHECK(registryIds == 43);

  // Coverage: all 188 registry stations have at least one GTFS parent.
  for (const Station &st : kStations) CHECK(stopsFor(st.id).count >= 1);

  // Transfer complexes: several parents, one entity.
  CHECK(stopsFor(2).count == 3);   // Atlantic Av-Barclays Ctr
  CHECK(stopsFor(83).count == 3);  // Broadway Junction
  CHECK(stopsFor(84).count == 2);  // Jay St-MetroTech
  CHECK(stopsFor(88).count == 2);  // Borough Hall
}

void testStopNormalization() {
  for (const GtfsStop &s : kGtfsStops) {
    char buf[5];
    memcpy(buf, s.stopId, 3);
    buf[3] = 'N';
    buf[4] = 0;
    StopLookup n = normalizeGtfsStop(buf);
    CHECK(n.status == StopLookupStatus::Ok && n.stationId == s.stationId &&
          n.platform == PlatformDirection::North);
    buf[3] = 'S';
    StopLookup so = normalizeGtfsStop(buf);
    CHECK(so.status == StopLookupStatus::Ok && so.stationId == s.stationId &&
          so.platform == PlatformDirection::South);
    buf[3] = 'X';
    CHECK(normalizeGtfsStop(buf).status == StopLookupStatus::Malformed);
  }

  // Well-formed but not in the table.
  for (const char *id : EACH(const char *, "ZZZ", "999", "A99", "ZZZN", "999S", "101", "101N",
                         "B99"))
    CHECK(normalizeGtfsStop(id).status == StopLookupStatus::Unknown);
  // Cross-system alias: semantic 170 "Lorimer St" is GTFS G29 "Metropolitan Av".
  for (const char *id : EACH(const char *, "G29", "G29N", "G29S")) {
    const StopLookup l = normalizeGtfsStop(id);
    CHECK(l.status == StopLookupStatus::Ok && l.stationId == 170);
  }
  CHECK(strcmp(stationName(170), "Lorimer St") == 0);
  // The frozen registry stays blank for 170; the alias lives only in the GTFS map.
  CHECK(stationById(170)->gtfsParentStopId == nullptr);

  // Malformed shapes.
  for (const char *id : EACH(const char *, "", "A", "A4", "A41NN", "A41NS", "a41", "A41n", "A 1",
                         "A-1", "A41X", "A41 ", " A41", "A41\n", "ÅA1", "A41NNN"))
    CHECK(normalizeGtfsStop(id).status == StopLookupStatus::Malformed);
  CHECK(normalizeGtfsStop(nullptr).status == StopLookupStatus::Malformed);

  // A trailing N/S is not stripped from a 3-char id: "N10" and "R31" are
  // parents whose last char is a digit, but "N" or "S" as a 4th char only.
  CHECK(normalizeGtfsStop("N10").stationId == normalizeGtfsStop("N10N").stationId);
  CHECK(normalizeGtfsStop("N10").status == StopLookupStatus::Ok);
  CHECK(normalizeGtfsStop("S01S").status == StopLookupStatus::Ok);
  CHECK(normalizeGtfsStop("S01S").platform == PlatformDirection::South);
}

void expectMatchLed(const GtfsMapResult &r, uint16_t designator) {
  const MapLed *led = byDesignator(designator);
  CHECK(led != nullptr);
  CHECK(r.status == GtfsMapStatus::Match);
  CHECK(r.resolved.status == ResolveStatus::Match);
  CHECK(r.resolved.semantic == led);
  CHECK(physicalOk(r.resolved));
}

void testStationSpecific() {
  // Route-specific shared stations: each route lights its own LED.
  CHECK(strcmp(stationName(83), "Broadway Junction") == 0);
  const GtfsMapResult a = mapGtfsTrain(stopped("A", "A51"));
  const GtfsMapResult c = mapGtfsTrain(stopped("C", "A51N"));
  const GtfsMapResult j = mapGtfsTrain(stopped("J", "J27S"));
  const GtfsMapResult z = mapGtfsTrain(stopped("Z", "J27"));
  const GtfsMapResult l = mapGtfsTrain(stopped("L", "L22"));
  for (const GtfsMapResult * r : EACH(const GtfsMapResult *, &a, &c, &j, &z, &l)) {
    CHECK(r->status == GtfsMapStatus::Match);
    CHECK(r->position.kind == PositionKind::Station);
    CHECK(r->position.stationId == 83);
    CHECK(physicalOk(r->resolved));
  }
  const uint16_t g[] = {a.resolved.globalIndex, c.resolved.globalIndex,
                        j.resolved.globalIndex, z.resolved.globalIndex,
                        l.resolved.globalIndex};
  for (int i = 0; i < 5; ++i)
    for (int k = i + 1; k < 5; ++k) CHECK(g[i] != g[k]);
  CHECK(a.resolved.semantic->route == Route::RouteA);
  CHECK(c.resolved.semantic->route == Route::RouteC);
  CHECK(l.resolved.semantic->route == Route::RouteL);

  // Atlantic Av-Barclays Ctr: three GTFS parents, eight route LEDs.
  CHECK(strncmp(stationName(2), "Atlantic Av", 11) == 0);
  struct { const char *route; const char *stop; } atlantic[] = {
      {"2", "235"}, {"4", "235S"}, {"5", "235N"}, {"B", "D24"},
      {"Q", "D24N"}, {"D", "R31"}, {"N", "R31S"}, {"R", "R31"},
  };
  uint16_t seen[8];
  int n = 0;
  for (const auto &t : atlantic) {
    const GtfsMapResult r = mapGtfsTrain(stopped(t.route, t.stop));
    CHECK(r.status == GtfsMapStatus::Match);
    CHECK(r.position.stationId == 2);
    CHECK(physicalOk(r.resolved));
    CHECK(r.resolved.semantic->route == routeFromGtfsRouteId(t.route));
    for (int i = 0; i < n; ++i) CHECK(seen[i] != r.resolved.globalIndex);
    seen[n++] = r.resolved.globalIndex;
  }

  // Jay St-MetroTech: A/F at A41, R at R29.
  CHECK(strncmp(stationName(84), "Jay St", 6) == 0);
  for (const char *s : EACH(const char *, "A41", "A41N", "A41S")) {
    CHECK(mapGtfsTrain(stopped("A", s)).status == GtfsMapStatus::Match);
    CHECK(mapGtfsTrain(stopped("F", s)).status == GtfsMapStatus::Match);
  }
  CHECK(mapGtfsTrain(stopped("R", "R29N")).status == GtfsMapStatus::Match);
  CHECK(mapGtfsTrain(stopped("R", "R29")).position.stationId == 84);

  // Coney Island-Stillwell Av.
  const uint16_t coney = stationNamed("Coney Island");
  CHECK(coney != 0);
  const StopList cs = stopsFor(coney);
  CHECK(cs.count >= 1);
  int matched = 0;
  for (const char *route : EACH(const char *, "D", "F", "N", "Q")) {
    const GtfsMapResult r = mapGtfsTrain(stopped(route, cs.ids[0]));
    if (r.status == GtfsMapStatus::Match) {
      ++matched;
      CHECK(r.position.stationId == coney);
      CHECK(physicalOk(r.resolved));
    }
  }
  CHECK(matched >= 1);

  // LED96 = G / Lorimer St / STATION, reached from G29 in every platform form.
  for (const char *id : EACH(const char *, "G29", "G29N", "G29S")) {
    const GtfsMapResult r = mapGtfsTrain(stopped("G", id));
    CHECK(r.status == GtfsMapStatus::Match);
    CHECK(r.position.stationId == 170);
    CHECK(r.resolved.semantic == byDesignator(96));
    CHECK(r.resolved.semantic->route == Route::RouteG);
    CHECK(r.resolved.semantic->role == Role::Station);
    CHECK(physicalOk(r.resolved));
    // No other route has a LED at this station through G29.
    for (const char *other : EACH(const char *, "L", "A", "F", "M", "J"))
      CHECK(mapGtfsTrain(stopped(other, id)).status == GtfsMapStatus::Unrepresented);
  }

  // Simple station.
  CHECK(strcmp(stationName(104), "Clark St") == 0);
  {
    const GtfsMapResult r = mapGtfsTrain(stopped("2", "231"));
    CHECK(r.status == GtfsMapStatus::Match);
    CHECK(r.position.stationId == 104);
    CHECK(r.resolved.semantic->route == Route::Route2);
  }
}

// Every GTFS-mapped registry station: every (LED, GTFS stop, platform form).
void testAllStations() {
  int ledsTested = 0, ledsUnmapped = 0, calls = 0;
  for (const MapLed &led : kMapLeds) {
    if (led.role != Role::Station) continue;
    const StopList stops = stopsFor(led.stationId);
    if (stops.count == 0) {
      ++ledsUnmapped;
      continue;
    }
    ++ledsTested;
    for (uint8_t i = 0; i < stops.count; ++i) {
      char platform[5];
      memcpy(platform, stops.ids[i], 3);
      platform[4] = 0;
      for (const char *form : EACH(const char *, "", "N", "S")) {
        platform[3] = form[0];
        const GtfsMapResult r = mapGtfsTrain(stopped(routeIdOf(led.route), platform));
        ++calls;
        CHECK(r.status == GtfsMapStatus::Match);
        CHECK(r.resolved.semantic == &led);
        CHECK(r.position.kind == PositionKind::Station);
        CHECK(physicalOk(r.resolved));
      }
    }
  }
  CHECK(ledsTested + ledsUnmapped == 276);
  CHECK(ledsUnmapped == 0);
  printf("  station LEDs via GTFS: %d tested, %d unmapped, %d lookups\n",
         ledsTested, ledsUnmapped, calls);
}

void testAllSegments() {
  int groups = 0, skipped = 0, multi = 0, lookups = 0;
  for (const MapLed &led : kMapLeds) {
    if (led.role != Role::Segment) continue;
    const StopList fromStops = stopsFor(led.endpointA);
    const StopList toStops = stopsFor(led.endpointB);
    if (fromStops.count == 0 || toStops.count == 0) {
      ++skipped;
      continue;
    }
    ++groups;
    const char *route = routeIdOf(led.route);
    const uint8_t n = led.segmentSlotCount;
    if (n > 1) ++multi;
    for (uint8_t i = 0; i < fromStops.count; ++i) {
      for (uint8_t k = 0; k < toStops.count; ++k) {
        const char *a = fromStops.ids[i];
        const char *b = toStops.ids[k];
        // Stored direction: stored slot s -> progress inside slot s.
        GtfsMapResult f = mapGtfsTrain(moving(route, a, b,
                                              progressForSlot(led.segmentSlot, n)));
        CHECK(f.status == GtfsMapStatus::Match);
        CHECK(f.resolved.semantic == &led);
        CHECK(physicalOk(f.resolved));
        // Reverse: the pixel is passed at slot n-1-s counted from B toward A.
        const uint8_t rev = static_cast<uint8_t>(n - 1 - led.segmentSlot);
        GtfsMapResult r = mapGtfsTrain(moving(route, b, a, progressForSlot(rev, n),
                                              GtfsVehicleStatus::IncomingAt));
        CHECK(r.status == GtfsMapStatus::Match);
        CHECK(r.resolved.semantic == &led);
        // Unknown progress: single-pixel resolves, multi-pixel is insufficient.
        GtfsMapResult u1 = mapGtfsTrain(moving(route, a, b));
        GtfsMapResult u2 = mapGtfsTrain(moving(route, b, a));
        if (n == 1) {
          CHECK(u1.status == GtfsMapStatus::Match && u1.resolved.semantic == &led);
          CHECK(u2.status == GtfsMapStatus::Match && u2.resolved.semantic == &led);
        } else {
          CHECK(u1.status == GtfsMapStatus::InsufficientPosition);
          CHECK(u2.status == GtfsMapStatus::InsufficientPosition);
          CHECK(u1.resolved.status == ResolveStatus::Ambiguous);
          CHECK(u1.resolved.semantic == nullptr);
        }
        lookups += 4;
      }
    }
  }
  CHECK(skipped == 0);
  CHECK(groups == 26);
  CHECK(multi == 4);
  printf("  segment LEDs via GTFS: %d tested, %d skipped, %d lookups\n",
         groups, skipped, lookups);
}

void testBridgeSegments() {
  // B: Grand St -> DeKalb Av, D: Grand St -> Atlantic Av-Barclays Ctr.
  const StopList grand = stopsFor(182);
  CHECK(grand.count >= 1);
  const char *gs = grand.ids[0];
  CHECK(strcmp(stationName(182), "Grand St") == 0);

  struct Case {
    const char *route;
    const char *to;
    uint16_t first, second;  // stored-slot 0 and 1 LEDs
  } cases[] = {{"B", "R30", 285, 309}, {"D", "R31", 286, 308}};
  for (const Case &c : cases) {
    struct { uint16_t p; uint16_t fwd; uint16_t rev; } t[] = {
        {0, c.first, c.second},   {1, c.first, c.second},
        {499, c.first, c.second}, {500, c.second, c.first},
        {501, c.second, c.first}, {999, c.second, c.first},
        {1000, c.second, c.first},
    };
    for (const auto &x : t) {
      expectMatchLed(mapGtfsTrain(moving(c.route, gs, c.to, x.p)), x.fwd);
      expectMatchLed(mapGtfsTrain(moving(c.route, c.to, gs, x.p)), x.rev);
    }
    // Platform suffixes do not change the answer.
    char to_n[5], gs_s[5];
    snprintf(to_n, sizeof to_n, "%sN", c.to);
    snprintf(gs_s, sizeof gs_s, "%sS", gs);
    expectMatchLed(mapGtfsTrain(moving(c.route, gs_s, to_n, 250)), c.first);
    expectMatchLed(mapGtfsTrain(moving(c.route, to_n, gs_s, 250)), c.second);

    for (const bool reverse : EACH(bool, false, true)) {
      const GtfsMapResult r = mapGtfsTrain(
          reverse ? moving(c.route, c.to, gs) : moving(c.route, gs, c.to));
      CHECK(r.status == GtfsMapStatus::InsufficientPosition);
      CHECK(r.position.kind == PositionKind::Segment);
      CHECK(r.position.segmentSlot == kAnySegmentSlot);
      CHECK(r.resolved.status == ResolveStatus::Ambiguous);
    }
    // StoppedAt ignores progress; out-of-range progress is invalid.
    CHECK(mapGtfsTrain(moving(c.route, gs, c.to, 1001)).status == GtfsMapStatus::Invalid);
    CHECK(mapGtfsTrain(moving(c.route, gs, c.to, 65534)).status == GtfsMapStatus::Invalid);
  }

  // slotForProgress boundaries for 1, 2, 3 pixels.
  CHECK(slotForProgress(0, 2) == 0 && slotForProgress(499, 2) == 0);
  CHECK(slotForProgress(500, 2) == 1 && slotForProgress(999, 2) == 1);
  CHECK(slotForProgress(1000, 2) == 1);
  CHECK(slotForProgress(0, 1) == 0 && slotForProgress(1000, 1) == 0);
  CHECK(slotForProgress(333, 3) == 0 && slotForProgress(334, 3) == 1);
  CHECK(slotForProgress(999, 3) == 2 && slotForProgress(1000, 3) == 2);
}

void testUnrepresented() {
  // Adjacent same-route station LEDs with no explicit segment between them.
  int pairs = 0;
  for (uint16_t i = 0; i + 1 < kMapLedCount; ++i) {
    const MapLed &a = kMapLeds[i];
    const MapLed &b = kMapLeds[i + 1];
    if (a.role != Role::Station || b.role != Role::Station) continue;
    if (a.route != b.route || a.channel != b.channel) continue;
    if (a.stationId == b.stationId) continue;
    const StopList sa = stopsFor(a.stationId);
    const StopList sb = stopsFor(b.stationId);
    if (sa.count == 0 || sb.count == 0) continue;
    if (segmentPixelCount(a.route, a.stationId, b.stationId) != 0) continue;
    for (const bool reverse : EACH(bool, false, true)) {
      const char *route = routeIdOf(a.route);
      const GtfsMapResult r = mapGtfsTrain(reverse ? moving(route, sb.ids[0], sa.ids[0])
                                                    : moving(route, sa.ids[0], sb.ids[0]));
      CHECK(r.status == GtfsMapStatus::Unrepresented);
      CHECK(r.position.kind == PositionKind::Segment);  // never a station fallback
      CHECK(r.resolved.status == ResolveStatus::NoMatch);
      CHECK(r.resolved.globalIndex == kNoGlobalIndex);
      CHECK(r.resolved.semantic == nullptr);
    }
    ++pairs;
  }
  CHECK(pairs > 50);
  printf("  unrepresented adjacent station pairs: %d\n", pairs);

  // Explicit: 2 Bergen St (236) -> Atlantic (235) has no segment LED.
  CHECK(mapGtfsTrain(moving("2", "236", "235")).status == GtfsMapStatus::Unrepresented);
  // Wrong route for the segment: B LEDs exist for Grand St -> DeKalb, A does not.
  CHECK(mapGtfsTrain(moving("A", "D22", "R30", 100)).status == GtfsMapStatus::Unrepresented);
  // Wrong route for a station: L has no LED at Jay St-MetroTech.
  const GtfsMapResult w = mapGtfsTrain(stopped("L", "A41"));
  CHECK(w.status == GtfsMapStatus::Unrepresented);
  CHECK(w.position.kind == PositionKind::Station && w.position.stationId == 84);
  CHECK(w.resolved.globalIndex == kNoGlobalIndex);
}

void testInvalid() {
  // Unsupported routes.
  for (const char *r : EACH(const char *, "GS", "W", "6", "7", "5X", "SI", "E", "b"))
    CHECK(mapGtfsTrain(stopped(r, "A41")).status == GtfsMapStatus::UnsupportedRoute);
  CHECK(mapGtfsTrain(moving("W", "A41", "A42")).status == GtfsMapStatus::UnsupportedRoute);

  // Missing required state.
  CHECK(mapGtfsTrain(stopped(nullptr, "A41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(stopped("", "A41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(stopped("A", nullptr)).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(stopped("A", "")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(moving("A", nullptr, "A41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(moving("A", "", "A41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(GtfsTrainState{"A", GtfsVehicleStatus::Unknown, "A41", "A42",
                                    kProgressUnknown}).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(GtfsTrainState{}).status == GtfsMapStatus::Invalid);

  // Unknown vs malformed stops.
  CHECK(mapGtfsTrain(stopped("A", "ZZZ")).status == GtfsMapStatus::UnknownStop);
  CHECK(mapGtfsTrain(stopped("A", "ZZZN")).status == GtfsMapStatus::UnknownStop);
  CHECK(mapGtfsTrain(moving("A", "ZZZ", "A41")).status == GtfsMapStatus::UnknownStop);
  CHECK(mapGtfsTrain(moving("A", "A41", "ZZZ")).status == GtfsMapStatus::UnknownStop);
  CHECK(mapGtfsTrain(stopped("A", "a41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(stopped("A", "A")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(stopped("A", "A41X")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(moving("A", "A4", "A41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(moving("A", "A41", "A 1")).status == GtfsMapStatus::Invalid);

  // Equal endpoints (same station, any platform forms) and parent-to-self.
  CHECK(mapGtfsTrain(moving("A", "A41", "A41")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(moving("A", "A41N", "A41S")).status == GtfsMapStatus::Invalid);
  CHECK(mapGtfsTrain(moving("2", "235", "235N")).status == GtfsMapStatus::Invalid);
  // Different GTFS parents of the same complex are also the same station.
  CHECK(mapGtfsTrain(moving("D", "R31", "D24")).status == GtfsMapStatus::Invalid);

  // Failure results carry no position.
  const GtfsMapResult bad = mapGtfsTrain(stopped("A", "ZZZ"));
  CHECK(bad.position.kind == PositionKind::Invalid);
  CHECK(bad.resolved.globalIndex == kNoGlobalIndex);

  // Progress is ignored when stopped.
  CHECK(mapGtfsTrain(GtfsTrainState{"A", GtfsVehicleStatus::StoppedAt, "A41", nullptr,
                                    5000}).status == GtfsMapStatus::Match);
}

}  // namespace

int main() {
  testRoutes();
  testStopTable();
  testStopNormalization();
  testStationSpecific();
  testAllStations();
  testAllSegments();
  testBridgeSegments();
  testUnrepresented();
  testInvalid();
  printf("%d checks, %d failures\n", checks, failures);
  printf(failures == 0 ? "GTFS ADAPTER TESTS PASSED\n" : "GTFS ADAPTER TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
