// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) validation of the Phase 4E GTFS-RT
// decoder and state builder using synthetic protobuf feeds.
// Build/run with: python tools/run_gtfs_rt_tests.py

#include "gtfs_rt_state.h"

#include "generated/gtfs_stop_table.h"
#include "led_map.h"
#include "semantic_api.h"

#include <stdio.h>
#include <string.h>

using namespace brooklyn::gtfsrt;
namespace sem = brooklyn::semantic;

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

// ---- Minimal protobuf writer ----------------------------------------------

struct Buf {
  uint8_t d[16384];
  size_t n = 0;
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
  Buf &fixed32(uint32_t f) { tag(f, 5); for (int i = 0; i < 4; ++i) byte(0xAA); return *this; }
  Buf &fixed64(uint32_t f) { tag(f, 1); for (int i = 0; i < 8; ++i) byte(0xBB); return *this; }
};

Buf tripDesc(const char *trip, const char *route, int sched = -1, const char *trainId = nullptr,
             int nyctDir = 0) {
  Buf b;
  if (trip) b.str(1, trip);
  b.str(3, "20261014");
  if (sched >= 0) b.vint(4, static_cast<uint64_t>(sched));
  if (route) b.str(5, route);
  if (trainId || nyctDir) {
    Buf x;
    if (trainId) x.str(1, trainId);
    if (nyctDir) x.vint(3, static_cast<uint64_t>(nyctDir));
    b.msg(1001, x);
  }
  return b;
}

// status < 0: field omitted.
Buf vehicleEntity(const char *id, const Buf &trip, int status, const char *stop, uint32_t ts,
                  int seq = -1, const char *vehId = nullptr) {
  Buf v;
  v.msg(1, trip);
  if (seq >= 0) v.vint(3, static_cast<uint64_t>(seq));
  if (status >= 0) v.vint(4, static_cast<uint64_t>(status));
  if (ts) v.vint(5, ts);
  if (stop) v.str(7, stop);
  if (vehId) { Buf vd; vd.str(1, vehId); v.msg(8, vd); }
  Buf e;
  e.str(1, id);
  e.msg(4, v);
  return e;
}

struct Stu { const char *stop; uint32_t arr; uint32_t dep; };

Buf tripUpdateEntity(const char *id, const Buf &trip, const Stu *stus, int nStus, uint32_t ts) {
  Buf t;
  t.msg(1, trip);
  for (int i = 0; i < nStus; ++i) {
    Buf s;
    s.vint(1, static_cast<uint64_t>(i));
    if (stus[i].arr) { Buf ev; ev.vint(2, stus[i].arr); s.msg(2, ev); }
    if (stus[i].dep) { Buf ev; ev.vint(2, stus[i].dep); s.msg(3, ev); }
    s.str(4, stus[i].stop);
    t.msg(2, s);
  }
  if (ts) t.vint(4, ts);
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
};

constexpr uint32_t kNow = 1700000000;

constexpr size_t kCap = 64;
RtTrain gTrains[kCap];

struct Decoded {
  DecodeStatus status;
  size_t count;
  FeedStats stats;
};

Decoded decode(const Buf &b, size_t cap = kCap) {
  Decoded d{};
  d.status = decodeFeed(b.d, b.n, gTrains, cap, d.count, d.stats);
  return d;
}

// ---- Previous-stop providers -------------------------------------------------

struct PrevCtx {
  const char *prev;
  int calls;
};

bool fixedPrev(void *ctx, const RtTrain &, const char *, PreviousStop &out) {
  PrevCtx &c = *static_cast<PrevCtx *>(ctx);
  ++c.calls;
  if (!c.prev) return false;
  snprintf(out.stopId, sizeof out.stopId, "%s", c.prev);
  out.source = PreviousSource::ExactTrip;
  return true;
}

BuilderConfig config(PrevCtx *ctx, uint32_t maxAge = kDefaultMaxVehicleAgeSec) {
  BuilderConfig c{};
  c.now = 0;
  c.maxVehicleAgeSec = maxAge;
  c.previousStop = ctx ? fixedPrev : nullptr;
  c.previousStopContext = ctx;
  return c;
}

// ---- Map lookups -------------------------------------------------------------

const char *routeIdOf(sem::Route r) { return sem::kRouteNames[static_cast<uint8_t>(r)]; }

