#pragma once
#include <stdint.h>

namespace brooklyn::network {
// Recover a stale association without erasing credentials or rebooting the MCU.
// Require repeated complete transport failures and rate-limit reassociation.
class RecoveryPolicy {
 public:
  void cycle(bool downloadedAny) {
    if (downloadedAny) failures_ = 0;
    else if (failures_ < 3) ++failures_;
  }
  bool due(uint32_t nowMs) const {
    return failures_ >= 3 && (!attempted_ || uint32_t(nowMs-lastAttempt_) >= 300000u);
  }
  void attempted(uint32_t nowMs) {
    failures_=0;attempted_=true;lastAttempt_=nowMs;
  }
 private:
  uint8_t failures_=0;
  bool attempted_=false;
  uint32_t lastAttempt_=0;
};
}
