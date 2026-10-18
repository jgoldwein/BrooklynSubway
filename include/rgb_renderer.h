// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_types.h"
#include "semantic_api.h"
#include "train_frame.h"

#include <stdint.h>

// Phase 4H: deterministic, heap-free conversion of a logical TrainFrame into an
// in-memory RGB frame.
//
//   TrainFrame -> RgbFrame (this file) -> FUTURE temporal renderer -> FUTURE LED driver
//
// This is memory only. Nothing here touches GPIO, LED_ENABLE, U23 or any LED
// driver, and nothing calls a physical setter/show function. The brightness is
// a logical rendering scale; it has not been validated as a board power/current
// limit.
//
// Policy: an occupied map LED shows its semantic route colour at the configured
// global brightness. Occupancy count never changes the colour (no additive
// brightness, mixing, blinking, fading or animation). Empty map LEDs and the
// four status LEDs are black.
namespace brooklyn::semantic {

using Rgb = brooklyn::led::Rgb;  // existing generic POD; not hardware-coupled
static_assert(sizeof(Rgb) == 3, "Rgb must be three bytes");

constexpr uint8_t kDefaultRenderBrightness = 64;  // ~25% of nominal; logical only

struct RgbFrame {
  Rgb pixels[kFrameLedCount];  // indexed by canonical led::map global index
};

struct RenderConfig {
  uint8_t brightness = kDefaultRenderBrightness;  // 0..255 global scale
  uint32_t routeVisibilityMask = kAllRoutesVisibleMask;
};

struct RenderResult {
  uint16_t renderedLeds;     // map LEDs given a colour (valid occupied map LEDs)
  uint16_t stationLeds;      // of renderedLeds, STATION role
  uint16_t segmentLeds;      // of renderedLeds, SEGMENT role
  uint16_t rejectedLeds;     // occupied entries refused (status LED, bad identity, Route::None)
  uint16_t occupiedStatusLeds;  // subset of rejectedLeds: occupied status LEDs
};

// Single source of truth for nominal MTA route-family colours (pre-brightness).
// Route::None (and any unknown value) returns black.
Rgb nominalRouteColor(Route route);

// (component * brightness + 127) / 255 per component; 0 -> black, 255 -> unchanged.
Rgb scaleBrightness(Rgb color, uint8_t brightness);

// Clears `output`, then paints every valid occupied, visible map LED. Hidden
// routes remain in `input`; visibility affects only this final RGB rendering.
// Pure function of `input` and `config`; `input` is never modified.
RenderResult renderTrainFrame(const TrainFrame &input, RgbFrame &output,
                              const RenderConfig &config = RenderConfig{});

}  // namespace brooklyn::semantic
