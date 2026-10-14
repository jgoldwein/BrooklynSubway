// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) validation of the Phase 4I end-to-end
// train pipeline: raw GTFS-RT bytes -> TrainFrame -> RgbFrame.
// Build/run with: python tools/run_train_pipeline_tests.py [-- replay args]
//
// Replay args: groups of feed files separated by "|", one group per snapshot.

#include "train_pipeline.h"

#include "gtfs_rt_state.h"
#include "led_map.h"
#include "semantic_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace brooklyn::pipeline;
namespace rt = brooklyn::gtfsrt;
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

// ---- Minimal protobuf writer ----------------------------------------------

constexpr size_t kBufMax = 131072;

struct Buf {
  uint8_t *d;
  size_t n = 0;
  Buf() : d(static_cast<uint8_t *>(malloc(kBufMax))) {}
  Buf(const Buf &o) : d(static_cast<uint8_t *>(malloc(kBufMax))), n(o.n) { memcpy(d, o.d, o.n); }
  Buf &operator=(const Buf &) = delete;
  ~Buf() { free(d); }
  void byte(uint8_t b) { d[n++] = b; }
  void varint(uint64_t v) {
    while (v >= 0x80) { byte(static_cast<uint8_t>(v | 0x80)); v >>= 7; }
    byte(static_cast<uint8_t>(v));
  }
  void tag(uint32_t f, uint8_t w) { varint((static_cast<uint64_t>(f) << 3) | w); }
  Buf &vint(uint32_t f, uint64_t v) { tag(f, 0); varint(v); return *this; }
  Buf &str(uint32_t f, const char *s) {
    tag(f, 2); varint(strlen(s)); memcpy(d + n, s, strlen(s)); n += strlen(s); return *this;
  }
  Buf &msg(uint32_t f, const Buf &m) {
    tag(f, 2); varint(m.n); memcpy(d + n, m.d, m.n); n += m.n; return *this;
  }
};

Buf tripDesc(const char *trip, const char *route) {
  Buf b;
  if (trip) b.str(1, trip);
  b.str(3, "20261014");
  if (route) b.str(5, route);
  return b;
}

// status: protobuf VehicleStopStatus 0 incoming, 1 stopped, 2 in transit.
Buf vehicleEntity(const char *id, const Buf &trip, int status, const char *stop, uint32_t ts) {
  Buf v;
  v.msg(1, trip);
  if (status >= 0) v.vint(4, static_cast<uint64_t>(status));
  if (ts) v.vint(5, ts);
  if (stop) v.str(7, stop);
  Buf e;
  e.str(1, id);
  e.msg(4, v);
  return e;
}

Buf tripUpdateEntity(const char *id, const Buf &trip) {
  Buf t;
  t.msg(1, trip);
  Buf e;
  e.str(1, id);
  e.msg(3, t);
  return e;
}

struct Feed {
  Buf b;
  explicit Feed(uint32_t headerTs) {
    Buf h;
    h.str(1, "1.0");
    h.vint(2, 0);
    h.vint(3, headerTs);
    b.msg(1, h);
  }
  Feed &add(const Buf &entity) { b.msg(2, entity); return *this; }
  Feed &vehicle(const char *trip, const char *route, int status, const char *stop,
                uint32_t ts) {
    return add(vehicleEntity("e", tripDesc(trip, route), status, stop, ts));
  }
};

constexpr uint32_t kNow = 1700000000;

TrainPipeline gPipe;

void begin() { gPipe.beginCycle(kNow); }
void feed(const Feed &f) { gPipe.processFeed(f.b.d, f.b.n); }

// Sum of every per-vehicle bucket; must equal vehiclesSeen.
uint32_t bucketSum(const Counters &c) {
  return c.resolvedStation + c.resolvedSegment + c.unrepresented + c.insufficientPosition +
         c.unsupportedRoute + c.unknownStop + c.invalid + c.previousStopAmbiguous +
         c.operationalStop + c.stale + c.notStarted + c.skipped + c.duplicateIdentical +
         c.duplicateConflict + c.tripCapacityOverflow + c.frameRejected;
}

void checkInvariants() {
  const Counters &c = gPipe.counters();
  CHECK(bucketSum(c) == c.vehiclesSeen);
  CHECK(c.statusNonBlackPixels == 0);
  CHECK(c.rendererRejected == 0);
  CHECK(c.nonBlackPixels == c.occupiedLeds);
  // Status LEDs never carry occupancy or colour.
  for (uint16_t i = 0; i < sem::kFrameLedCount; ++i) {
    if (sem::semanticForLed(i) == nullptr) {
      CHECK(gPipe.frame().occupancy[i] == 0);
      const sem::Rgb &p = gPipe.rgb().pixels[i];
      CHECK(p.red == 0 && p.green == 0 && p.blue == 0);
    }
  }
}