// First GTFS parent stop of a station (platform-free).
const char *firstStop(uint16_t stationId) {
  for (const sem::GtfsStop &s : sem::kGtfsStops)
    if (s.stationId == stationId) return s.stopId;
  return nullptr;
}

const sem::MapLed *findSegment(bool multiPixel) {
  for (const sem::MapLed &l : sem::kMapLeds)
    if (l.role == sem::Role::Segment && (l.segmentSlotCount > 1) == multiPixel &&
        firstStop(l.endpointA) && firstStop(l.endpointB))
      return &l;
  return nullptr;
}

const sem::MapLed *findStation(sem::Route route) {
  for (const sem::MapLed &l : sem::kMapLeds)
    if (l.role == sem::Role::Station && l.route == route && firstStop(l.stationId)) return &l;
  return nullptr;
}

Observation eval(const RtTrain &t, const BuilderConfig &cfg, uint32_t feedTs = kNow) {
  Observation o;
  evaluateTrain(t, feedTs, cfg, o);
  return o;
}

// Builds one merged train via the real decoder from a vehicle entity.
RtTrain oneVehicle(const char *route, int status, const char *stop, uint32_t ts = kNow,
                   const char *tripId = "010000_X..S01R") {
  Feed f(kNow);
  f.add(vehicleEntity("v", tripDesc(tripId, route), status, stop, ts));
  const Decoded d = decode(f.b);
  CHECK(d.status == DecodeStatus::Ok && d.count == 1);
  return gTrains[0];
}

// ---- Tests -------------------------------------------------------------------

void testDecodeBasics() {
  const Stu stus[] = {{"G29S", 1700000100, 1700000130}, {"G30S", 1700000300, 0}};
  Feed f(kNow);
  f.add(tripUpdateEntity("1", tripDesc("012000_G..S", "G", -1, "TRN 1", 3), stus, 2, 1699999990));
  f.add(vehicleEntity("2", tripDesc("012000_G..S", "G"), 1, "G29S", 1699999995, 4, "VEH9"));
  Buf alert;
  alert.str(1, "3");
  Buf al;
  al.str(1, "x");
  alert.msg(5, al);
  f.add(alert);

  const Decoded d = decode(f.b);
  CHECK(d.status == DecodeStatus::Ok);
  CHECK(d.count == 1);
  CHECK(d.stats.headerTimestamp == kNow);
  CHECK(d.stats.entities == 3);
  CHECK(d.stats.tripUpdates == 1 && d.stats.vehicles == 1 && d.stats.alerts == 1);
  CHECK(d.stats.malformedEntities == 0);

  const RtTrain &t = gTrains[0];
  CHECK(strcmp(t.tripId, "012000_G..S") == 0);
  CHECK(strcmp(t.routeId, "G") == 0);
  CHECK(strcmp(t.vehicleId, "VEH9") == 0);
  CHECK(strcmp(t.trainId, "TRN 1") == 0);
  CHECK(t.hasVehicle && t.hasTripUpdate);
  CHECK(t.status == VehicleStatus::StoppedAt);  // protobuf 1
  CHECK(strcmp(t.vehicleStopId, "G29S") == 0);
  CHECK(t.vehicleTimestamp == 1699999995 && t.tripUpdateTimestamp == 1699999990);
  CHECK(t.hasCurrentStopSequence && t.currentStopSequence == 4);
  CHECK(strcmp(t.nextStopId, "G29S") == 0);
  CHECK(t.nextStopArrival == 1700000100 && t.nextStopDeparture == 1700000130);
  CHECK(t.stopTimeUpdateCount == 2);
  CHECK(t.nyctDirection == NyctDirection::South);
  CHECK(t.scheduleRelationship == kScheduleRelationshipAbsent);
  CHECK(!t.fieldTruncated);
}

void testStatusEnumMapping() {
  CHECK(oneVehicle("G", 0, "G29S").status == VehicleStatus::IncomingAt);
  CHECK(oneVehicle("G", 1, "G29S").status == VehicleStatus::StoppedAt);
  CHECK(oneVehicle("G", 2, "G29S").status == VehicleStatus::InTransitTo);
  CHECK(oneVehicle("G", -1, "G29S").status == VehicleStatus::Absent);
  CHECK(oneVehicle("G", 9, "G29S").status == VehicleStatus::Absent);  // unknown enum value
}

