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
  uint32_t clockUnix = 0;
  uint32_t clockAtMs = 0;
};

struct Operations {
  void *context;
  bool (*arm)(void *);
  bool (*transmit)(void *, const led::DisplayState *, uint16_t);
  void (*stop)(void *);
};

struct OutputStats {
  uint32_t framesWritten = 0;
  uint32_t lastWriteAtMs = 0;
  uint32_t mapChanges = 0;
  uint16_t blinkingLeds = 0;
  bool powerCanResume = false;
};

// Called under the runtime mutex. Hardware operations have one owner at a time.
class Controller {
 public:
  void publish(const Snapshot &snapshot);
  bool enable(uint32_t nowMs);
  void disable(const Operations &operations);
  void service(uint32_t nowMs, bool powerPressed, bool recovery,
               const Operations &operations);
  bool enabled() const { return enabled_; }
  OutputStats stats() const;
 private:
  bool safe(uint32_t nowMs) const;
  bool powerPress(uint32_t nowMs, bool pressed);
  Snapshot snapshot_{};
  bool enabled_ = false;
  bool armed_ = false;
  bool dirty_ = true;
  bool phase_ = true;
  uint32_t animationTick_ = UINT32_MAX;
  bool powerCanResume_ = false;
  bool buttonInitialized_ = false;
  bool buttonReading_ = false;
  bool buttonStable_ = false;
  uint32_t buttonChangedAtMs_ = 0;
  OutputStats stats_{};
};

}  // namespace brooklyn::display::live