uint16_t firstOccupied() {
  for (uint16_t i = 0; i < sem::kFrameLedCount; ++i)
    if (gPipe.frame().occupancy[i]) return i;
  return 0xFFFF;
}

// ---- Synthetic cases -----------------------------------------------------------

void testStoppedStation() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 1, "G29N", kNow);
  feed(f);
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.resolvedStation == 1 && c.resolvedSegment == 0);
  CHECK(c.occupiedLeds == 1 && c.nonBlackPixels == 1 && c.resolvedTrains == 1);
  CHECK(c.supportedVehicles == 1 && c.vehiclesSeen == 1);
  const sem::Rgb &p = gPipe.rgb().pixels[firstOccupied()];
  CHECK(p.red == 27 && p.green == 48 && p.blue == 17);  // G at brightness 64
  checkInvariants();
}

void testSingleIdenticalStationBothDirections() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 1, "G29N", kNow);
  f.vehicle("T2_G..S", "G", 1, "G29S", kNow);
  feed(f);
  gPipe.finishCycle();
  CHECK(gPipe.counters().resolvedStation == 2);
  CHECK(gPipe.counters().occupiedLeds == 1 && gPipe.counters().collisionLeds == 1);
  CHECK(gPipe.counters().maxOccupancy == 2);
  checkInvariants();
}

void testSinglePixelSegmentBothDirections() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 2, "G29N", kNow);  // previous stop G30
  f.vehicle("T2_G..S", "G", 2, "G30S", kNow);  // previous stop G29
  feed(f);
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.resolvedSegment == 2 && c.resolvedStation == 0);
  CHECK(c.occupiedLeds == 1 && gPipe.frame().occupancy[185] == 2);
  const sem::Rgb &p = gPipe.rgb().pixels[185];
  CHECK(p.red == 27 && p.green == 48 && p.blue == 17);
  checkInvariants();
}

const sem::MapLed *ledByDesignator(uint16_t designator) {
  for (uint16_t i = 0; i < sem::kFrameLedCount; ++i) {
    const sem::MapLed *m = sem::semanticForLed(i);
    if (m && m->designator == designator) return m;
  }
  return nullptr;
}

// The frozen Phase 3B/4C two-pixel groups: B Grand St..DeKalb (LED285 slot 0,
// LED309 slot 1) and D (LED286 slot 0, LED308 slot 1). The GTFS ids used below
// are checked against the canonical registry ids stored in the map records.
void testMultiPixelSegmentsEndToEnd() {
  const sem::MapLed *b0 = ledByDesignator(285), *b1 = ledByDesignator(309);
  const sem::MapLed *d0 = ledByDesignator(286), *d1 = ledByDesignator(308);
  CHECK(b0 && b1 && d0 && d1);
  if (!(b0 && b1 && d0 && d1)) return;
  CHECK(b0->route == sem::Route::RouteB && b1->route == sem::Route::RouteB);
  CHECK(d0->route == sem::Route::RouteD && d1->route == sem::Route::RouteD);
  CHECK(b0->role == sem::Role::Segment && d1->role == sem::Role::Segment);
  CHECK(b0->segmentSlot == 0 && b1->segmentSlot == 1 && b0->segmentSlotCount == 2 &&
        b1->segmentSlotCount == 2);
  CHECK(d0->segmentSlot == 0 && d1->segmentSlot == 1 && d0->segmentSlotCount == 2 &&
        d1->segmentSlotCount == 2);
  CHECK(b0->endpointA == b1->endpointA && b0->endpointB == b1->endpointB);
  CHECK(d0->endpointA == d1->endpointA && d0->endpointB == d1->endpointB);
  CHECK(b0->endpointA == d0->endpointA);       // shared endpoint (GTFS D22)
  CHECK(b0->endpointB != d0->endpointB);       // B ends at R30, D at R31
  const uint16_t shared = b0->endpointA;
  CHECK(sem::normalizeGtfsStop("D22N").stationId == shared);
  CHECK(sem::normalizeGtfsStop("R30S").stationId == b0->endpointB);
  CHECK(sem::normalizeGtfsStop("R31N").stationId == d0->endpointB);

  // B: both directions, raw feed -> pipeline. Unknown progress.
  begin();
  Feed fb(kNow);
  fb.vehicle("B1_B..N", "B", 2, "D22N", kNow);  // previous R30
  fb.vehicle("B2_B..S", "B", 2, "R30S", kNow);  // previous D22
  feed(fb);
  gPipe.finishCycle();
  const Counters &cb = gPipe.counters();
  CHECK(cb.insufficientPosition == 2 && cb.resolvedSegment == 0);
  CHECK(cb.occupiedLeds == 0 && cb.nonBlackPixels == 0 && cb.resolvedTrains == 0);
  CHECK(gPipe.frame().occupancy[b0->globalOrder] == 0 && gPipe.frame().occupancy[b1->globalOrder] == 0);
  checkInvariants();

  // D: with only VehiclePosition data the previous stop is ambiguous (D22 has
  // predecessors R30 and R31 northbound, R31 has D22 and R30 southbound), so the
  // pipeline refuses to guess. Nothing is drawn.
  begin();
  Feed fd(kNow);
  fd.vehicle("D1_D..N", "D", 2, "D22N", kNow);
  fd.vehicle("D2_D..S", "D", 2, "R31S", kNow);
  feed(fd);
  gPipe.finishCycle();
  const Counters &cd = gPipe.counters();
  CHECK(cd.previousStopAmbiguous == 2 && cd.insufficientPosition == 0);
  CHECK(cd.occupiedLeds == 0 && cd.nonBlackPixels == 0);
  CHECK(gPipe.frame().occupancy[d0->globalOrder] == 0 && gPipe.frame().occupancy[d1->globalOrder] == 0);
  checkInvariants();
}