void testMergeOrderAndMultipleTrains() {
  const Stu stus[] = {{"A51S", 1700000100, 0}};
  // Vehicle first, trip update second, plus a different trip on the same route.
  Feed f(kNow);
  f.add(vehicleEntity("a", tripDesc("T1_A..S", "A"), 1, "A51S", kNow));
  f.add(tripUpdateEntity("b", tripDesc("T1_A..S", "A"), stus, 1, kNow));
  f.add(vehicleEntity("c", tripDesc("T2_A..S", "A"), 1, "A51S", kNow));
  f.add(tripUpdateEntity("d", tripDesc("T3_A..N", "A"), stus, 1, kNow));
  const Decoded d = decode(f.b);
  CHECK(d.count == 3);
  CHECK(strcmp(gTrains[0].tripId, "T1_A..S") == 0 && gTrains[0].hasVehicle && gTrains[0].hasTripUpdate);
  CHECK(strcmp(gTrains[1].tripId, "T2_A..S") == 0 && gTrains[1].hasVehicle && !gTrains[1].hasTripUpdate);
  CHECK(strcmp(gTrains[2].tripId, "T3_A..N") == 0 && !gTrains[2].hasVehicle && gTrains[2].hasTripUpdate);
  // Same route, three distinct identities: none discarded.
  for (int i = 0; i < 3; ++i) CHECK(strcmp(gTrains[i].routeId, "A") == 0);

  // A route disagreement between TripUpdate and vehicle is counted, first wins.
  Feed g(kNow);
  g.add(tripUpdateEntity("x", tripDesc("TX", "A"), stus, 1, kNow));
  g.add(vehicleEntity("y", tripDesc("TX", "C"), 1, "A51S", kNow));
  const Decoded dg = decode(g.b);
  CHECK(dg.count == 1 && strcmp(gTrains[0].routeId, "A") == 0 && dg.stats.routeConflicts == 1);

  // Two entities of the same kind with one trip_id merge into one identity.
  Feed h(kNow);
  h.add(vehicleEntity("p", tripDesc("TD", "A"), 1, "A51S", kNow));
  h.add(vehicleEntity("q", tripDesc("TD", "A"), 2, "A52S", kNow));
  const Decoded dh = decode(h.b);
  CHECK(dh.count == 1 && gTrains[0].status == VehicleStatus::InTransitTo);
}

void testStationConversion() {
  // G29 stopped -> LED96 = G / Lorimer St (semantic name differs from GTFS).
  for (const char *stop : EACH(const char *, "G29", "G29N", "G29S")) {
    const RtTrain t = oneVehicle("G", 1, stop);
    const Observation o = eval(t, config(nullptr));
    CHECK(o.category == Category::ResolvedStation);
    CHECK(o.adapted && o.gtfsStatus == sem::GtfsVehicleStatus::StoppedAt);
    CHECK(o.result.position.stationId == 170);
    CHECK(o.result.resolved.semantic != nullptr && o.result.resolved.semantic->designator == 96);
    CHECK(strcmp(sem::stationName(170), "Lorimer St") == 0);
    CHECK(o.previous.source == PreviousSource::None);
  }
  // Stopped trains never need a previous stop: the provider is not consulted.
  PrevCtx ctx{nullptr, 0};
  eval(oneVehicle("G", 1, "G29S"), config(&ctx));
  CHECK(ctx.calls == 0);

  // Wrong route at a station: valid GTFS position, map has no LED.
  CHECK(eval(oneVehicle("A", 1, "G29S"), config(nullptr)).category == Category::Unrepresented);
}

