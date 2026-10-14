// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) validation of the Phase 4G logical train
// frame. Build/run with: python tools/run_train_frame_tests.py

#include "gtfs_rt_state.h"
#include "led_map.h"
#include "semantic_api.h"
#include "train_frame.h"
#include "train_position_resolver.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

using namespace brooklyn::semantic;
namespace led = brooklyn::led;
using brooklyn::gtfsrt::BuilderConfig;
using brooklyn::gtfsrt::Category;
using brooklyn::gtfsrt::Observation;
using brooklyn::gtfsrt::Reason;
using brooklyn::gtfsrt::RtTrain;
using brooklyn::gtfsrt::VehicleStatus;

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

ResolveResult matchFor(const MapLed &m) {
  return ResolveResult{ResolveStatus::Match, m.globalOrder, m.channel, m.channelIndex, &m};
}

GtfsMapResult gtfsMatch(const MapLed &m) {
  GtfsMapResult g{};
  g.status = GtfsMapStatus::Match;
  g.resolved = matchFor(m);
  return g;
}

GtfsMapResult gtfsStatus(GtfsMapStatus s) {
  GtfsMapResult g{};
  g.status = s;
  g.resolved = ResolveResult{ResolveStatus::Invalid, kNoGlobalIndex, 0, 0, nullptr};
  return g;
}