const char *gForcedPrevious;

bool forcedPrevious(void *, const rt::RtTrain &, const char *, rt::PreviousStop &out) {
  memset(&out, 0, sizeof out);
  strcpy(out.stopId, gForcedPrevious);
  out.source = rt::PreviousSource::Realtime;
  return true;
}

struct DCapture {
  rt::Category category[8];
  uint32_t n;
};
DCapture gCap;

void captureVisitor(void *, const rt::RtTrain &t, uint32_t headerTs) {
  rt::BuilderConfig cfg{};
  cfg.now = kNow;
  cfg.maxVehicleAgeSec = rt::kDefaultMaxVehicleAgeSec;
  cfg.previousStop = forcedPrevious;
  rt::Observation o;
  rt::evaluateTrain(t, headerTs, cfg, o);
  if (gCap.n < 8) gCap.category[gCap.n++] = o.category;
}

// Raw feed -> streaming decoder -> state builder with a known previous stop:
// the D two-pixel segment keeps its Phase 4C behaviour (InsufficientPosition).
void testDSegmentWithKnownPreviousStop() {
  struct Case { const char *stop; const char *previous; } cases[] = {
      {"D22N", "R31"}, {"R31S", "D22"}};
  for (const Case &c : cases) {
    Feed f(kNow);
    f.vehicle("DX_D..N", "D", 2, c.stop, kNow);
    gForcedPrevious = c.previous;
    memset(&gCap, 0, sizeof gCap);
    rt::FeedStats stats;
    CHECK(rt::visitVehicles(f.b.d, f.b.n, captureVisitor, nullptr, stats) == rt::DecodeStatus::Ok);
    CHECK(gCap.n == 1 && gCap.category[0] == rt::Category::InsufficientPosition);
  }
}

void testUnknownUnrepresentedUnsupported() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_2..N", "2", 1, "120N", kNow);   // unknown stop
  f.vehicle("T2_2..N", "2", 2, "232N", kNow);   // unrepresented
  f.vehicle("T3_7..N", "7", 1, "702N", kNow);   // unsupported route
  feed(f);
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.unknownStop == 1 && c.unrepresented == 1 && c.unsupportedRoute == 1);
  CHECK(c.supportedVehicles == 2 && c.vehiclesSeen == 3);
  CHECK(c.occupiedLeds == 0);
  checkInvariants();
}

void testH19Operational() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_H..N", "H", 1, "H19N", kNow);
  f.vehicle("T2_H..S", "H", 1, "H19S", kNow);
  feed(f);
  gPipe.finishCycle();
  CHECK(gPipe.counters().operationalStop == 2);
  CHECK(gPipe.counters().unknownStop == 0 && gPipe.counters().occupiedLeds == 0);
  checkInvariants();
}

void testPreviousStopAmbiguous() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_A..N", "A", 2, "A42N", kNow);
  f.vehicle("T2_D..N", "D", 2, "D22N", kNow);
  feed(f);
  gPipe.finishCycle();
  CHECK(gPipe.counters().previousStopAmbiguous == 2);
  CHECK(gPipe.counters().occupiedLeds == 0);
  checkInvariants();
}

void testStale() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 1, "G29N", kNow - 1000);
  feed(f);
  gPipe.finishCycle();
  CHECK(gPipe.counters().stale == 1 && gPipe.counters().occupiedLeds == 0);
  checkInvariants();
}