void testBetweenStations() {
  const sem::MapLed *seg = findSegment(false);
  CHECK(seg != nullptr);
  if (!seg) return;
  const char *route = routeIdOf(seg->route);
  const char *a = firstStop(seg->endpointA);
  const char *b = firstStop(seg->endpointB);
  char bN[5], aS[5];
  snprintf(bN, sizeof bN, "%.3sN", b);
  snprintf(aS, sizeof aS, "%.3sS", a);

  // INCOMING_AT and IN_TRANSIT_TO both become a segment with unknown progress.
  for (int status = 0; status <= 2; status += 2) {
    PrevCtx ctx{a, 0};
    const Observation o = eval(oneVehicle(route, status, bN), config(&ctx));
    CHECK(o.category == Category::ResolvedSegment);
    CHECK(o.gtfsStatus == (status == 0 ? sem::GtfsVehicleStatus::IncomingAt
                                       : sem::GtfsVehicleStatus::InTransitTo));
    CHECK(ctx.calls == 1);
    CHECK(o.result.resolved.semantic == seg);
    CHECK(o.result.position.kind == sem::PositionKind::Segment);
  }
  // Opposite direction of travel resolves to the same pixel.
  PrevCtx back{b, 0};
  CHECK(eval(oneVehicle(route, 2, aS), config(&back)).result.resolved.semantic == seg);

  // Multi-pixel segment with no progress: honest InsufficientPosition, no LED.
  const sem::MapLed *multi = findSegment(true);
  CHECK(multi != nullptr);
  if (multi) {
    const char *mr = routeIdOf(multi->route);
    const char *ma = firstStop(multi->endpointA);
    char mb[5];
    snprintf(mb, sizeof mb, "%.3sN", firstStop(multi->endpointB));
    PrevCtx ctx{ma, 0};
    const Observation o = eval(oneVehicle(mr, 2, mb), config(&ctx));
    CHECK(o.category == Category::InsufficientPosition);
    CHECK(o.result.resolved.semantic == nullptr);
    CHECK(o.result.resolved.globalIndex == sem::kNoGlobalIndex);
    CHECK(sem::segmentPixelCount(multi->route, multi->endpointA, multi->endpointB) == multi->segmentSlotCount);
  }
}

void testUnsupportedAndShuttles() {
  for (const char *r : EACH(const char *, "W", "E", "1", "6", "7", "GS", "SI", "SS", "X")) {
    // Supported-route filtering happens before any other check, even with no vehicle.
    Feed f(kNow);
    const Stu s[] = {{"101S", kNow + 60, 0}};
    f.add(tripUpdateEntity("t", tripDesc("TU", r), s, 1, kNow));
    decode(f.b);
    CHECK(eval(gTrains[0], config(nullptr)).category == Category::UnsupportedRoute);
    CHECK(eval(oneVehicle(r, 1, "G29S"), config(nullptr)).category == Category::UnsupportedRoute);
  }
  // Shuttle ids FS and H alias to semantic S; GS is NOT a mapped shuttle.
  const sem::MapLed *shuttle = findStation(sem::Route::RouteS);
  CHECK(shuttle != nullptr);
  if (shuttle) {
    for (const char *r : EACH(const char *, "S", "FS", "H")) {
      const Observation o = eval(oneVehicle(r, 1, firstStop(shuttle->stationId)), config(nullptr));
      CHECK(o.category == Category::ResolvedStation);
      CHECK(o.result.position.route == sem::Route::RouteS);
      CHECK(o.result.resolved.semantic == shuttle);
    }
  }
  CHECK(eval(oneVehicle("GS", 1, "G29S"), config(nullptr)).category == Category::UnsupportedRoute);
  // All 18 semantic routes pass the route filter.
  for (uint8_t i = 1; i <= sem::kRouteCount; ++i) {
    const Observation o = eval(oneVehicle(sem::kRouteNames[i], 1, "ZZZ"), config(nullptr));
    CHECK(o.category != Category::UnsupportedRoute);
  }
}

