#pragma once

#include "operational_display.h"
#include "led_types.h"

namespace brooklyn::display::live {

struct Snapshot {
  semantic::TrainFrame frame{};
  StatusIndicators status{};
  uint32_t frameAtMs = 0;
  uint32_t routeMask = semantic::kAllRoutesVisibleMask;
  uint8_t brightness = semantic::kDefaultRenderBrightness;
  bool hasFrame = false;
};

struct Operations {
  void *context;
  bool (*arm)(void *);
  bool (*transmit)(void *, const led::DisplayState *, uint16_t);
  void (*stop)(void *);
};

// Called under the runtime mutex. Hardware operations have one owner at a time.
class Controller {
 public:
  void publish(const Snapshot &snapshot) { snapshot_ = snapshot; dirty_ = true; }
  bool enable(uint32_t nowMs);
  void disable(const Operations &operations);
  void service(uint32_t nowMs, bool powerPressed, bool recovery,
               const Operations &operations);
  bool enabled() const { return enabled_; }
 private:
  bool safe(uint32_t nowMs) const;
  Snapshot snapshot_{};
  bool enabled_ = false;
  bool armed_ = false;
  bool dirty_ = true;
  bool phase_ = true;
};

}  // namespace brooklyn::display::live