void testMalformedEntityAndTripUpdate() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 1, "G29N", kNow);
  Buf bad;  // vehicle entity with no trip_id
  {
    Buf v;
    v.vint(4, 1).str(7, "G29N");
    bad.str(1, "bad");
    bad.msg(4, v);
  }
  f.add(bad);
  f.add(tripUpdateEntity("tu", tripDesc("T9_G..N", "G")));
  feed(f);
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.resolvedStation == 1 && c.malformedEntities == 1 && c.tripUpdatesIgnored == 1);
  CHECK(c.vehiclesSeen == 1 && c.entitiesSeen == 3);
  checkInvariants();
}

void testEmptyAndCorruptFeeds() {
  begin();
  CHECK(gPipe.processFeed(nullptr, 0) == rt::DecodeStatus::Empty);
  static const uint8_t junk[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  CHECK(gPipe.processFeed(junk, sizeof junk) != rt::DecodeStatus::Ok);
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 1, "G29N", kNow);
  f.b.n -= 5;  // truncate mid-entity
  CHECK(gPipe.processFeed(f.b.d, f.b.n) != rt::DecodeStatus::Ok);
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.feedsProcessed == 3 && c.feedDecodeFailures == 3);
  CHECK(c.occupiedLeds == 0 && c.nonBlackPixels == 0);
  checkInvariants();
}

// Deterministic pseudo-random corruption never lights an invalid pixel.
void testCorruptionFuzz() {
  Feed base(kNow);
  base.vehicle("T1_G..N", "G", 1, "G29N", kNow);
  base.vehicle("T2_R..N", "R", 2, "R29N", kNow);
  base.vehicle("T3_2..N", "2", 1, "120N", kNow);
  uint32_t seed = 12345;
  for (int iter = 0; iter < 2000; ++iter) {
    Feed f(kNow);
    f.b.n = 0;
    memcpy(f.b.d, base.b.d, base.b.n);
    f.b.n = base.b.n;
    for (int k = 0; k < 3; ++k) {
      seed = seed * 1664525u + 1013904223u;
      f.b.d[(seed >> 8) % f.b.n] = static_cast<uint8_t>(seed >> 24);
    }
    begin();
    gPipe.processFeed(f.b.d, f.b.n);
    gPipe.finishCycle();
    const Counters &c = gPipe.counters();
    CHECK(c.statusNonBlackPixels == 0 && c.rendererRejected == 0 && c.frameRejected == 0);
    CHECK(bucketSum(c) == c.vehiclesSeen);
  }
}

void testCollisions() {
  begin();
  Feed f(kNow);
  f.vehicle("A1_G..N", "G", 1, "G29N", kNow);
  f.vehicle("A2_G..N", "G", 1, "G29N", kNow);
  f.vehicle("A3_G..N", "G", 1, "G29S", kNow);
  f.vehicle("B1_R..N", "R", 1, "R29N", kNow);
  feed(f);
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.resolvedTrains == 4 && c.occupiedLeds == 2 && c.collisionLeds == 1);
  CHECK(c.maxOccupancy == 3 && c.excessTrains == 2);
  // Colour does not depend on occupancy.
  begin();
  Feed g(kNow);
  g.vehicle("A1_G..N", "G", 1, "G29N", kNow);
  feed(g);
  gPipe.finishCycle();
  const sem::Rgb one = gPipe.rgb().pixels[firstOccupied()];
  CHECK(one.red == 27 && one.green == 48 && one.blue == 17);
}

void testMultipleFeedsAndOrder() {
  Feed a(kNow), b(kNow);
  a.vehicle("A1_G..N", "G", 1, "G29N", kNow);
  b.vehicle("B1_R..N", "R", 1, "R29N", kNow);
  b.vehicle("B2_G..N", "G", 2, "G29N", kNow);  // segment G30-G29
  begin();
  feed(a);
  feed(b);
  gPipe.finishCycle();
  sem::TrainFrame f1 = gPipe.frame();
  sem::RgbFrame r1 = gPipe.rgb();
  const Counters c1 = gPipe.counters();
  CHECK(c1.feedsProcessed == 2 && c1.resolvedTrains == 3 && c1.occupiedLeds == 3);
  begin();
  feed(b);
  feed(a);
  gPipe.finishCycle();
  CHECK(memcmp(f1.occupancy, gPipe.frame().occupancy, sizeof f1.occupancy) == 0);
  CHECK(memcmp(&r1, &gPipe.rgb(), sizeof r1) == 0);
  CHECK(gPipe.counters().resolvedTrains == c1.resolvedTrains);
  checkInvariants();
}