// Rockaway Park Shuttle (route H): every H stop in the static GTFS that has a
// shuttle LED on the map must resolve H + stop -> Route S -> that station's S LED.
void testRockawayShuttleStops() {
  struct HStop {
    const char *parent;
    uint16_t stationId;
  };
  const HStop mapped[] = {{"H04", 154}, {"H12", 155}, {"H13", 156}, {"H14", 157}, {"H15", 158}};
  for (const HStop &s : mapped) {
    const sem::MapLed *led = nullptr;
    for (const sem::MapLed &l : sem::kMapLeds)
      if (l.role == sem::Role::Station && l.route == sem::Route::RouteS && l.stationId == s.stationId)
        led = &l;
    CHECK(led != nullptr);
    char forms[3][8];
    snprintf(forms[0], sizeof forms[0], "%s", s.parent);
    snprintf(forms[1], sizeof forms[1], "%sN", s.parent);
    snprintf(forms[2], sizeof forms[2], "%sS", s.parent);
    for (int i = 0; i < 3; ++i) {
      const Observation o = eval(oneVehicle("H", 1, forms[i]), config(nullptr));
      CHECK(o.category == Category::ResolvedStation);
      CHECK(o.result.position.route == sem::Route::RouteS);
      CHECK(led != nullptr && o.result.resolved.semantic == led);
    }
  }
  // H01-H03 are mapped stations (A-only LEDs): valid GTFS stop, no shuttle LED.
  for (const char *stop : EACH(const char *, "H01N", "H02S", "H03N")) {
    CHECK(eval(oneVehicle("H", 1, stop), config(nullptr)).category == Category::Unrepresented);
  }
  // H19 is a known realtime-only operational stop (absent from static GTFS). It is
  // deliberately NOT mapped to Broad Channel (H04): no LED, not UnknownStop.
  for (const char *stop : EACH(const char *, "H19", "H19N", "H19S")) {
    for (int status = 0; status <= 2; ++status) {
      const Observation o = eval(oneVehicle("H", status, stop), config(nullptr));
      CHECK(o.category == Category::OperationalStop);
      CHECK(!o.adapted);
    }
    // Still operational when the report is old: it is not downgraded to Stale.
    CHECK(eval(oneVehicle("H", 1, stop, kNow - 5000), config(nullptr)).category ==
          Category::OperationalStop);
    CHECK(isRealtimeOnlyOperationalStop("H", stop));
    // Route-specific: any other route reporting the same stop id is not exempt.
    CHECK(!isRealtimeOnlyOperationalStop("A", stop));
    CHECK(eval(oneVehicle("A", 1, stop), config(nullptr)).category == Category::UnknownStop);
  }
  CHECK(!isRealtimeOnlyOperationalStop("H", "H04N") && !isRealtimeOnlyOperationalStop("H", "H1"));
  CHECK(!isRealtimeOnlyOperationalStop(nullptr, "H19N") && !isRealtimeOnlyOperationalStop("H", nullptr));
  // H04 keeps resolving to Broad Channel (station 154) in parent, N and S forms.
  for (const char *stop : EACH(const char *, "H04", "H04N", "H04S")) {
    const Observation o = eval(oneVehicle("H", 1, stop), config(nullptr));
    CHECK(o.category == Category::ResolvedStation);
    CHECK(o.result.resolved.semantic != nullptr && o.result.resolved.semantic->stationId == 154);
  }
}

// M08 (M platform of Myrtle-Wyckoff, station 70) and R33 (N/R platform of 4 Av-9 St,
// station 86) are second GTFS parents of stations that already carry those routes' LEDs.
void testSecondaryParents() {
  for (const char *stop : EACH(const char *, "M08", "M08N", "M08S")) {
    const Observation o = eval(oneVehicle("M", 1, stop), config(nullptr));
    CHECK(o.category == Category::ResolvedStation);
    CHECK(o.result.position.route == sem::Route::RouteM);
    CHECK(o.result.resolved.semantic != nullptr && o.result.resolved.semantic->stationId == 70);
  }
  for (const char *stop : EACH(const char *, "R33", "R33N", "R33S")) {
    const Observation o = eval(oneVehicle("R", 1, stop), config(nullptr));
    CHECK(o.category == Category::ResolvedStation);
    CHECK(o.result.position.route == sem::Route::RouteR);
    CHECK(o.result.resolved.semantic != nullptr && o.result.resolved.semantic->stationId == 86);
  }
}

void testUnknownAndInvalidStops() {
  CHECK(eval(oneVehicle("A", 1, "A99N"), config(nullptr)).category == Category::UnknownStop);
  CHECK(eval(oneVehicle("A", 1, "ZZZ"), config(nullptr)).category == Category::UnknownStop);
  // Malformed stop id shapes are Invalid(AdapterInvalid), never guessed.
  for (const char *bad : EACH(const char *, "A", "A5", "a51", "A51X", "A51NN")) {
    const Observation o = eval(oneVehicle("A", 1, bad), config(nullptr));
    CHECK(o.category == Category::Invalid && o.reason == Reason::AdapterInvalid);
  }
  // A vehicle with a status but no stop id.
  const Observation m = eval(oneVehicle("A", 1, nullptr), config(nullptr));
  CHECK(m.category == Category::Invalid && m.reason == Reason::MissingStop);
  // No route id at all.
  Feed f(kNow);
  f.add(vehicleEntity("v", tripDesc("TNR", nullptr), 1, "A51S", kNow));
  decode(f.b);
  const Observation nr = eval(gTrains[0], config(nullptr));
  CHECK(nr.category == Category::Invalid && nr.reason == Reason::MissingRoute);
  // Same station for previous and current stop is contradictory.
  PrevCtx same{"G29", 0};
  const Observation o = eval(oneVehicle("G", 2, "G29N"), config(&same));
  CHECK(o.category == Category::Invalid && o.reason == Reason::AdapterInvalid);
}

