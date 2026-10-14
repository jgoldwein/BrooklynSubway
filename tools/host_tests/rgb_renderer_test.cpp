// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Host-side (no hardware, no network) validation of the Phase 4H RGB renderer.
// Build/run with: python tools/run_rgb_renderer_tests.py

#include "led_map.h"
#include "rgb_renderer.h"
#include "semantic_api.h"
#include "train_frame.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

using namespace brooklyn::semantic;
namespace led = brooklyn::led;

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

bool eq(Rgb a, Rgb b) { return a.red == b.red && a.green == b.green && a.blue == b.blue; }
bool black(Rgb a) { return a.red == 0 && a.green == 0 && a.blue == 0; }

bool isStatus(uint16_t i) { return semanticForLed(i) == nullptr; }

RenderConfig cfg(uint8_t b) {
  RenderConfig c;
  c.brightness = b;
  return c;
}

// Independent expected table: route -> 0xRRGGBB.
struct Expect { Route route; uint32_t rgb; };
const Expect kExpect[] = {
    {Route::Route2, 0xEE352E}, {Route::Route3, 0xEE352E}, {Route::Route4, 0x00933C},
    {Route::Route5, 0x00933C}, {Route::RouteA, 0x0039A6}, {Route::RouteC, 0x0039A6},
    {Route::RouteB, 0xFF6319}, {Route::RouteD, 0xFF6319}, {Route::RouteF, 0xFF6319},
    {Route::RouteM, 0xFF6319}, {Route::RouteG, 0x6CBE45}, {Route::RouteJ, 0x996633},
    {Route::RouteZ, 0x996633}, {Route::RouteL, 0xA7A9AC}, {Route::RouteN, 0xFCCC0A},
    {Route::RouteQ, 0xFCCC0A}, {Route::RouteR, 0xFCCC0A}, {Route::RouteS, 0x808183},
};

Rgb fromHex(uint32_t v) {
  return Rgb{static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
}

Rgb refScale(Rgb c, uint8_t b) {
  return Rgb{static_cast<uint8_t>((c.red * b + 127) / 255), static_cast<uint8_t>((c.green * b + 127) / 255),
             static_cast<uint8_t>((c.blue * b + 127) / 255)};
}

uint16_t firstLed(Role role) {
  for (uint16_t i = 0; i < kMapLedCount; ++i)
    if (kMapLeds[i].role == role) return kMapLeds[i].globalOrder;
  return 0xFFFF;
}

uint64_t hashBytes(const void *p, size_t n) {
  uint64_t h = 1469598103934665603ull;
  const uint8_t *b = static_cast<const uint8_t *>(p);
  for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
  return h;
}

void checkStatusBlack(const RgbFrame &f) {
  int seen = 0;
  for (uint16_t i = 0; i < kFrameLedCount; ++i)
    if (isStatus(i)) { ++seen; CHECK(black(f.pixels[i])); }
  CHECK(seen == 4);
}

}  // namespace

