#pragma once
#include <stdint.h>
namespace brooklyn::audio {
enum class ServiceEvent : uint8_t { None, Lost, Restored };
class ServicePolicy {
 public:
  ServiceEvent observe(uint32_t nowMs,bool available,bool enabled,bool muted) {
    if(!enabled || muted){reset();return ServiceEvent::None;}
    if(available) {
      const bool restore=announced_;ready_=true;timing_=announced_=false;
      return restore?ServiceEvent::Restored:ServiceEvent::None;
    }
    if(!ready_)return ServiceEvent::None; // Never alarm while starting for the first time.
    if(!timing_){timing_=true;lostAt_=nowMs;}
    return !announced_ && uint32_t(nowMs-lostAt_)>=60000u?ServiceEvent::Lost:ServiceEvent::None;
  }
  void lossAnnounced(){announced_=true;}
  void reset(){ready_=timing_=announced_=false;}
 private:
  bool ready_=false,timing_=false,announced_=false;
  uint32_t lostAt_=0;
};
}