void testSkips() {
  Feed f(kNow);
  f.add(vehicleEntity("v", tripDesc("TC", "G", 3), 1, "G29S", kNow));
  decode(f.b);
  Observation o = eval(gTrains[0], config(nullptr));
  CHECK(o.category == Category::Skipped && o.reason == Reason::Canceled);

  const Stu s[] = {{"G29S", kNow + 60, 0}};
  Feed g(kNow);
  g.add(tripUpdateEntity("t", tripDesc("TV", "G"), s, 1, kNow));
  decode(g.b);
  o = eval(gTrains[0], config(nullptr));
  CHECK(o.category == Category::Skipped && o.reason == Reason::NoVehicle);

  o = eval(oneVehicle("G", -1, "G29S"), config(nullptr));
  CHECK(o.category == Category::Skipped && o.reason == Reason::NotStarted);
  CHECK(!o.adapted);

  // In transit with no provider / provider says unknown: skipped, not guessed.
  o = eval(oneVehicle("G", 2, "G29S"), config(nullptr));
  CHECK(o.category == Category::Skipped && o.reason == Reason::NoPreviousStop);
  PrevCtx none{nullptr, 0};
  o = eval(oneVehicle("G", 2, "G29S"), config(&none));
  CHECK(o.category == Category::Skipped && o.reason == Reason::NoPreviousStop && none.calls == 1);
}

void testStale() {
  const uint32_t age = kDefaultMaxVehicleAgeSec;
  const BuilderConfig c = config(nullptr);
  CHECK(eval(oneVehicle("G", 1, "G29S", kNow - age), c).category == Category::ResolvedStation);  // boundary
  Observation o = eval(oneVehicle("G", 1, "G29S", kNow - age - 1), c);
  CHECK(o.category == Category::Skipped && o.reason == Reason::Stale);
  CHECK(eval(oneVehicle("G", 1, "G29S", kNow + 500), c).category == Category::ResolvedStation);  // future
  CHECK(eval(oneVehicle("G", 1, "G29S", 0), c).category == Category::ResolvedStation);  // no timestamp
  // Disabled threshold.
  CHECK(eval(oneVehicle("G", 1, "G29S", 1000), config(nullptr, 0)).category == Category::ResolvedStation);
  // Explicit reference time overrides the feed header.
  BuilderConfig later = config(nullptr);
  later.now = kNow + 1000;
  CHECK(eval(oneVehicle("G", 1, "G29S", kNow), later, kNow).reason == Reason::Stale);
  // Stale vehicles that are also unsupported stay UnsupportedRoute (route check is first).
  CHECK(eval(oneVehicle("W", 1, "G29S", 5), c).category == Category::UnsupportedRoute);
}

void testSharedLedAndDuplicates() {
  Feed f(kNow);
  for (int i = 0; i < 4; ++i) {
    char trip[24], id[8];
    snprintf(trip, sizeof trip, "%06d_G..S", 10000 + i);
    snprintf(id, sizeof id, "e%d", i);
    f.add(vehicleEntity(id, tripDesc(trip, "G"), 1, i % 2 ? "G29N" : "G29S", kNow));
  }
  const Decoded d = decode(f.b);
  CHECK(d.count == 4);
  int sameLed = 0;
  for (size_t i = 0; i < d.count; ++i) {
    const Observation o = eval(gTrains[i], config(nullptr));
    if (o.category == Category::ResolvedStation && o.result.resolved.semantic->designator == 96) ++sameLed;
  }
  CHECK(sameLed == 4);  // all four trains are kept and all resolve to LED96
}