uint64_t tableHash() {
  uint64_t h = 1469598103934665603ull;
  const uint8_t *p = reinterpret_cast<const uint8_t *>(kMapLeds);
  for (size_t i = 0; i < sizeof kMapLeds; ++i) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

// Invariants that must hold for every frame.
void checkFrameInvariants(const TrainFrame &f) {
  uint32_t sum = 0;
  uint16_t occ = 0, col = 0;
  uint8_t mx = 0;
  for (uint16_t i = 0; i < kFrameLedCount; ++i) {
    const uint8_t c = f.occupancy[i];
    if (c) {
      ++occ;
      CHECK(semanticForLed(i) != nullptr);  // never a status LED
    }
    if (c > 1) ++col;
    if (c > mx) mx = c;
    sum += c;
  }
  CHECK(occ == f.occupiedLeds);
  CHECK(col == f.collisionLeds);
  CHECK(mx == f.maxOccupancy);
  CHECK(f.resolvedTrains == sum + f.saturatedTrains);
  CHECK(f.excessTrains == f.resolvedTrains - f.occupiedLeds);
}

uint16_t firstLed(Role role, uint8_t slotCount = 0) {
  for (uint16_t i = 0; i < kMapLedCount; ++i)
    if (kMapLeds[i].role == role && (slotCount == 0 || kMapLeds[i].segmentSlotCount == slotCount))
      return i;
  return 0xFFFF;
}

void testBasic() {
  static_assert(sizeof(TrainFrame) <= 340, "frame should stay a few hundred bytes");
  TrainFrame f;
  clearFrame(f);
  checkFrameInvariants(f);
  CHECK(f.resolvedTrains == 0 && f.occupiedLeds == 0 && f.collisionLeds == 0 && f.maxOccupancy == 0);

  const MapLed &st = kMapLeds[firstLed(Role::Station)];
  CHECK(addResolvedTrain(f, matchFor(st)) == FrameAdd::Added);
  CHECK(frameOccupancy(f, st.globalOrder) == 1 && frameOccupied(f, st.globalOrder));
  CHECK(f.resolvedTrains == 1 && f.occupiedLeds == 1 && f.collisionLeds == 0 && f.excessTrains == 0);
  checkFrameInvariants(f);

  const MapLed &sg = kMapLeds[firstLed(Role::Segment)];
  CHECK(addGtfsResult(f, gtfsMatch(sg)) == FrameAdd::Added);
  CHECK(f.occupiedLeds == 2 && f.resolvedTrains == 2);
  CHECK(isSegmentLed(sg.globalOrder) && isStationLed(st.globalOrder));
  checkFrameInvariants(f);

  // Distinct LEDs
  clearFrame(f);
  for (int i = 0; i < 10; ++i) addResolvedTrain(f, matchFor(kMapLeds[i * 7]));
  CHECK(f.occupiedLeds == 10 && f.collisionLeds == 0 && f.maxOccupancy == 1);
  checkFrameInvariants(f);

  // Same station LED x2, x3
  clearFrame(f);
  addResolvedTrain(f, matchFor(st));
  addResolvedTrain(f, matchFor(st));
  CHECK(frameOccupancy(f, st.globalOrder) == 2 && f.collisionLeds == 1 && f.excessTrains == 1);
  addResolvedTrain(f, matchFor(st));
  CHECK(frameOccupancy(f, st.globalOrder) == 3 && f.collisionLeds == 1 && f.excessTrains == 2 &&
        f.maxOccupancy == 3 && f.occupiedLeds == 1 && f.resolvedTrains == 3);
  checkFrameInvariants(f);

  // Same segment LED x2
  clearFrame(f);
  addResolvedTrain(f, matchFor(sg));
  addResolvedTrain(f, matchFor(sg));
  CHECK(frameOccupancy(f, sg.globalOrder) == 2 && f.collisionLeds == 1);

  // clear/reset
  clearFrame(f);
  CHECK(f.occupiedLeds == 0 && f.resolvedTrains == 0 && f.excessTrains == 0 && f.collisionLeds == 0);
  for (uint16_t i = 0; i < kFrameLedCount; ++i) CHECK(f.occupancy[i] == 0);
  checkFrameInvariants(f);
}

void testOppositeDirectionSegment() {
  // A single-pixel segment queried A->B and B->A resolves to one LED.
  unsigned found = 0;
  for (uint16_t i = 0; i < kMapLedCount; ++i) {
    const MapLed &m = kMapLeds[i];
    if (m.role != Role::Segment || m.segmentSlotCount != 1) continue;
    const ResolveResult ab = resolveTrainPosition(segmentPosition(m.route, m.endpointA, m.endpointB));
    const ResolveResult ba = resolveTrainPosition(segmentPosition(m.route, m.endpointB, m.endpointA));
    CHECK(ab.status == ResolveStatus::Match && ba.status == ResolveStatus::Match);
    CHECK(ab.globalIndex == ba.globalIndex);
    TrainFrame f;
    clearFrame(f);
    GtfsMapResult g{};
    g.status = GtfsMapStatus::Match;
    g.resolved = ab;
    addGtfsResult(f, g);
    g.resolved = ba;
    addGtfsResult(f, g);
    CHECK(frameOccupancy(f, ab.globalIndex) == 2 && f.collisionLeds == 1 && f.occupiedLeds == 1);
    ++found;
  }
  CHECK(found > 0);
  printf("opposite-direction single-pixel segments exercised: %u\n", found);
}

void testSaturation() {
  TrainFrame f;
  clearFrame(f);
  const MapLed &m = kMapLeds[0];
  for (int i = 0; i < 300; ++i) CHECK(addResolvedTrain(f, matchFor(m)) == FrameAdd::Added);
  CHECK(f.occupancy[m.globalOrder] == 255);  // no wraparound
  CHECK(f.maxOccupancy == 255 && f.saturatedTrains == 45 && f.resolvedTrains == 300);
  CHECK(f.excessTrains == 299 && f.collisionLeds == 1 && f.occupiedLeds == 1);
  checkFrameInvariants(f);
  // Other LEDs unaffected by saturation.
  addResolvedTrain(f, matchFor(kMapLeds[1]));
  CHECK(f.occupancy[kMapLeds[1].globalOrder] == 1 && f.occupancy[m.globalOrder] == 255);
}

void testIneligible() {
  TrainFrame f;
  clearFrame(f);
  const GtfsMapStatus bad[] = {GtfsMapStatus::Unrepresented, GtfsMapStatus::InsufficientPosition,
                               GtfsMapStatus::UnsupportedRoute, GtfsMapStatus::UnknownStop,
                               GtfsMapStatus::Invalid};
  for (GtfsMapStatus s : bad) CHECK(addGtfsResult(f, gtfsStatus(s)) == FrameAdd::Ignored);
  // A non-Match status is ignored even if the inner result looks like a Match.
  GtfsMapResult trap = gtfsMatch(kMapLeds[0]);
  trap.status = GtfsMapStatus::Unrepresented;
  CHECK(addGtfsResult(f, trap) == FrameAdd::Ignored);
  // Non-Match resolver statuses are ignored.
  const ResolveStatus rs[] = {ResolveStatus::NoMatch, ResolveStatus::Ambiguous, ResolveStatus::Invalid};
  for (ResolveStatus s : rs) {
    ResolveResult r = matchFor(kMapLeds[0]);
    r.status = s;
    CHECK(addResolvedTrain(f, r) == FrameAdd::Ignored);
  }
  CHECK(f.resolvedTrains == 0 && f.occupiedLeds == 0 && f.rejectedTrains == 0);
  CHECK(f.ignoredTrains == 5 + 1 + 3);
  checkFrameInvariants(f);
}

// Upstream categories that never reach the frame: the caller rule is
// "add only when the observation was adapted", and only ResolvedStation /
// ResolvedSegment categories are adapted Matches.
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

void feed(TrainFrame &f, const Observation &o) {
  if (o.adapted) addGtfsResult(f, o.result);
}

void testUpstreamIneligible() {
  TrainFrame f;
  clearFrame(f);
  Observation o = eval(train("W", "R30N", VehicleStatus::InTransitTo));
  CHECK(o.category == Category::UnsupportedRoute);
  feed(f, o);
  o = eval(train("A", "ZZZN", VehicleStatus::InTransitTo));
  CHECK(o.category == Category::UnknownStop);
  feed(f, o);
  o = eval(train("H", "H19N", VehicleStatus::InTransitTo));
  CHECK(o.category == Category::OperationalStop);
  feed(f, o);
  o = eval(train("", "R30N", VehicleStatus::StoppedAt));
  CHECK(o.category == Category::Invalid);
  feed(f, o);
  RtTrain stale = train("A", "A41N", VehicleStatus::StoppedAt);
  stale.vehicleTimestamp = 1;
  o = eval(stale);
  CHECK(o.category == Category::Skipped && o.reason == Reason::Stale);
  feed(f, o);
  RtTrain nv = train("A", "A41N", VehicleStatus::StoppedAt);
  nv.hasVehicle = false;
  o = eval(nv);
  CHECK(o.category == Category::Skipped);
  feed(f, o);
  CHECK(f.resolvedTrains == 0 && f.occupiedLeds == 0);

  // PreviousStopAmbiguous (real, from the compiled topology), never added.
  unsigned ambiguous = 0, unrepresented = 0;
  static const char *const kRoutes[] = {"2", "5", "A", "D", "J", "N", "Q", "R", "B", "C", "F"};
  static const char *const kStops[] = {"239N", "A48N", "J20N", "R31N", "239S", "A48S", "J20S", "R31S"};
  for (const char *route : kRoutes) {
    for (const char *stop : kStops) {
      o = eval(train(route, stop, VehicleStatus::InTransitTo));
      if (o.category == Category::Skipped && o.reason == Reason::PreviousStopAmbiguous) {
        ++ambiguous;
        CHECK(!o.adapted);
        feed(f, o);
      }
      if (o.category == Category::Unrepresented) ++unrepresented;
      if (o.category == Category::InsufficientPosition) {
        CHECK(addGtfsResult(f, o.result) == FrameAdd::Ignored);
      }
    }
  }
  CHECK(ambiguous > 0);
  CHECK(f.resolvedTrains == 0);
  printf("real PreviousStopAmbiguous observations exercised: %u (Unrepresented seen: %u)\n", ambiguous,
         unrepresented);

  // A resolved train is accepted.
  o = eval(train("2", "232N", VehicleStatus::StoppedAt));
  CHECK(o.category == Category::ResolvedStation);
  feed(f, o);
  CHECK(f.resolvedTrains == 1 && f.occupiedLeds == 1);
  checkFrameInvariants(f);
}

void testIdentityRejection() {
  TrainFrame f;
  clearFrame(f);
  // Every status LED: claimed Match must be rejected, with and without semantic.
  unsigned statusLeds = 0;
  for (uint16_t i = 0; i < led::map::TOTAL_LED_COUNT; ++i) {
    led::map::PhysicalLed p{};
    CHECK(led::map::physicalLed(led::GlobalPhysicalIndex{i}, p));
    if (p.role != led::LedRole::Status) continue;
    ++statusLeds;
    ResolveResult r{ResolveStatus::Match, i, static_cast<uint8_t>(p.address.channel), p.address.index,
                    nullptr};
    CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
    r.semantic = &kMapLeds[0];
    CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  }
  CHECK(statusLeds == 4);
  CHECK(f.rejectedTrains == 8 && f.resolvedTrains == 0 && f.occupiedLeds == 0);

  const MapLed &m = kMapLeds[5];
  ResolveResult good = matchFor(m);
  ResolveResult r = good;
  r.globalIndex = kNoGlobalIndex;
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.globalIndex = 306;
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.globalIndex = 0xFFFE;
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.semantic = nullptr;
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.semantic = &kMapLeds[6];  // different record than the index
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.channel = static_cast<uint8_t>(m.channel == 8 ? 1 : m.channel + 1);
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.channelIndex = static_cast<uint8_t>(m.channelIndex + 1);
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  r = good;
  r.globalIndex = kMapLeds[6].globalOrder;  // index disagrees with the semantic record
  CHECK(addResolvedTrain(f, r) == FrameAdd::Rejected);
  CHECK(f.resolvedTrains == 0 && f.occupiedLeds == 0);
  CHECK(addResolvedTrain(f, good) == FrameAdd::Added);
  checkFrameInvariants(f);
}

void testAll302() {
  const uint64_t before = tableHash();
  unsigned station = 0, segment = 0;
  TrainFrame all;
  clearFrame(all);
  for (uint16_t i = 0; i < kMapLedCount; ++i) {
    const MapLed &m = kMapLeds[i];
    TrainFrame f;
    clearFrame(f);
    CHECK(addResolvedTrain(f, matchFor(m)) == FrameAdd::Added);
    CHECK(f.occupiedLeds == 1 && f.occupancy[m.globalOrder] == 1);
    CHECK(routeForLed(m.globalOrder) == m.route && m.route != Route::None);
    CHECK(semanticForLed(m.globalOrder) == &m);
    CHECK(m.globalOrder < kFrameLedCount);
    checkFrameInvariants(f);
    CHECK(addResolvedTrain(all, matchFor(m)) == FrameAdd::Added);
    (m.role == Role::Station ? station : segment)++;
  }
  CHECK(station == 276 && segment == 26);
  CHECK(all.occupiedLeds == 302 && all.resolvedTrains == 302 && all.collisionLeds == 0);
  unsigned unoccupied = 0;
  for (uint16_t i = 0; i < kFrameLedCount; ++i) {
    if (!frameOccupied(all, i)) {
      ++unoccupied;
      CHECK(semanticForLed(i) == nullptr);  // exactly the four status LEDs
    }
  }
  CHECK(unoccupied == 4);
  CHECK(frameOccupancy(all, 306) == 0 && frameOccupancy(all, 0xFFFF) == 0);
  checkFrameInvariants(all);
  CHECK(tableHash() == before);  // frame operations never modify semantic tables
  printf("all 302 map LEDs individually and jointly occupied; 4 status LEDs stay empty\n");
}

void testOrderIndependence() {
  GtfsMapResult in[400];
  size_t n = 0;
  uint32_t seed = 12345;
  auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
  for (; n < 360; ++n) in[n] = gtfsMatch(kMapLeds[rnd() % 40 + (n % 3) * 20]);  // many collisions
  in[n++] = gtfsStatus(GtfsMapStatus::UnknownStop);
  in[n++] = gtfsStatus(GtfsMapStatus::Unrepresented);
  TrainFrame ref;
  composeTrainFrame(ref, in, n);
  checkFrameInvariants(ref);
  CHECK(ref.collisionLeds > 5);
  for (int round = 0; round < 50; ++round) {
    for (size_t i = n - 1; i > 0; --i) {
      const size_t j = rnd() % (i + 1);
      GtfsMapResult t = in[i];
      in[i] = in[j];
      in[j] = t;
    }
    TrainFrame f;
    const size_t added = composeTrainFrame(f, in, n);
    CHECK(added == 360);
    CHECK(memcmp(&f, &ref, sizeof f) == 0);
  }
  // compose resets previous content
  TrainFrame f;
  composeTrainFrame(f, in, n);
  composeTrainFrame(f, in, 0);
  CHECK(f.occupiedLeds == 0 && f.resolvedTrains == 0);
}

}  // namespace

int main() {
  testBasic();
  testOppositeDirectionSegment();
  testSaturation();
  testIneligible();
  testUpstreamIneligible();
  testIdentityRejection();
  testAll302();
  testOrderIndependence();
  printf("sizeof(TrainFrame) = %zu\n", sizeof(TrainFrame));
  printf("Phase 4G train-frame host tests: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
