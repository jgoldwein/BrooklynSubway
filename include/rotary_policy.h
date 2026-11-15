// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace brooklyn::controls {
// Two-transition policy for the reported half-step detents. Supports either
// resting Gray-code phase; bounce reversals cancel.
class RotaryEncoder {
 public:
  void reset(bool a, bool b) { previous_=(uint8_t(a)<<1)|uint8_t(b);accumulator_=0;initialized_=true; }
  int8_t update(bool a, bool b) {
    if (!initialized_) { reset(a,b);return 0; }
    const uint8_t next=(uint8_t(a)<<1)|uint8_t(b);
    if (next==previous_) return 0;
    if ((next^previous_)==3) {previous_=next;accumulator_=0;return 0;}
    static const int8_t transitions[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    accumulator_+=transitions[(previous_<<2)|next];previous_=next;
    if (accumulator_>=2) {accumulator_=0;return 1;}
    if (accumulator_<=-2) {accumulator_=0;return -1;}
    return 0;
  }
 private:
  bool initialized_=false;
  uint8_t previous_=0;
  int8_t accumulator_=0;
};
}