void testPropagationOfAdapterStatuses() {
  const sem::MapLed *seg = findSegment(false);
  const sem::MapLed *multi = findSegment(true);
  CHECK(seg && multi);
  if (!seg || !multi) return;
  PrevCtx ps{firstStop(seg->endpointA), 0}, pm{firstStop(multi->endpointA), 0}, pg{"G29", 0};
  PrevCtx pu{"ZZZ", 0};
  char segTo[5], multiTo[5];
  snprintf(segTo, sizeof segTo, "%.3sN", firstStop(seg->endpointB));
  snprintf(multiTo, sizeof multiTo, "%.3sN", firstStop(multi->endpointB));

  struct Row { RtTrain t; PrevCtx *ctx; Category want; sem::GtfsMapStatus adapter; };
  const Row rows[] = {
      {oneVehicle("G", 1, "G29S"), nullptr, Category::ResolvedStation, sem::GtfsMapStatus::Match},
      {oneVehicle(routeIdOf(seg->route), 2, segTo), &ps, Category::ResolvedSegment, sem::GtfsMapStatus::Match},
      {oneVehicle(routeIdOf(multi->route), 2, multiTo), &pm, Category::InsufficientPosition,
       sem::GtfsMapStatus::InsufficientPosition},
      {oneVehicle("A", 1, "G29S"), nullptr, Category::Unrepresented, sem::GtfsMapStatus::Unrepresented},
      {oneVehicle("A", 1, "A99N"), nullptr, Category::UnknownStop, sem::GtfsMapStatus::UnknownStop},
      {oneVehicle("G", 2, "G29N"), &pg, Category::Invalid, sem::GtfsMapStatus::Invalid},
      {oneVehicle("G", 2, "G29N"), &pu, Category::UnknownStop, sem::GtfsMapStatus::UnknownStop},
  };
  for (const Row &r : rows) {
    const Observation o = eval(r.t, config(r.ctx));
    CHECK(o.category == r.want);
    CHECK(o.adapted && o.result.status == r.adapter);
  }
  // Names are stable and non-empty for every category/reason.
  for (int c = 0; c <= static_cast<int>(Category::OperationalStop); ++c)
    CHECK(strcmp(categoryName(static_cast<Category>(c)), "?") != 0);
  for (int r = 1; r <= static_cast<int>(Reason::NoPreviousStop); ++r)
    CHECK(strcmp(reasonName(static_cast<Reason>(r)), "?") != 0);
}

void testMalformedFeeds() {
  // Empty / null input.
  size_t n = 0;
  FeedStats st;
  CHECK(decodeFeed(nullptr, 0, gTrains, kCap, n, st) == DecodeStatus::Empty && n == 0);
  const uint8_t one = 0;
  CHECK(decodeFeed(&one, 0, gTrains, kCap, n, st) == DecodeStatus::Empty);

  Feed f(kNow);
  f.add(vehicleEntity("a", tripDesc("TA", "G"), 1, "G29S", kNow));
  f.add(vehicleEntity("b", tripDesc("TB", "G"), 1, "G29S", kNow));
  const size_t full = f.b.n;

  // Truncated mid-way through the second entity: first train is kept.
  Decoded d = decode(f.b);
  CHECK(d.status == DecodeStatus::Ok && d.count == 2);
  for (size_t cut : EACH(size_t, full - 1, full - 10, full - 20)) {
    Buf t = f.b;
    t.n = cut;
    d = decode(t);
    CHECK(d.status == DecodeStatus::Truncated);
    CHECK(d.count == 1 && strcmp(gTrains[0].tripId, "TA") == 0);
  }
  // Invalid wire type (7) in the outer framing.
  Buf bad;
  bad.byte(0x0F);
  bad.byte(0x00);
  CHECK(decode(bad).status == DecodeStatus::Malformed);
  // Field number 0.
  Buf zero;
  zero.byte(0x00);
  zero.byte(0x00);
  CHECK(decode(zero).status == DecodeStatus::Malformed);
  // Unterminated varint.
  Buf varint;
  for (int i = 0; i < 11; ++i) varint.byte(0xFF);
  CHECK(decode(varint).status == DecodeStatus::Truncated);
  // Declared length longer than the buffer.
  Buf over;
  over.tag(2, 2);
  over.varint(5000);
  over.byte(1);
  CHECK(decode(over).status == DecodeStatus::Truncated);

  // Per-entity damage is skipped and counted; neighbours survive.
  Feed g(kNow);
  Buf noTrip;  // vehicle entity whose trip descriptor has no trip_id
  {
    Buf v;
    v.msg(1, tripDesc(nullptr, "G"));
    v.vint(4, 1);
    noTrip.str(1, "n");
    noTrip.msg(4, v);
  }
  g.add(noTrip);
  Buf inner;  // vehicle payload whose nested trip message is truncated
  {
    Buf v;
    v.tag(1, 2);
    v.varint(50);
    v.byte(0x0A);
    inner.str(1, "i");
    inner.msg(4, v);
  }
  g.add(inner);
  Buf badEntity;  // entity whose own framing is broken (group wire type)
  badEntity.byte(0x0B);
  g.add(badEntity);
  g.add(vehicleEntity("ok", tripDesc("TOK", "G"), 1, "G29S", kNow));
  d = decode(g.b);
  CHECK(d.status == DecodeStatus::Ok);
  CHECK(d.count == 1 && strcmp(gTrains[0].tripId, "TOK") == 0);
  CHECK(d.stats.entities == 4);
  CHECK(d.stats.malformedEntities == 3);

  // Random garbage never crashes and never overruns (bounded by capacity).
  uint32_t seed = 12345;
  for (int iter = 0; iter < 2000; ++iter) {
    Buf junk;
    const size_t len = 1 + (seed % 300);
    for (size_t i = 0; i < len; ++i) {
      seed = seed * 1664525u + 1013904223u;
      junk.byte(static_cast<uint8_t>(seed >> 24));
    }
    decode(junk, 8);
  }
  CHECK(true);

  // Mutating every byte of a valid feed never crashes.
  for (size_t i = 0; i < full; ++i) {
    Buf m = f.b;
    m.d[i] ^= 0xFF;
    decode(m);
  }
  CHECK(true);
}