void testCycleReset() {
  begin();
  Feed f(kNow);
  f.vehicle("T1_G..N", "G", 1, "G29N", kNow);
  feed(f);
  gPipe.finishCycle();
  CHECK(gPipe.counters().occupiedLeds == 1);
  begin();
  CHECK(gPipe.state() == CycleState::Collecting);
  CHECK(gPipe.counters().vehiclesSeen == 0 && gPipe.counters().occupiedLeds == 0);
  feed(f);  // same trip id again: a fresh cycle must not call it a duplicate
  gPipe.finishCycle();
  CHECK(gPipe.counters().duplicateIdentical == 0 && gPipe.counters().occupiedLeds == 1);
  // Finish is idempotent and later feeds are ignored.
  const Counters before = gPipe.counters();
  gPipe.finishCycle();
  CHECK(gPipe.processFeed(f.b.d, f.b.n) == rt::DecodeStatus::Empty);
  CHECK(memcmp(&before, &gPipe.counters(), sizeof before) == 0);
}

struct Snap {
  sem::TrainFrame frame;
  sem::RgbFrame rgb;
  Counters counters;
};
Snap gSnap[2];

void capture(Snap &s) {
  s.frame = gPipe.frame();
  s.rgb = gPipe.rgb();
  s.counters = gPipe.counters();
}

bool sameSnap(const Snap &a, const Snap &b) {
  return memcmp(a.frame.occupancy, b.frame.occupancy, sizeof a.frame.occupancy) == 0 &&
         memcmp(&a.rgb, &b.rgb, sizeof a.rgb) == 0 && memcmp(&a.counters, &b.counters, sizeof a.counters) == 0;
}

// Runs the feeds in the given order and snapshots the result.
void runOrder(Snap &out, const Feed *const *feeds, int n) {
  begin();
  for (int i = 0; i < n; ++i) feed(*feeds[i]);
  gPipe.finishCycle();
  capture(out);
  checkInvariants();
}

