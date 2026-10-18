// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "semantic_api.h"

#include <stdint.h>

namespace brooklyn::config {

constexpr uint8_t kDefaultBrightness = 64;
constexpr uint32_t kDefaultRouteVisibilityMask =
    semantic::kAllRoutesVisibleMask;
constexpr const char *kNvsNamespace = "bkmap_cfg";
constexpr const char *kBrightnessKey = "brightness";
constexpr const char *kRouteVisibilityMaskKey = "route_mask";

struct Store {
  void *context;
  bool (*loadBrightness)(void *context, uint32_t &value);
  bool (*saveBrightness)(void *context, uint8_t value);
  bool (*loadRouteVisibilityMask)(void *context, uint32_t &value);
  bool (*saveRouteVisibilityMask)(void *context, uint32_t value);
};

class DeviceConfiguration {
 public:
  explicit DeviceConfiguration(Store store = {}) : store_(store) {}

  bool load();
  bool setBrightness(uint8_t brightness);
  bool setRouteVisibilityMask(uint32_t mask);
  uint8_t brightness() const { return brightness_; }
  uint32_t routeVisibilityMask() const { return routeVisibilityMask_; }

 private:
  Store store_;
  uint8_t brightness_ = kDefaultBrightness;
  uint32_t routeVisibilityMask_ = kDefaultRouteVisibilityMask;
};

Store nvsStore();

}  // namespace brooklyn::config
