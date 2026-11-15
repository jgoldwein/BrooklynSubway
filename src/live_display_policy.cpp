#include "live_display_policy.h"
#include "led_power.h"
#include <string.h>

namespace brooklyn::display::live {

void Controller::publish(const Snapshot &snapshot) {
  if ((memcmp(snapshot_.frame.occupancy, snapshot.frame.occupancy,
              sizeof(snapshot.frame.occupancy)) != 0 ||
       memcmp(snapshot_.frame.approachingOnly, snapshot.frame.approachingOnly,
              sizeof(snapshot.frame.approachingOnly)) != 0 ||
       memcmp(snapshot_.frame.uncertainOnly, snapshot.frame.uncertainOnly, sizeof(snapshot.frame.uncertainOnly)) != 0 ||
       memcmp(snapshot_.frame.blueSecondsRemaining, snapshot.frame.blueSecondsRemaining, sizeof(snapshot.frame.blueSecondsRemaining)) != 0 ||
       memcmp(snapshot_.frame.lastReportedFadeStartUnix, snapshot.frame.lastReportedFadeStartUnix, sizeof(snapshot.frame.lastReportedFadeStartUnix)) != 0 ||
       memcmp(snapshot_.frame.timing, snapshot.frame.timing, sizeof(snapshot.frame.timing)) != 0) &&
      stats_.mapChanges != UINT32_MAX) ++stats_.mapChanges;
  snapshot_ = snapshot;
  hasSnapshot_ = true;
  everHadFrame_ = everHadFrame_ || snapshot.hasFrame;
  // With no web enable switch, a later physical POWER press may retry once
  // healthy data returns. This never auto-restarts a fault-stopped session.
  if (physicalPowerControl_ && snapshot.hasFrame && snapshot.status.systemHealthy &&
      !snapshot.status.provisioning &&
      (snapshot.status.displayState == State::Fresh || snapshot.status.displayState == State::DegradedPartial))
    powerCanResume_ = true;
  dirty_ = true;
}

OutputStats Controller::stats() const {
  auto result = stats_;
  result.powerCanResume = powerCanResume_ && !enabled_ && !automaticStartPending_;
  result.automaticStartPending = automaticStartPending_ || (quietSuppressed_ && quietRestore_);
  result.powerMode = powerMode_;
  if (!enabled_) { result.blinkingLeds = 0; result.visibleMapLeds = 0; result.darkOccupiedMapLeds = 0; for(auto &count:result.routeLights) count=0; }
  return result;
}

bool Controller::powerPress(uint32_t nowMs, bool pressed) {
  if (!buttonInitialized_) {
    buttonInitialized_ = true;
    buttonReading_ = buttonStable_ = pressed;
    buttonArmed_ = !pressed; // Ignore a button held through reset until released.
    return false;
  }
  if (pressed != buttonReading_) {
    buttonReading_ = pressed;
    buttonChangedAtMs_ = nowMs;
  }
  if (buttonStable_ != buttonReading_ && uint32_t(nowMs - buttonChangedAtMs_) >= 30) {
    buttonStable_ = buttonReading_;
    if (buttonStable_) { pressedAtMs_ = nowMs; longHandled_ = false; }
    else {
      const bool shortPress = buttonArmed_ && !longHandled_;
      buttonArmed_ = true;
      return shortPress;
    }
  }
  if (buttonArmed_ && buttonStable_ && pressed && !longHandled_ &&
      uint32_t(nowMs - pressedAtMs_) >= 3000) {
    longHandled_ = true;
    rebootRequested_ = rebootPending_ = true;
  }
  return false;
}

bool Controller::safe(uint32_t nowMs) const {
  return snapshot_.hasFrame && snapshot_.status.systemHealthy &&
      !snapshot_.status.provisioning &&
      snapshot_.status.displayState != State::WaitingForData &&
      snapshot_.status.displayState != State::NoData &&
      static_cast<uint32_t>(nowMs - snapshot_.frameAtMs) < kHardStaleLimitMs;
}

bool Controller::enable(uint32_t nowMs) {
  // An old held frame cannot be used to start a new session.
  if (quietBrightness_==0 || !ledsRequested() || !safe(nowMs) || static_cast<uint32_t>(nowMs - snapshot_.frameAtMs) > kPartialGraceMs ||
      (snapshot_.status.displayState != State::Fresh &&
                       snapshot_.status.displayState != State::DegradedPartial)) return false;
  enabled_ = true;
  automaticStartPending_ = false;
  powerCanResume_ = true;
  dirty_ = true;
  return true;
}

uint32_t Controller::indicationClock(uint32_t nowMs) const {
  return snapshot_.clockUnix ? snapshot_.clockUnix+uint32_t(nowMs-snapshot_.clockAtMs)/1000 : 0;
}

void Controller::endIndication(uint16_t led,uint32_t nowMs,IndicationEnd reason,Indication next) {
  const uint8_t bit=uint8_t{1}<<(led%8);
  if (!(stoppedActive_[led/8]&bit)) return;
  stoppedActive_[led/8]&=~bit;
  if(indicationSequence_==UINT32_MAX) return; // saturated sequence never wraps into duplicate IDs
  auto &event=indicationEvents_[indicationHead_];
  event={++indicationSequence_,stoppedAtMs_[led],nowMs,indicationClock(nowMs),led,reason,next};
  indicationHead_=(indicationHead_+1)%kIndicationHistoryCapacity;
  if(indicationSize_<kIndicationHistoryCapacity)++indicationSize_;
}

void Controller::stopIndications(uint32_t nowMs,IndicationEnd reason) {
  for(uint16_t i=0;i<semantic::kFrameLedCount;++i)endIndication(i,nowMs,reason,Indication::Off);
  stats_.indications={};
  lastObservationAtMs_=nowMs;
}

void Controller::observeIndications(const semantic::RenderTrace &trace,const led::DisplayState *frame,uint32_t nowMs) {
  stats_.indications={};
  for(uint16_t i=0;i<semantic::kFrameLedCount;++i) {
    if(!semantic::semanticForLed(i))continue;
    const auto &color=frame[i].color;
    const bool visible=color.red||color.green||color.blue;
    const auto kind=visible?trace.kind[i]:Indication::Off;
    const auto reason=!visible && trace.kind[i]!=Indication::Off?IndicationEnd::CurrentLimited:trace.reason[i];
    switch(kind) {
      case Indication::Approaching:++stats_.indications.approaching;break;
      case Indication::Stopped:++stats_.indications.stopped;break;
      case Indication::Leaving:++stats_.indications.leaving;break;
      case Indication::Uncertain:++stats_.indications.uncertain;break;
      default:break;
    }
    const uint8_t bit=uint8_t{1}<<(i%8);
    if(kind==Indication::Stopped) {
      if(!(stoppedActive_[i/8]&bit)) { stoppedAtMs_[i]=nowMs;stoppedActive_[i/8]|=bit; }
    } else endIndication(i,nowMs,reason,kind);
  }
  lastObservationAtMs_=nowMs;
}

void Controller::readIndications(IndicationReport &out,uint32_t nowMs,uint32_t after,bool latest) const {
  out={};out.available=true;out.outputEnabled=enabled_&&armed_;out.counts=stats_.indications;
  out.nowMs=nowMs;out.observedAtMs=lastObservationAtMs_;out.observedUnix=indicationClock(nowMs);
  out.newestSequence=indicationSequence_;
  const uint8_t oldest=(indicationHead_+kIndicationHistoryCapacity-indicationSize_)%kIndicationHistoryCapacity;
  if(indicationSize_)out.oldestSequence=indicationEvents_[oldest].sequence;
  out.gap=!latest && out.oldestSequence && after<out.oldestSequence-1;
  const uint8_t begin=latest && indicationSize_>kIndicationHistoryPage?indicationSize_-kIndicationHistoryPage:0;
  out.nextAfter=after;
  for(uint8_t j=begin;j<indicationSize_;++j) {
    const auto &e=indicationEvents_[(oldest+j)%kIndicationHistoryCapacity];
    if(!latest && e.sequence<=after)continue;
    if(out.eventCount==kIndicationHistoryPage) {out.more=true;break;}
    out.events[out.eventCount++]=e;out.nextAfter=e.sequence;
  }
  for(uint16_t i=0;i<semantic::kFrameLedCount;++i) {
    if(!(stoppedActive_[i/8]&(uint8_t{1}<<(i%8))))continue;
    ++out.activeTotal;
    if(out.activeCount<kActiveIndicationPage)
      out.active[out.activeCount++]={stoppedAtMs_[i],uint32_t(nowMs-stoppedAtMs_[i]),i};
  }
}

void Controller::disable(const Operations &operations, uint32_t nowMs, IndicationEnd reason) {
  stopIndications(nowMs==UINT32_MAX?lastObservationAtMs_:nowMs,reason);
  enabled_ = false;
  automaticStartPending_ = false;
  powerCanResume_ = false;
  quietSuppressed_=quietRestore_=false;
  if (armed_ && operations.stop) operations.stop(operations.context);
  armed_ = false;
}

void Controller::service(uint32_t nowMs, bool powerPressed, bool recovery,
                         const Operations &operations) {
  const bool releasedEvent = powerPress(nowMs, powerPressed);
  if (releasedEvent && !rebootPending_) {
    powerMode_ = (powerMode_ + 1) % 4;
    if (ledsRequested()) automaticStartPending_ = true;
    else disable(operations,nowMs,IndicationEnd::PowerButton);
  }
  if (rebootPending_ || !ledsRequested()) {
    disable(operations,nowMs,IndicationEnd::PowerButton);
    return;
  }
  if(quietBrightness_==0) {
    if(recovery || (hasSnapshot_ && !snapshot_.status.systemHealthy)) {
      disable(operations,nowMs,recovery?IndicationEnd::Recovery:IndicationEnd::OutputFailure);return;
    }
    if(!quietSuppressed_) {quietRestore_=enabled_ || automaticStartPending_;quietSuppressed_=true;}
    quietRestore_=quietRestore_ || automaticStartPending_;
    stopIndications(nowMs,IndicationEnd::QuietHours);
    enabled_=automaticStartPending_=powerCanResume_=false;
    if(armed_ && operations.stop) operations.stop(operations.context);
    armed_=false;return;
  }
  if(quietSuppressed_) {
    quietSuppressed_=false;
    if(quietRestore_ && ledsRequested()) automaticStartPending_=true;
    quietRestore_=false;
  }
  if (!hasSnapshot_) return;
  // Keep startup LOW while waiting for the first usable cycle. POWER can
  // cancel automatic startup during this wait without enabling anything.
  if (!everHadFrame_ && !snapshot_.hasFrame && snapshot_.status.systemHealthy && !recovery)
    return;
  if (automaticStartPending_ && !powerPressed && !recovery) (void)enable(nowMs);
  if (recovery || !safe(nowMs) || !operations.arm ||
      !operations.transmit || !operations.stop) {
    disable(operations,nowMs,recovery?IndicationEnd::Recovery:IndicationEnd::NoUsableData);
    return;
  }
  if (!enabled_) return;
  if (!armed_) {
    if (!operations.arm(operations.context)) {
      operations.stop(operations.context);
      stopIndications(nowMs,IndicationEnd::OutputFailure);
      enabled_ = false;
      powerCanResume_ = false;
      return;
    }
    armed_ = true;
    dirty_ = true;
  }
  const uint32_t tick = nowMs / 40;
  if (!dirty_ && tick == animationTick_) return;
  semantic::RenderConfig config{};
  config.brightness = snapshot_.brightness < led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP
      ? snapshot_.brightness : led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP;
  if(config.brightness>quietBrightness_) config.brightness=quietBrightness_;
  config.colors=snapshot_.colors;
  config.unknownTimingMode = semantic::unknownTimingMode(snapshot_.unknownTiming);
  config.showUnknownTiming = config.unknownTimingMode != semantic::UnknownTimingMode::Hidden;
  config.lastReportedFadeSeconds = semantic::lastReportedFadeSeconds(snapshot_.unknownTiming);
  config.unknownTimingBrightness = snapshot_.unknownTiming & 255;
  config.routeVisibilityMask = snapshot_.routeMask;
  config.animateTraffic = true;
  config.animationTimeMs = nowMs;
  config.nowUnix = snapshot_.clockUnix ? snapshot_.clockUnix +
      static_cast<uint32_t>(nowMs - snapshot_.clockAtMs) / 1000 : 0;
  semantic::RgbFrame rgb{};
  semantic::RenderTrace trace{};
  const auto result = semantic::renderTrainFrame(snapshot_.frame, rgb, config, &trace);
  auto status = snapshot_.status;
  status.oledScreen = oledScreen_;
  status.brightness = config.brightness;
  if (result.rejectedLeds != 0 || !applyStatusOverlay(rgb, status)) {
    disable(operations,nowMs,IndicationEnd::OutputFailure);
    return;
  }
  led::DisplayState source[semantic::kFrameLedCount]{};
  led::DisplayState limited[semantic::kFrameLedCount]{};
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) source[i].color = rgb.pixels[i];
  led::power::FramePowerReport report{};
  // RGB is already capped; apply only the existing total-current estimator.
  led::power::limitFrameToBudget(source, semantic::kFrameLedCount, 255, 255,
                                limited, report);
  if (!operations.transmit(operations.context, limited, semantic::kFrameLedCount)) {
    disable(operations,nowMs,IndicationEnd::OutputFailure);
    return;
  }
  observeIndications(trace,limited,nowMs);
  stats_.visibleMapLeds = stats_.darkOccupiedMapLeds = 0;
  for(auto &count:stats_.routeLights) count=0;
  for (uint16_t i=0;i<semantic::kFrameLedCount;++i) {
    if (!semantic::semanticForLed(i)) continue; // Exclude the four status LEDs.
    const auto &color=limited[i].color;
    if (color.red || color.green || color.blue) {
      ++stats_.visibleMapLeds;
      const auto route=static_cast<uint8_t>(semantic::routeForLed(i));
      if(route && route<=semantic::kRouteCount) ++stats_.routeLights[route];
    }
    else if (snapshot_.frame.occupancy[i]) ++stats_.darkOccupiedMapLeds;
  }
  animationTick_ = tick;
  dirty_ = false;
  if (stats_.framesWritten != UINT32_MAX) ++stats_.framesWritten;
  stats_.lastWriteAtMs = nowMs;
  stats_.blinkingLeds = 0; // Compatibility statistic: traffic no longer blinks.
}

}  // namespace brooklyn::display::live
