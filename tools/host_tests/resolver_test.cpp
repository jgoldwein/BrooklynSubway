// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware) validation of the Phase 4C train-position resolver.
// Build/run with: python tools/run_resolver_tests.py

#include "led_map.h"
#include "semantic_api.h"
#include "train_position_resolver.h"

#include <stdio.h>
#include <string.h>

using namespace brooklyn::semantic;
namespace ledmap = brooklyn::led::map;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                \
  do {                                                             \
    ++checks;                                                      \
    if (!(cond)) {                                                 \
      ++failures;                                                  \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                              \
  } while (0)

const MapLed *byDesignator(uint16_t designator) {
  for (uint16_t i = 0; i < kMapLedCount; ++i) {
    if (kMapLeds[i].designator == designator) return &kMapLeds[i];
  }
  return nullptr;
}

// Independent oracle: counts matching records without sharing resolver code.
int oracleStation(Route r, uint16_t s) {
  int n = 0;
  for (const MapLed &l : kMapLeds)
    if (l.route == r && l.role == Role::Station && l.stationId == s) ++n;
  return n;
}
int oracleSegment(Route r, uint16_t a, uint16_t b) {
  int n = 0;
  for (const MapLed &l : kMapLeds) {
    if (l.route != r || l.role != Role::Segment) continue;
    if ((l.endpointA == a && l.endpointB == b) ||
        (l.endpointA == b && l.endpointB == a))
      ++n;
  }
  return n;
}

// Slot reversal applied when a caller supplies endpoints against the stored
// direction: slot N becomes slotCount - 1 - N.
uint8_t reversedSlot(const MapLed &l) {
  return static_cast<uint8_t>(l.segmentSlotCount - 1 - l.segmentSlot);
}

bool physicalOk(const ResolveResult &r) {
  if (r.status != ResolveStatus::Match || r.semantic == nullptr) return false;
  brooklyn::led::PhysicalAddress address{};
  if (!ledmap::toPhysicalAddress(brooklyn::led::GlobalPhysicalIndex{r.globalIndex},
                                 address))
    return false;
  return static_cast<uint8_t>(address.channel) == r.channel &&
         address.index == r.channelIndex &&
         r.channel == r.semantic->channel &&
         r.channelIndex == r.semantic->channelIndex &&
         r.globalIndex == r.semantic->globalOrder;
}

Route routeOf(char c) {
  for (uint8_t i = 1; i <= kRouteCount; ++i)
    if (kRouteNames[i][0] == c) return static_cast<Route>(i);
  return Route::None;
}

void testAllRecordsRoundTrip() {
  int stations = 0, segments = 0, singlePixel = 0, multiPixel = 0;
  for (uint16_t i = 0; i < kMapLedCount; ++i) {
    const MapLed &led = kMapLeds[i];
    if (led.role == Role::Station) {
      ++stations;
      CHECK(led.segmentSlot == 0 && led.segmentSlotCount == 0);
      const ResolveResult r =
          resolveTrainPosition(stationPosition(led.route, led.stationId));
      CHECK(r.status == ResolveStatus::Match);
      CHECK(r.semantic == &led);
      CHECK(physicalOk(r));
      // A slot on a station query is ignored.
      TrainMapPosition withSlot = stationPosition(led.route, led.stationId);
      withSlot.segmentSlot = 7;
      CHECK(resolveTrainPosition(withSlot).semantic == &led);
    } else {
      ++segments;
      CHECK(led.segmentSlotCount >= 1 && led.segmentSlot < led.segmentSlotCount);
      const ResolveResult fwd = resolveTrainPosition(segmentPosition(
          led.route, led.endpointA, led.endpointB, led.segmentSlot));
      const ResolveResult rev = resolveTrainPosition(segmentPosition(
          led.route, led.endpointB, led.endpointA, reversedSlot(led)));
      CHECK(fwd.status == ResolveStatus::Match && fwd.semantic == &led);
      CHECK(rev.status == ResolveStatus::Match && rev.semantic == &led);
      CHECK(fwd.globalIndex == rev.globalIndex && fwd.channel == rev.channel &&
            fwd.channelIndex == rev.channelIndex);
      CHECK(physicalOk(fwd) && physicalOk(rev));

      // Unspecified slot: single-pixel -> Match, multi-pixel -> Ambiguous.
      const ResolveResult any1 = resolveTrainPosition(
          segmentPosition(led.route, led.endpointA, led.endpointB));
      const ResolveResult any2 = resolveTrainPosition(
          segmentPosition(led.route, led.endpointB, led.endpointA, kAnySegmentSlot));
      if (led.segmentSlotCount == 1) {
        ++singlePixel;
        CHECK(any1.status == ResolveStatus::Match && any1.semantic == &led);
        CHECK(any2.status == ResolveStatus::Match && any2.semantic == &led);
      } else {
        ++multiPixel;
        CHECK(any1.status == ResolveStatus::Ambiguous && any1.semantic == nullptr);
        CHECK(any2.status == ResolveStatus::Ambiguous && any2.semantic == nullptr);
      }
      // One past the last slot is out of range.
      CHECK(resolveTrainPosition(segmentPosition(led.route, led.endpointA, led.endpointB,
                                                 led.segmentSlotCount))
                .status == ResolveStatus::Invalid);
    }
  }
  CHECK(stations == 276);
  CHECK(segments == 26);
  CHECK(stations + segments == 302);
  CHECK(singlePixel == 22 && multiPixel == 4);
  printf("round trip: %d STATION + %d SEGMENT = %d records, all unique "
         "(segments forward and reversed with slot mapping); any-slot: %d single-pixel "
         "Match, %d LEDs in 2 multi-pixel groups Ambiguous\n",
         stations, segments, stations + segments, singlePixel, multiPixel);
}

void testSlotGroups() {
  const Route B = routeOf('B');
  const Route D = routeOf('D');
  const MapLed *l285 = byDesignator(285), *l309 = byDesignator(309);
  const MapLed *l286 = byDesignator(286), *l308 = byDesignator(308);
  CHECK(l285 && l309 && l286 && l308);
  CHECK(l285->endpointA == l309->endpointA && l285->endpointB == l309->endpointB);
  CHECK(l286->endpointA == l308->endpointA && l286->endpointB == l308->endpointB);
  CHECK(l285->route == B && l309->route == B && l286->route == D && l308->route == D);
  CHECK(!strcmp(stationName(l285->endpointA), "Grand St") &&
        !strcmp(stationName(l285->endpointB), "DeKalb Av"));
  CHECK(!strcmp(stationName(l286->endpointA), "Grand St") &&
        !strcmp(stationName(l286->endpointB), "Atlantic Av-Barclays Ctr"));

  struct Case {
    const MapLed *group;  // supplies route + stored endpoints
    bool reversed;
    uint8_t slot;
    const MapLed *expect;
  };
  const Case cases[] = {
      {l285, false, 0, l285}, {l285, false, 1, l309},
      {l285, true, 0, l309},  {l285, true, 1, l285},
      {l286, false, 0, l286}, {l286, false, 1, l308},
      {l286, true, 0, l308},  {l286, true, 1, l286},
  };
  for (const Case &c : cases) {
    const uint16_t a = c.reversed ? c.group->endpointB : c.group->endpointA;
    const uint16_t b = c.reversed ? c.group->endpointA : c.group->endpointB;
    const ResolveResult r =
        resolveTrainPosition(segmentPosition(c.group->route, a, b, c.slot));
    CHECK(r.status == ResolveStatus::Match && r.semantic == c.expect);
    CHECK(physicalOk(r));
    // Out-of-range slot on these two-pixel groups.
    CHECK(resolveTrainPosition(segmentPosition(c.group->route, a, b, 2)).status ==
          ResolveStatus::Invalid);
  }
  // Wrong route never borrows another route's slot.
  CHECK(resolveTrainPosition(segmentPosition(D, l285->endpointA, l285->endpointB, 0)).status ==
        ResolveStatus::NoMatch);
  printf("explicit slots: B 285/309 and D 286/308 forward + reversed verified\n");
}
void testKnownSegments() {
  struct Case {
    uint16_t des;
    char route;
    const char *a;
    const char *b;
  };
  const Case cases[] = {
      {187, 'G', "Greenpoint Av", "Nassau Av"},
      {288, 'C', "Lafayette Av", "Clinton-Washington Avs"},
      {287, 'A', "Hoyt-Schermerhorn Sts", "Nostrand Av"},
      {285, 'B', "Grand St", "DeKalb Av"},
      {286, 'D', "Grand St", "Atlantic Av-Barclays Ctr"},
      {291, 'F', "York St", "East Broadway"},
      {292, 'N', "Atlantic Av-Barclays Ctr", "36 St"},
      {295, 'G', "Lorimer St", "Broadway"},
      {297, 'M', "Hewes St", "Lorimer St"},
      {299, 'L', "Broadway Junction", "Atlantic Av"},
      {300, '2', "Franklin Av-Medgar Evers College",
       "President St-Medgar Evers College"},
      {301, '3', "Franklin Av-Medgar Evers College", "Nostrand Av"},
      {303, '5', "Atlantic Av-Barclays Ctr", "Franklin Av-Medgar Evers College"},
      {304, 'S', "Botanic Garden", "Park Pl"},
      {305, 'S', "Broad Channel", "Beach 90 St"},
  };
  for (const Case &c : cases) {
    const MapLed *led = byDesignator(c.des);
    CHECK(led != nullptr && led->role == Role::Segment && led->stationId == 0);
    if (led == nullptr || led->role != Role::Segment) continue;
    CHECK(led->route == routeOf(c.route));
    const char *a = stationName(led->endpointA);
    const char *b = stationName(led->endpointB);
    const bool namesOk =
        a && b &&
        ((!strcmp(a, c.a) && !strcmp(b, c.b)) ||
         (!strcmp(a, c.b) && !strcmp(b, c.a)));
    CHECK(namesOk);
    if (!namesOk) printf("  LED%u endpoints: %s / %s\n", c.des, a ? a : "?", b ? b : "?");
    const ResolveResult r = resolveTrainPosition(segmentPosition(
        routeOf(c.route), led->endpointA, led->endpointB, led->segmentSlot));
    CHECK(r.status == ResolveStatus::Match && r.semantic == led);
  }
  // Manhattan Bridge LEDs resolve from generated endpoints, no special cases.
  const uint16_t bridge[] = {308, 309, 310, 311};
  for (uint16_t des : bridge) {
    const MapLed *led = byDesignator(des);
    CHECK(led != nullptr && led->role == Role::Segment);
    if (led == nullptr) continue;
    const ResolveResult r = resolveTrainPosition(
        segmentPosition(led->route, led->endpointB, led->endpointA, reversedSlot(*led)));
    CHECK(r.status == ResolveStatus::Match && r.semantic == led);
  }
  const MapLed *l96 = byDesignator(96);
  CHECK(l96 && l96->role == Role::Station &&
        !strcmp(stationName(l96->stationId), "Lorimer St") &&
        l96->route == routeOf('G'));
}

void testSharedTransfers() {
  int multiRouteStations = 0;
  bool sawBroadwayJunction = false;
  for (uint16_t sid = 1; sid <= kRegistryCount; ++sid) {
    uint16_t count = 0;
    const uint16_t *idx = stationLedIndices(sid, count);
    if (count < 2) continue;
    ++multiRouteStations;
    if (!strcmp(stationName(sid), "Broadway Junction")) sawBroadwayJunction = true;
    for (uint16_t k = 0; k < count; ++k) {
      const MapLed &led = kMapLeds[idx[k]];
      const ResolveResult r = resolveTrainPosition(stationPosition(led.route, sid));
      CHECK(r.status == ResolveStatus::Match && r.semantic == &led);
      for (uint16_t j = 0; j < count; ++j) {
        if (j != k) CHECK(r.semantic != &kMapLeds[idx[j]]);
      }
    }
  }
  CHECK(multiRouteStations > 0);
  CHECK(sawBroadwayJunction);
  printf("shared transfer stations (>=2 route LEDs): %d, Broadway Junction included: %s\n",
         multiRouteStations, sawBroadwayJunction ? "yes" : "no");
}

void testNegativeAndInvalid() {
  // Endpoint-only stations: valid IDs with no LED on any route.
  int endpointOnly = 0;
  for (uint16_t sid = 1; sid <= kRegistryCount; ++sid) {
    uint16_t count = 0;
    stationLedIndices(sid, count);
    if (count != 0) continue;
    ++endpointOnly;
    for (uint8_t r = 1; r <= kRouteCount; ++r)
      CHECK(resolveTrainPosition(stationPosition(static_cast<Route>(r), sid)).status ==
            ResolveStatus::NoMatch);
  }
  CHECK(endpointOnly == 3);

  // Wrong route / no LED on that route, compared with the independent oracle.
  int wrongRoute = 0, noSegment = 0;
  for (uint8_t r = 1; r <= kRouteCount; ++r) {
    for (uint16_t s = 1; s <= kRegistryCount; ++s) {
      const ResolveResult res =
          resolveTrainPosition(stationPosition(static_cast<Route>(r), s));
      const int expected = oracleStation(static_cast<Route>(r), s);
      CHECK(expected <= 1);
      CHECK(res.status ==
            (expected == 1 ? ResolveStatus::Match : ResolveStatus::NoMatch));
      if (expected == 0) ++wrongRoute;
    }
  }
  CHECK(wrongRoute > 0);

  // Every (route, unordered pair) over the full registry agrees with the oracle;
  // station pairs without a segment LED must be NoMatch (no fallback).
  for (uint8_t r = 1; r <= kRouteCount; ++r) {
    for (uint16_t a = 1; a <= kRegistryCount; ++a) {
      for (uint16_t b = static_cast<uint16_t>(a + 1); b <= kRegistryCount; ++b) {
        const ResolveResult x =
            resolveTrainPosition(segmentPosition(static_cast<Route>(r), a, b));
        const ResolveResult y =
            resolveTrainPosition(segmentPosition(static_cast<Route>(r), b, a));
        const int expected = oracleSegment(static_cast<Route>(r), a, b);
        CHECK(expected <= 2);
        CHECK(x.status == y.status && x.semantic == y.semantic);
        CHECK(x.status == (expected == 0   ? ResolveStatus::NoMatch
                           : expected == 1 ? ResolveStatus::Match
                                           : ResolveStatus::Ambiguous));
        if (expected == 0) ++noSegment;
      }
    }
  }
  CHECK(noSegment > 0);

  // Adjacent station LEDs on a route with no segment LED between them.
  int adjacentNoSegment = 0;
  for (uint16_t i = 0; i + 1 < kMapLedCount; ++i) {
    const MapLed &p = kMapLeds[i];
    const MapLed &q = kMapLeds[i + 1];
    if (p.role == Role::Station && q.role == Role::Station && p.route == q.route &&
        p.stationId != q.stationId &&
        oracleSegment(p.route, p.stationId, q.stationId) == 0) {
      CHECK(resolveTrainPosition(segmentPosition(p.route, p.stationId, q.stationId))
                .status == ResolveStatus::NoMatch);
      ++adjacentNoSegment;
    }
  }
  CHECK(adjacentNoSegment > 0);

  // Wrong route for a real segment.
  const MapLed *l299 = byDesignator(299);
  CHECK(resolveTrainPosition(
            segmentPosition(routeOf('A'), l299->endpointA, l299->endpointB))
            .status == ResolveStatus::NoMatch);

  // Invalid inputs.
  const Route A = routeOf('A');
  CHECK(resolveTrainPosition(stationPosition(Route::None, 1)).status == ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(stationPosition(static_cast<Route>(kRouteCount + 1), 1)).status ==
        ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(stationPosition(A, 0)).status == ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(stationPosition(A, kRegistryCount + 1)).status ==
        ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(segmentPosition(Route::None, 1, 2)).status == ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(segmentPosition(A, 0, 2)).status == ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(segmentPosition(A, 1, kRegistryCount + 1)).status ==
        ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(segmentPosition(A, 5, 5)).status == ResolveStatus::Invalid);
  CHECK(resolveTrainPosition(TrainMapPosition{A, PositionKind::Invalid, 1, 1, 2, 0}).status ==
        ResolveStatus::Invalid);
  CHECK(resolveTrainPositionIn(nullptr, 0, stationPosition(A, 1)).status ==
        ResolveStatus::Invalid);

  const ResolveResult bad = resolveTrainPosition(stationPosition(Route::None, 1));
  CHECK(bad.globalIndex == kNoGlobalIndex && bad.semantic == nullptr &&
        bad.channel == 0 && bad.channelIndex == 0);
  printf("negative: %d endpoint-only ids, %d route/station NoMatch, %d route/pair NoMatch, "
         "%d adjacent-station pairs without segment LED\n",
         endpointOnly, wrongRoute, noSegment, adjacentNoSegment);
}

void testAmbiguity() {
  const Route G = routeOf('G');
  const MapLed station = {1, 1, 10, 0, 0, 1, 0, G, Role::Station, 0, 0};
  const MapLed segment = {2, 2, 0, 10, 11, 1, 1, G, Role::Segment, 0, 1};
  MapLed dupStation[2] = {station, station};
  dupStation[1].designator = 3;
  dupStation[1].globalOrder = 3;
  const ResolveResult amb = resolveTrainPositionIn(dupStation, 2, stationPosition(G, 10));
  CHECK(amb.status == ResolveStatus::Ambiguous);
  CHECK(amb.semantic == nullptr && amb.globalIndex == kNoGlobalIndex);

  MapLed dupSegment[2] = {segment, segment};
  dupSegment[1].endpointA = 11;  // reversed endpoint order still collides
  dupSegment[1].endpointB = 10;
  dupSegment[1].globalOrder = 4;
  // Same stored slot in both records collides even when a slot is requested.
  CHECK(resolveTrainPositionIn(dupSegment, 2, segmentPosition(G, 10, 11, 0)).status ==
        ResolveStatus::Ambiguous);
  // Distinct slots in a (synthetic) two-pixel group resolve uniquely.
  MapLed pair[2] = {segment, segment};
  pair[0].segmentSlotCount = pair[1].segmentSlotCount = 2;
  pair[1].segmentSlot = 1;
  pair[1].globalOrder = 5;
  CHECK(resolveTrainPositionIn(pair, 2, segmentPosition(G, 10, 11, 1)).semantic == &pair[1]);
  CHECK(resolveTrainPositionIn(pair, 2, segmentPosition(G, 11, 10, 1)).semantic == &pair[0]);
  CHECK(resolveTrainPositionIn(pair, 2, segmentPosition(G, 10, 11)).status ==
        ResolveStatus::Ambiguous);
  CHECK(resolveTrainPositionIn(dupSegment, 2, segmentPosition(G, 10, 11)).status ==
        ResolveStatus::Ambiguous);

  MapLed unique[2] = {station, segment};
  CHECK(resolveTrainPositionIn(unique, 2, stationPosition(G, 10)).status == ResolveStatus::Match);
  CHECK(resolveTrainPositionIn(unique, 2, segmentPosition(G, 11, 10)).status ==
        ResolveStatus::Match);
  // A station query must not match a segment record that mentions the station.
  CHECK(resolveTrainPositionIn(&dupSegment[0], 1, stationPosition(G, 10)).status ==
        ResolveStatus::NoMatch);
}

void testDatasetUnique() {
  // The shipped dataset must be unambiguous for every one of its own records.
  for (const MapLed &led : kMapLeds) {
    if (led.role == Role::Station)
      CHECK(oracleStation(led.route, led.stationId) == 1);
    else
      CHECK(oracleSegment(led.route, led.endpointA, led.endpointB) ==
            led.segmentSlotCount);
  }
  CHECK(selfCheck());
}

}  // namespace

int main() {
  testDatasetUnique();
  testAllRecordsRoundTrip();
  testSlotGroups();
  testKnownSegments();
  testSharedTransfers();
  testNegativeAndInvalid();
  testAmbiguity();
  printf("%d checks, %d failures\n", checks, failures);
  printf(failures == 0 ? "RESOLVER TESTS PASSED\n" : "RESOLVER TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