// All permutations of up to 3 feeds must give identical frames/pixels/counters.
void checkOrderIndependent(const Feed &a, const Feed &b, const Feed *c = nullptr) {
  static const int perms3[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  const Feed *all[3] = {&a, &b, c};
  const int n = c ? 3 : 2;
  const int count = c ? 6 : 2;
  const Feed *order[3];
  for (int k = 0; k < n; ++k) order[k] = all[k];
  runOrder(gSnap[0], order, n);
  for (int p = 1; p < count; ++p) {
    for (int k = 0; k < n; ++k) order[k] = all[c ? perms3[p][k] : (p == 1 ? 1 - k : k)];
    runOrder(gSnap[1], order, n);
    CHECK(sameSnap(gSnap[0], gSnap[1]));
  }
}

void testDuplicates() {
  // 1. Same trip twice with identical state, one feed (second record 5 s older:
  // the resolved outcome is what counts).
  begin();
  Feed f(kNow);
  f.vehicle("DUP_G..N", "G", 1, "G29N", kNow);
  f.vehicle("DUP_G..N", "G", 1, "G29N", kNow - 5);
  feed(f);
  gPipe.finishCycle();
  Counters c = gPipe.counters();
  CHECK(c.duplicateIdentical == 1 && c.duplicateConflict == 0 && c.conflictedTrips == 0);
  CHECK(c.resolvedStation == 1 && c.resolvedTrains == 1 && c.occupiedLeds == 1);
  CHECK(gPipe.frame().occupancy[firstOccupied()] == 1);
  checkInvariants();

  // 2. Same trip twice, conflicting state: rejected for the cycle, nothing drawn.
  begin();
  Feed g(kNow);
  g.vehicle("DUP_G..N", "G", 1, "G29N", kNow);
  g.vehicle("DUP_G..N", "G", 1, "G30N", kNow);
  feed(g);
  gPipe.finishCycle();
  c = gPipe.counters();
  CHECK(c.conflictedTrips == 1 && c.duplicateConflict == 2 && c.duplicateIdentical == 0);
  CHECK(c.resolvedStation == 0 && c.resolvedTrains == 0 && c.occupiedLeds == 0);
  checkInvariants();

  // 4. Duplicates split between two processFeed() calls, 5. in both orders.
  Feed a1(kNow), a2(kNow), b1(kNow), b2(kNow);
  a1.vehicle("DUP_G..N", "G", 1, "G29N", kNow);
  a2.vehicle("DUP_G..N", "G", 1, "G29N", kNow);
  checkOrderIndependent(a1, a2);
  CHECK(gSnap[0].counters.duplicateIdentical == 1 && gSnap[0].counters.resolvedTrains == 1);
  b1.vehicle("DUP_G..N", "G", 1, "G29N", kNow);
  b2.vehicle("DUP_G..N", "G", 1, "G30N", kNow);
  checkOrderIndependent(b1, b2);
  CHECK(gSnap[0].counters.conflictedTrips == 1 && gSnap[0].counters.duplicateConflict == 2 &&
        gSnap[0].counters.occupiedLeds == 0);

  // Three records of one trip: A,A,B / A,B,A / B,A,A all reject the trip.
  Feed x(kNow), y(kNow), z(kNow);
  x.vehicle("TRI_G..N", "G", 1, "G29N", kNow);
  y.vehicle("TRI_G..N", "G", 1, "G29N", kNow);
  z.vehicle("TRI_G..N", "G", 1, "G30N", kNow);
  checkOrderIndependent(x, y, &z);
  CHECK(gSnap[0].counters.conflictedTrips == 1 && gSnap[0].counters.duplicateConflict == 3);
  CHECK(gSnap[0].counters.occupiedLeds == 0);
  checkInvariants();

  // Mixed: unrelated trips plus duplicates, three feeds, every order.
  Feed m1(kNow), m2(kNow), m3(kNow);
  m1.vehicle("M1_G..N", "G", 1, "G29N", kNow);
  m1.vehicle("DUPI_R..N", "R", 1, "R29N", kNow);
  m2.vehicle("DUPI_R..N", "R", 1, "R29N", kNow);
  m2.vehicle("DUPC_G..N", "G", 1, "G29N", kNow);
  m3.vehicle("DUPC_G..N", "G", 2, "G30S", kNow);
  m3.vehicle("M3_G..S", "G", 1, "G29S", kNow);
  checkOrderIndependent(m1, m2, &m3);
  c = gSnap[0].counters;
  CHECK(c.duplicateIdentical == 1 && c.conflictedTrips == 1 && c.duplicateConflict == 2);
  CHECK(c.resolvedTrains == 3);  // M1, M3 (same G29 LED) and DUPI_R
  CHECK(c.occupiedLeds == 2 && c.collisionLeds == 1);

  // Distinct trips are not duplicates.
  begin();
  Feed d(kNow);
  d.vehicle("X1_G..N", "G", 1, "G29N", kNow);
  d.vehicle("X2_G..N", "G", 1, "G29N", kNow);
  feed(d);
  gPipe.finishCycle();
  CHECK(gPipe.counters().duplicateIdentical == 0 && gPipe.counters().duplicateConflict == 0);
  CHECK(gPipe.counters().resolvedTrains == 2 && gPipe.counters().maxOccupancy == 2);
}

void testHashCollision() {
  // Different trip_ids with an identical 32-bit FNV-1a hash (the old identity).
  // They must stay distinct trains, be reported as an identity collision, and
  // never as an ordinary duplicate.
  CHECK(tripHashPrimary("C449599") == tripHashPrimary("C612382"));
  CHECK(tripHashSecondary("C449599") != tripHashSecondary("C612382"));
  Feed f(kNow);
  f.vehicle("C449599", "G", 1, "G29N", kNow);
  f.vehicle("C612382", "G", 1, "G29S", kNow);
  Feed g(kNow), h(kNow);
  g.vehicle("C449599", "G", 1, "G29N", kNow);
  h.vehicle("C612382", "G", 1, "G30S", kNow);
  begin();
  feed(f);
  gPipe.finishCycle();
  Counters c = gPipe.counters();
  CHECK(c.tripIdentityCollision == 1);
  CHECK(c.duplicateIdentical == 0 && c.duplicateConflict == 0 && c.conflictedTrips == 0);
  CHECK(c.resolvedStation == 2 && c.resolvedTrains == 2 && c.maxOccupancy == 2);
  checkInvariants();
  // Different states, different feeds, both orders: still two distinct trains.
  checkOrderIndependent(g, h);
  c = gSnap[0].counters;
  CHECK(c.tripIdentityCollision == 1 && c.resolvedTrains == 2 && c.occupiedLeds == 2);
  CHECK(c.duplicateIdentical == 0 && c.duplicateConflict == 0);
}

void testCapacityOverflow() {
  const int total = kMaxTrackedTrips + 100;
  begin();
  int sent = 0;
  while (sent < total) {
    Feed f(kNow);
    for (int i = 0; i < 300 && sent < total; ++i, ++sent) {
      char id[24];
      snprintf(id, sizeof id, "OV%05d_G..N", sent);
      f.vehicle(id, "G", 1, "G29N", kNow);
    }
    feed(f);
  }
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  CHECK(c.vehiclesSeen == static_cast<uint32_t>(total));
  CHECK(c.tripCapacityOverflow == 100);
  CHECK(c.resolvedStation == kMaxTrackedTrips && c.duplicateIdentical == 0);
  CHECK(c.occupiedLeds == 1 && gPipe.frame().occupancy[firstOccupied()] == 255);  // saturated
  CHECK(c.excessTrains == kMaxTrackedTrips - 1);
  checkInvariants();
  // The pipeline keeps working after overflow and resets cleanly.
  begin();
  CHECK(gPipe.counters().tripCapacityOverflow == 0);
  Feed f(kNow);
  f.vehicle("OV00001_G..N", "G", 1, "G29N", kNow);
  feed(f);
  gPipe.finishCycle();
  CHECK(gPipe.counters().resolvedStation == 1 && gPipe.counters().duplicateIdentical == 0);
}

void testUnsupportedBypassesTable() {
  // Unsupported-route vehicles never consume trip-table capacity.
  begin();
  int sent = 0;
  while (sent < 1500) {
    Feed f(kNow);
    for (int i = 0; i < 300 && sent < 1500; ++i, ++sent) {
      char id[24];
      snprintf(id, sizeof id, "U%05d_7..N", sent);
      f.vehicle(id, "7", 1, "702N", kNow);
    }
    feed(f);
  }
  gPipe.finishCycle();
  CHECK(gPipe.counters().unsupportedRoute == 1500);
  CHECK(gPipe.counters().tripCapacityOverflow == 0);
  checkInvariants();
}

TrainPipeline gSelf;

void testSelfCheckAndSizes() {
  CHECK(selfCheckEmptyCycle(gSelf));
  CHECK(gSelf.counters().occupiedLeds == 0);
  printf("sizeof(TrainPipeline)=%zu Counters=%zu TrainFrame=%zu RgbFrame=%zu rt::RtTrain=%zu\n",
         sizeof(TrainPipeline), sizeof(Counters), sizeof(sem::TrainFrame), sizeof(sem::RgbFrame),
         sizeof(rt::RtTrain));
}

// ---- Replay against the merged host pipeline ---------------------------------------

struct Blob {
  uint8_t *data = nullptr;
  size_t len = 0;
};

bool readFile(const char *path, Blob &b) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return false;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  b.data = static_cast<uint8_t *>(malloc(static_cast<size_t>(n)));
  b.len = fread(b.data, 1, static_cast<size_t>(n), fp);
  fclose(fp);
  return true;
}

