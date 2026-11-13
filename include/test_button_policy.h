#pragma once
#include <stdint.h>

namespace brooklyn::controls {
// One request on debounced release. Boot-held buttons must be released first.
class TestButton {
 public:
  void reset(bool pressed, uint32_t nowMs) {
    initialized_ = true; reading_ = stable_ = pressed;
    armed_ = false; changedAtMs_ = nowMs;
  }
  bool update(bool pressed, uint32_t nowMs) {
    if (!initialized_) { reset(pressed, nowMs); return false; }
    if (pressed != reading_) { reading_ = pressed; changedAtMs_ = nowMs; }
    if (stable_ == reading_ || static_cast<uint32_t>(nowMs - changedAtMs_) < 30) return false;
    stable_ = reading_;
    if (stable_) { armed_ = true; return false; }
    const bool result = armed_;
    armed_ = false;
    return result;
  }
 private:
  bool initialized_ = false, reading_ = false, stable_ = false, armed_ = false;
  uint32_t changedAtMs_ = 0;
};
}