int main() {
  static_assert(sizeof(Rgb) == 3, "");
  printf("sizeof(Rgb)=%zu sizeof(RgbFrame)=%zu sizeof(RenderConfig)=%zu sizeof(TrainFrame)=%zu\n",
         sizeof(Rgb), sizeof(RgbFrame), sizeof(RenderConfig), sizeof(TrainFrame));
  CHECK(sizeof(RgbFrame) == 306 * 3);
  CHECK(RenderConfig{}.brightness == 64);
  CHECK(kDefaultRenderBrightness == 64);

  // 1. empty frame -> all black
  {
    TrainFrame tf; clearFrame(tf);
    RgbFrame rf; memset(&rf, 0xAA, sizeof rf);
    RenderResult r = renderTrainFrame(tf, rf, cfg(255));
    for (uint16_t i = 0; i < kFrameLedCount; ++i) CHECK(black(rf.pixels[i]));
    CHECK(r.renderedLeds == 0 && r.rejectedLeds == 0);
  }

  // 2, 17, 18, 19: route colours
  for (const Expect &e : kExpect) CHECK(eq(nominalRouteColor(e.route), fromHex(e.rgb)));
  CHECK(black(nominalRouteColor(Route::None)));
  CHECK(black(nominalRouteColor(static_cast<Route>(99))));
  CHECK(eq(nominalRouteColor(Route::Route2), nominalRouteColor(Route::Route3)));
  CHECK(eq(nominalRouteColor(Route::Route4), nominalRouteColor(Route::Route5)));
  CHECK(eq(nominalRouteColor(Route::RouteA), nominalRouteColor(Route::RouteC)));
  CHECK(eq(nominalRouteColor(Route::RouteB), nominalRouteColor(Route::RouteD)));
  CHECK(eq(nominalRouteColor(Route::RouteB), nominalRouteColor(Route::RouteF)));
  CHECK(eq(nominalRouteColor(Route::RouteB), nominalRouteColor(Route::RouteM)));
  CHECK(eq(nominalRouteColor(Route::RouteJ), nominalRouteColor(Route::RouteZ)));
  CHECK(eq(nominalRouteColor(Route::RouteN), nominalRouteColor(Route::RouteQ)));
  CHECK(eq(nominalRouteColor(Route::RouteN), nominalRouteColor(Route::RouteR)));
  {
    // families differ from each other
    const Route fam[] = {Route::Route2, Route::Route4, Route::RouteA, Route::RouteB, Route::RouteG,
                         Route::RouteJ, Route::RouteL, Route::RouteN, Route::RouteS};
    for (size_t a = 0; a < 9; ++a)
      for (size_t b = a + 1; b < 9; ++b) CHECK(!eq(nominalRouteColor(fam[a]), nominalRouteColor(fam[b])));
  }

  // 3, 4, 5: brightness
  for (const Expect &e : kExpect) {
    const Rgb n = fromHex(e.rgb);
    CHECK(eq(scaleBrightness(n, 255), n));
    CHECK(black(scaleBrightness(n, 0)));
    CHECK(eq(scaleBrightness(n, 64), refScale(n, 64)));
  }
  CHECK(eq(scaleBrightness(Rgb{255, 255, 255}, 64), (Rgb{64, 64, 64})));
  CHECK(eq(scaleBrightness(Rgb{0xEE, 0x35, 0x2E}, 64), (Rgb{60, 13, 12})));
  CHECK(eq(scaleBrightness(Rgb{1, 2, 3}, 128), (Rgb{1, 1, 2})));
  for (int b = 0; b < 256; ++b) {
    for (int v = 0; v < 256; ++v) {
      const Rgb s = scaleBrightness(Rgb{static_cast<uint8_t>(v), 0, 0}, static_cast<uint8_t>(b));
      CHECK(s.red == (v * b + 127) / 255 && s.red <= v && s.green == 0 && s.blue == 0);
    }
  }

  // 6, 7: single station / segment
  const Role roles[] = {Role::Station, Role::Segment};  for (Role role : roles) {
    const uint16_t idx = firstLed(role);
    TrainFrame tf; clearFrame(tf);
    tf.occupancy[idx] = 1; tf.occupiedLeds = 1;
    RgbFrame rf;
    RenderResult r = renderTrainFrame(tf, rf, cfg(255));
    CHECK(eq(rf.pixels[idx], nominalRouteColor(semanticForLed(idx)->route)));
    CHECK(r.renderedLeds == 1 && r.rejectedLeds == 0);
    CHECK(role == Role::Station ? (r.stationLeds == 1 && r.segmentLeds == 0)
                                : (r.segmentLeds == 1 && r.stationLeds == 0));
    for (uint16_t i = 0; i < kFrameLedCount; ++i) if (i != idx) CHECK(black(rf.pixels[i]));
    checkStatusBlack(rf);
  }

  // 8, 20: every map LED individually
  for (uint16_t k = 0; k < kMapLedCount; ++k) {
    const uint16_t idx = kMapLeds[k].globalOrder;
    TrainFrame tf; clearFrame(tf);
    tf.occupancy[idx] = 1; tf.occupiedLeds = 1;
    RgbFrame rf;
    RenderResult r = renderTrainFrame(tf, rf, cfg(255));
    CHECK(r.renderedLeds == 1 && r.rejectedLeds == 0);
    CHECK(eq(rf.pixels[idx], nominalRouteColor(semanticForLed(idx)->route)));
    CHECK(!black(rf.pixels[idx]));
    int nonblack = 0;
    for (uint16_t i = 0; i < kFrameLedCount; ++i) if (!black(rf.pixels[i])) ++nonblack;
    CHECK(nonblack == 1);
    checkStatusBlack(rf);
  }

  // 9, 10: all 302 together
  {
    TrainFrame tf; clearFrame(tf);
    for (uint16_t k = 0; k < kMapLedCount; ++k) tf.occupancy[kMapLeds[k].globalOrder] = 1;
    tf.occupiedLeds = kMapLedCount;
    RgbFrame rf;
    RenderResult r = renderTrainFrame(tf, rf, cfg(64));
    CHECK(r.renderedLeds == 302 && r.stationLeds == 276 && r.segmentLeds == 26);
    CHECK(r.rejectedLeds == 0 && r.occupiedStatusLeds == 0);
    for (uint16_t k = 0; k < kMapLedCount; ++k) {
      const uint16_t idx = kMapLeds[k].globalOrder;
      CHECK(eq(rf.pixels[idx], scaleBrightness(nominalRouteColor(semanticForLed(idx)->route), 64)));
    }
    checkStatusBlack(rf);
    // brightness 0: all black but still counted valid
    RenderResult r0 = renderTrainFrame(tf, rf, cfg(0));
    for (uint16_t i = 0; i < kFrameLedCount; ++i) CHECK(black(rf.pixels[i]));
    CHECK(r0.rejectedLeds == 0);
  }

  // 11-15: occupancy independence, counters/input unchanged, determinism
  {
    const uint16_t idx = firstLed(Role::Station);
    const uint8_t counts[] = {1, 2, 3, 255};
    Rgb ref{};
    for (size_t c = 0; c < 4; ++c) {
      TrainFrame tf; clearFrame(tf);
      tf.occupancy[idx] = counts[c];
      tf.occupiedLeds = 1; tf.collisionLeds = counts[c] > 1; tf.maxOccupancy = counts[c];
      tf.resolvedTrains = counts[c]; tf.excessTrains = counts[c] - 1;
      TrainFrame before = tf;
      RgbFrame a, b;
      renderTrainFrame(tf, a, cfg(64));
      renderTrainFrame(tf, b, cfg(64));
      CHECK(memcmp(&a, &b, sizeof a) == 0);
      CHECK(memcmp(&tf, &before, sizeof tf) == 0);
      CHECK(tf.collisionLeds == before.collisionLeds && tf.maxOccupancy == before.maxOccupancy);
      if (c == 0) ref = a.pixels[idx];
      CHECK(eq(a.pixels[idx], ref));
    }
  }

  // 16: corrupt occupied status LEDs are rejected and stay black
  {
    for (uint16_t s = 0; s < kFrameLedCount; ++s) {
      if (!isStatus(s)) continue;
      TrainFrame tf; clearFrame(tf);
      tf.occupancy[s] = 1;
      RgbFrame rf; memset(&rf, 0x55, sizeof rf);
      RenderResult r = renderTrainFrame(tf, rf, cfg(255));
      CHECK(r.rejectedLeds == 1 && r.occupiedStatusLeds == 1 && r.renderedLeds == 0);
      for (uint16_t i = 0; i < kFrameLedCount; ++i) CHECK(black(rf.pixels[i]));
    }
    // all four corrupt plus one valid map LED
    TrainFrame tf; clearFrame(tf);
    for (uint16_t s = 0; s < kFrameLedCount; ++s) if (isStatus(s)) tf.occupancy[s] = 7;
    const uint16_t idx = firstLed(Role::Segment);
    tf.occupancy[idx] = 2;
    RgbFrame rf;
    RenderResult r = renderTrainFrame(tf, rf, cfg(255));
    CHECK(r.rejectedLeds == 4 && r.occupiedStatusLeds == 4 && r.renderedLeds == 1);
    CHECK(eq(rf.pixels[idx], nominalRouteColor(semanticForLed(idx)->route)));
    checkStatusBlack(rf);
  }

  // every map LED has a real route and renders non-black; semantic table untouched by render
  {
    const uint64_t h0 = hashBytes(kMapLeds, sizeof kMapLeds);
    for (uint16_t k = 0; k < kMapLedCount; ++k) {
      CHECK(kMapLeds[k].route != Route::None);
      CHECK(!black(nominalRouteColor(kMapLeds[k].route)));
    }
    TrainFrame tf; clearFrame(tf);
    RgbFrame rf;
    renderTrainFrame(tf, rf, cfg(64));
    CHECK(hashBytes(kMapLeds, sizeof kMapLeds) == h0);
  }

  // repeated render of a mixed frame is byte-identical
  {
    TrainFrame tf; clearFrame(tf);
    for (uint16_t k = 0; k < kMapLedCount; k += 3) tf.occupancy[kMapLeds[k].globalOrder] = static_cast<uint8_t>(1 + k % 5);
    RgbFrame a, b;
    renderTrainFrame(tf, a, cfg(64));
    memset(&b, 0xFF, sizeof b);
    renderTrainFrame(tf, b, cfg(64));
    CHECK(memcmp(&a, &b, sizeof a) == 0);
  }

  printf("Phase 4H RGB renderer host tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