// Reference: full merged decode (TripUpdate + VehiclePosition), first-trip-wins
// dedup for supported routes, same builder configuration.
struct Reference {
  sem::TrainFrame frame;
  sem::RgbFrame rgb;
  uint32_t resolved = 0;
  uint32_t noVehicle = 0;
  uint32_t canceled = 0;
};

rt::RtTrain trains[1024];
char seen[2048][rt::kTripIdMax];
Reference gRef;

void buildReference(Blob *blobs, int n, uint32_t now, Reference &ref) {
  size_t nSeen = 0;
  sem::clearFrame(ref.frame);
  for (int i = 0; i < n; ++i) {
    size_t count = 0;
    rt::FeedStats stats;
    rt::decodeFeed(blobs[i].data, blobs[i].len, trains, 1024, count, stats);
    for (size_t k = 0; k < count; ++k) {
      const rt::RtTrain &t = trains[k];
      if (!t.hasVehicle) { ++ref.noVehicle; continue; }
      if (sem::routeFromGtfsRouteId(t.routeId) != sem::Route::None) {
        bool dup = false;
        for (size_t s = 0; s < nSeen; ++s)
          if (strcmp(seen[s], t.tripId) == 0) { dup = true; break; }
        if (dup) continue;
        if (nSeen < 2048) strcpy(seen[nSeen++], t.tripId);
      }
      rt::BuilderConfig cfg{};
      cfg.now = now;
      cfg.maxVehicleAgeSec = rt::kDefaultMaxVehicleAgeSec;
      cfg.previousStop = rt::topologyPreviousStop;
      cfg.previousStopContext = nullptr;
      rt::Observation o;
      rt::evaluateTrain(t, stats.headerTimestamp, cfg, o);
      if (o.category == rt::Category::ResolvedStation ||
          o.category == rt::Category::ResolvedSegment) {
        if (sem::addGtfsResult(ref.frame, o.result) == sem::FrameAdd::Added) ++ref.resolved;
      } else if (o.category == rt::Category::Skipped && o.reason == rt::Reason::Canceled) {
        ++ref.canceled;
      }
    }
  }
  memset(&ref.rgb, 0, sizeof ref.rgb);
  sem::renderTrainFrame(ref.frame, ref.rgb, sem::RenderConfig{});
}

