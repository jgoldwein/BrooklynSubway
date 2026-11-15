#pragma once

#include "operational_display.h"
#include "led_types.h"

namespace brooklyn::display::live {

struct Snapshot {
  semantic::TrainFrame frame{};
  StatusIndicators status{};
  uint32_t frameAtMs = 0;
  uint32_t routeMask = semantic::kAllRoutesVisibleMask;
  semantic::TrainColors colors=semantic::kDefaultTrainColors;
  uint32_t unknownTiming = 511;
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
  uint16_t visibleMapLeds = 0;
  uint16_t darkOccupiedMapLeds = 0;
  IndicationCounts indications{};
  bool powerCanResume = false;
  bool automaticStartPending = false;
  uint8_t powerMode = 0;
};

// Called under the runtime mutex. Hardware operations have one owner at a time.
class Controller {
 public:
  explicit Controller(bool automaticStart = false) {
    automaticStartPending_ = automaticStart;
    powerCanResume_ = automaticStart;
    physicalPowerControl_ = automaticStart;
  }
  void publish(const Snapshot &snapshot);
  bool enable(uint32_t nowMs);
  void disable(const Operations &operations, uint32_t nowMs=UINT32_MAX,
               IndicationEnd reason=IndicationEnd::OutputDisabled);
  void readIndications(IndicationReport &report, uint32_t nowMs, uint32_t after=0, bool latest=true) const;
  void service(uint32_t nowMs, bool powerPressed, bool recovery,
               const Operations &operations);
  bool enabled() const { return enabled_; }
  OutputStats stats() const;
  void requestAutomaticStart() { automaticStartPending_ = true; powerCanResume_ = true; }
  void allowPowerResume() { automaticStartPending_ = false; powerCanResume_ = true; }
  bool screenEnabled() const { return powerMode_ < 2; }
  bool ledsRequested() const { return (powerMode_ & 1) == 0 && !rebootPending_; }
  bool consumeRebootRequest() { const bool r = rebootRequested_; rebootRequested_ = false; return r; }
 private:
  bool safe(uint32_t nowMs) const;
  bool powerPress(uint32_t nowMs, bool pressed);
  void observeIndications(const semantic::RenderTrace &trace, const led::DisplayState *frame, uint32_t nowMs);
  void endIndication(uint16_t led, uint32_t nowMs, IndicationEnd reason, Indication next);
  void stopIndications(uint32_t nowMs, IndicationEnd reason);
  uint32_t indicationClock(uint32_t nowMs) const;
  uint32_t stoppedAtMs_[semantic::kFrameLedCount]{};
  uint8_t stoppedActive_[(semantic::kFrameLedCount+7)/8]{};
  IndicationEvent indicationEvents_[kIndicationHistoryCapacity]{};
  uint32_t indicationSequence_=0, lastObservationAtMs_=0;
  uint8_t indicationHead_=0, indicationSize_=0;
  Snapshot snapshot_{};
  bool enabled_ = false;
  bool armed_ = false;
  bool dirty_ = true;
  uint32_t animationTick_ = UINT32_MAX;
  bool powerCanResume_ = false;
  bool automaticStartPending_ = false;
  bool hasSnapshot_ = false;
  bool everHadFrame_ = false;
  bool physicalPowerControl_ = false;
  uint8_t powerMode_ = 0;
  bool rebootRequested_ = false, rebootPending_ = false;
  bool buttonArmed_ = false, longHandled_ = false;
  uint32_t pressedAtMs_ = 0;
  bool buttonInitialized_ = false;
  bool buttonReading_ = false;
  bool buttonStable_ = false;
  uint32_t buttonChangedAtMs_ = 0;
  OutputStats stats_{};
};

}  // namespace brooklyn::display::live