void testCapacityAndTruncatedIds() {
  Feed f(kNow);
  for (int i = 0; i < 3; ++i) {
    char trip[16], id[8];
    snprintf(trip, sizeof trip, "T%d", i);
    snprintf(id, sizeof id, "e%d", i);
    f.add(vehicleEntity(id, tripDesc(trip, "G"), 1, "G29S", kNow));
  }
  Decoded d = decode(f.b, 2);
  CHECK(d.status == DecodeStatus::CapacityExceeded && d.count == 2 && d.stats.droppedForCapacity == 1);

  Feed g(kNow);
  g.add(vehicleEntity("e", tripDesc("0123456789012345678901234567890123456789", "G"), 1, "G29S", kNow));
  d = decode(g.b);
  CHECK(d.count == 1 && gTrains[0].fieldTruncated && strlen(gTrains[0].tripId) == kTripIdMax - 1);
}

void testUnknownFieldsAndDeleted() {
  Buf trip = tripDesc("TF", "G");
  Buf v;
  v.msg(1, trip);
  v.fixed32(60);
  v.fixed64(61);
  v.vint(4, 1);
  v.str(7, "G29S");
  Buf e;
  e.str(1, "x");
  e.fixed32(99);
  e.msg(4, v);
  Feed f(kNow);
  f.add(e);
  Buf del;
  del.str(1, "d");
  del.vint(2, 1);
  Buf dv;
  dv.msg(1, tripDesc("TDEL", "G"));
  del.msg(4, dv);
  f.add(del);
  const Decoded d = decode(f.b);
  CHECK(d.status == DecodeStatus::Ok && d.count == 1 && strcmp(gTrains[0].tripId, "TF") == 0);
  CHECK(d.stats.deleted == 1 && d.stats.entities == 2);
  CHECK(gTrains[0].status == VehicleStatus::StoppedAt);
}

void testDirection() {
  RtTrain t = oneVehicle("A", 1, "A51S");
  CHECK(travelDirection(t) == 'S');
  t = oneVehicle("A", 1, "A51N");
  CHECK(travelDirection(t) == 'N');
  Feed f(kNow);
  f.add(vehicleEntity("a", tripDesc("032600_B..N65R", "B", -1, nullptr, 3), 1, "D08", kNow));
  decode(f.b);
  CHECK(travelDirection(gTrains[0]) == 'S');  // NYCT extension beats the trip_id marker
  Feed g(kNow);
  g.add(vehicleEntity("a", tripDesc("032600_B..N65R", "B"), 1, "D08", kNow));
  decode(g.b);
  CHECK(travelDirection(gTrains[0]) == 'N');
  Feed h(kNow);
  h.add(vehicleEntity("a", tripDesc("weird", "B"), 1, "D08", kNow));
  decode(h.b);
  CHECK(travelDirection(gTrains[0]) == '?');
}

}  // namespace

int main() {
  testDecodeBasics();
  testStatusEnumMapping();
  testMergeOrderAndMultipleTrains();
  testStationConversion();
  testBetweenStations();
  testUnsupportedAndShuttles();
  testRockawayShuttleStops();
  testSecondaryParents();
  testUnknownAndInvalidStops();
  testSkips();
  testStale();
  testSharedLedAndDuplicates();
  testPropagationOfAdapterStatuses();
  testMalformedFeeds();
  testCapacityAndTruncatedIds();
  testUnknownFieldsAndDeleted();
  testDirection();
  printf("Phase 4E GTFS-RT host tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
