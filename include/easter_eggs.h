// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "oled_screens.h"
#include "led_types.h"
namespace brooklyn::eggs {
enum class Kind : uint8_t { Celebration, Express, Credits, Morse, Anniversary, Rangers };
constexpr uint8_t kCount=6;
enum class Visual : uint8_t { None, Line, Chaser, All };
struct Sample {
  oled::ScreenText text{};
  Visual visual=Visual::None;
  semantic::Route route=semantic::Route::None;
  uint16_t position=0, frequency=0;
  led::Rgb color{};
};
uint32_t duration(Kind kind);
bool sample(Kind kind,uint32_t elapsed,Sample &out);
// TEST+ROUTE: two-second simultaneous hold, release both to launch.
// Suppresses both ordinary actions through their debounced releases.
class Chord {
 public:
  bool update(bool test,bool route,uint32_t now);
  bool suppressed() const {return blocked_;}
 private:
  bool initialized_=false, ready_=false, blocked_=false, held_=false;
  bool armed_=false, releasing_=false;
  uint32_t since_=0, releasedAt_=0;
};
}