void replay(Blob *blobs, int n, const char *label, uint32_t now) {
  memset(&gRef, 0, sizeof gRef);
  buildReference(blobs, n, now, gRef);
  gPipe.beginCycle(now);
  size_t bytes = 0, largest = 0;
  for (int i = 0; i < n; ++i) {
    gPipe.processFeed(blobs[i].data, blobs[i].len);
    bytes += blobs[i].len;
    if (blobs[i].len > largest) largest = blobs[i].len;
  }
  gPipe.finishCycle();
  const Counters &c = gPipe.counters();
  const bool frameEq = memcmp(gRef.frame.occupancy, gPipe.frame().occupancy,
                              sizeof gRef.frame.occupancy) == 0;
  const bool rgbEq = memcmp(&gRef.rgb, &gPipe.rgb(), sizeof gRef.rgb) == 0;
  CHECK(frameEq);
  CHECK(rgbEq);
  CHECK(bucketSum(c) == c.vehiclesSeen);
  CHECK(c.statusNonBlackPixels == 0 && c.rendererRejected == 0 && c.frameRejected == 0);
  CHECK(c.tripCapacityOverflow == 0 && c.feedDecodeFailures == 0);
  CHECK(c.resolvedTrains == gRef.resolved);
  // The reference keeps the first duplicate; the snapshots contain none, so the
  // two policies must agree here.
  CHECK(c.duplicateIdentical == 0 && c.duplicateConflict == 0 && c.tripIdentityCollision == 0);
  // Reversed feed order must reproduce the same frame, pixels and counters.
  capture(gSnap[0]);
  gPipe.beginCycle(now);
  for (int i = n - 1; i >= 0; --i) gPipe.processFeed(blobs[i].data, blobs[i].len);
  gPipe.finishCycle();
  capture(gSnap[1]);
  const bool orderEq = sameSnap(gSnap[0], gSnap[1]);
  CHECK(orderEq);
  printf("REPLAY %s: feeds=%u bytes=%zu largestFeed=%zu entities=%u vehicles=%u supported=%u "
         "tripUpdatesIgnored=%u\n",
         label, c.feedsProcessed, bytes, largest, c.entitiesSeen, c.vehiclesSeen,
         c.supportedVehicles, c.tripUpdatesIgnored);
  printf("  resolvedStation=%u resolvedSegment=%u unrepresented=%u insufficient=%u "
         "unsupported=%u unknown=%u invalid=%u prevAmbiguous=%u operational=%u stale=%u "
         "notStarted=%u skipped=%u duplicateIdentical=%u malformed=%u\n",
         c.resolvedStation, c.resolvedSegment, c.unrepresented, c.insufficientPosition,
         c.unsupportedRoute, c.unknownStop, c.invalid, c.previousStopAmbiguous,
         c.operationalStop, c.stale, c.notStarted, c.skipped, c.duplicateIdentical,
         c.malformedEntities);
  printf("  frame: resolvedTrains=%u occupiedLeds=%u collisionLeds=%u excess=%u maxOcc=%u "
         "nonBlack=%u statusNonBlack=%u rejected=%u\n",
         c.resolvedTrains, c.occupiedLeds, c.collisionLeds, c.excessTrains, c.maxOccupancy,
         c.nonBlackPixels, c.statusNonBlackPixels, c.rendererRejected);
  printf("  vs merged host pipeline: TrainFrame %s, RgbFrame %s (host-only skips: noVehicle=%u "
         "canceled=%u); reversed feed order: %s\n",
         frameEq ? "IDENTICAL" : "DIFFERENT", rgbEq ? "IDENTICAL" : "DIFFERENT", gRef.noVehicle,
         gRef.canceled, orderEq ? "IDENTICAL" : "DIFFERENT");
}

Blob gBlobs[16];
char gLabel[128];
uint32_t gNow;

void flushGroup(int &n, bool &open) {
  if (open && n > 0) replay(gBlobs, n, gLabel, gNow);
  for (int i = 0; i < n; ++i) { free(gBlobs[i].data); gBlobs[i] = Blob{}; }
  n = 0;
  open = false;
}

}  // namespace

int main(int argc, char **argv) {
  testStoppedStation();
  testSingleIdenticalStationBothDirections();
  testSinglePixelSegmentBothDirections();
  testMultiPixelSegmentsEndToEnd();
  testDSegmentWithKnownPreviousStop();
  testUnknownUnrepresentedUnsupported();
  testH19Operational();
  testPreviousStopAmbiguous();
  testStale();
  testMalformedEntityAndTripUpdate();
  testEmptyAndCorruptFeeds();
  testCorruptionFuzz();
  testCollisions();
  testMultipleFeedsAndOrder();
  testCycleReset();
  testDuplicates();
  testHashCollision();
  testCapacityOverflow();
  testUnsupportedBypassesTable();
  testSelfCheckAndSizes();

  // Replay groups: "<label>@<now>" then files, groups separated by "|".
  int n = 0;
  bool open = false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "|") == 0) { flushGroup(n, open); continue; }
    if (!open) {
      char *at = strchr(argv[i], '@');
      snprintf(gLabel, sizeof gLabel, "%.*s", at ? static_cast<int>(at - argv[i]) : 100, argv[i]);
      gNow = at ? static_cast<uint32_t>(strtoul(at + 1, nullptr, 10)) : 0;
      open = true;
      continue;
    }
    if (n < 16 && readFile(argv[i], gBlobs[n])) ++n;
    else printf("cannot read %s\n", argv[i]);
  }
  flushGroup(n, open);
  printf("Phase 4I pipeline tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
