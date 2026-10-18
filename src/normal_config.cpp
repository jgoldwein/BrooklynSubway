// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "normal_config.h"

namespace brooklyn::config {

bool DeviceConfiguration::load() {
  brightness_ = kDefaultBrightness;
  routeVisibilityMask_ = kDefaultRouteVisibilityMask;
  bool loaded = false;

  uint32_t stored = 0;
  if (store_.loadBrightness != nullptr &&
      store_.loadBrightness(store_.context, stored) && stored <= 255) {
    brightness_ = static_cast<uint8_t>(stored);
    loaded = true;
  }
  if (store_.loadRouteVisibilityMask != nullptr &&
      store_.loadRouteVisibilityMask(store_.context, stored)) {
    routeVisibilityMask_ = stored & semantic::kAllRoutesVisibleMask;
    loaded = true;
  }
  return loaded;
}

bool DeviceConfiguration::setBrightness(uint8_t brightness) {
  if (store_.saveBrightness == nullptr ||
      !store_.saveBrightness(store_.context, brightness)) {
    return false;
  }
  brightness_ = brightness;
  return true;
}

bool DeviceConfiguration::setRouteVisibilityMask(uint32_t mask) {
  mask &= semantic::kAllRoutesVisibleMask;
  if (store_.saveRouteVisibilityMask == nullptr ||
      !store_.saveRouteVisibilityMask(store_.context, mask)) {
    return false;
  }
  routeVisibilityMask_ = mask;
  return true;
}

}  // namespace brooklyn::config
