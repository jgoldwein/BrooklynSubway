#pragma once
#include "test_button_policy.h"
#include <stdint.h>
namespace brooklyn::controls {
struct BrightnessRequest { uint8_t mode = 0; bool brighter = false; };
// Sampled by the live worker even during blocking network requests.
class BrightnessButtons {
 public:
  bool update(bool bright, bool dim, uint8_t mode, uint32_t now, BrightnessRequest &out) {
    if (!initialized_ || mode != mode_) {
      initialized_ = true; mode_ = mode;
      bright_.reset(bright, now); dim_.reset(dim, now);
      chord_ = bright && dim;
      return false;
    }
    if (bright && dim) chord_ = true;
    const bool up = bright_.update(bright, now), down = dim_.update(dim, now);
    if (chord_) {
      if (!bright && !dim) {
        bright_.reset(false, now); dim_.reset(false, now); chord_ = false;
      }
      return false;
    }
    if ((mode != 1 && mode != 2) || up == down) return false;
    out.mode = mode; out.brighter = up; return true;
  }
 private:
  TestButton bright_, dim_;
  bool initialized_ = false, chord_ = false;
  uint8_t mode_ = 0;
};
inline uint8_t steppedBrightness(uint8_t current, bool oled, bool brighter) {
  if (!oled && current == 0) return brighter ? 1 : 0; // Preserve a web-selected zero when dimming.
  const int maximum = oled ? 255 : 16, minimum = oled ? 0 : 1;
  int value = current > maximum ? maximum : current < minimum ? minimum : current;
  value += brighter ? (oled ? 16 : 1) : -(oled ? 16 : 1);
  return static_cast<uint8_t>(value < minimum ? minimum : value > maximum ? maximum : value);
}
}
