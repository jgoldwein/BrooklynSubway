// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "normal_config.h"

namespace brooklyn::config {

bool DeviceConfiguration::load() {
  brightness_ = kDefaultBrightness;
  if (store_.loadBrightness == nullptr) {
    return false;
  }

  uint32_t stored = 0;
  if (!store_.loadBrightness(store_.context, stored) || stored > 255) {
    return false;
  }
  brightness_ = static_cast<uint8_t>(stored);
  return true;
}

bool DeviceConfiguration::setBrightness(uint8_t brightness) {
  if (store_.saveBrightness == nullptr ||
      !store_.saveBrightness(store_.context, brightness)) {
    return false;
  }
  brightness_ = brightness;
  return true;
}

}  // namespace brooklyn::config
