// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

namespace brooklyn::config {

constexpr uint8_t kDefaultBrightness = 64;
constexpr const char *kNvsNamespace = "bkmap_cfg";
constexpr const char *kBrightnessKey = "brightness";

struct Store {
  void *context;
  bool (*loadBrightness)(void *context, uint32_t &value);
  bool (*saveBrightness)(void *context, uint8_t value);
};

class DeviceConfiguration {
 public:
  explicit DeviceConfiguration(Store store = {}) : store_(store) {}

  bool load();
  bool setBrightness(uint8_t brightness);
  uint8_t brightness() const { return brightness_; }

 private:
  Store store_;
  uint8_t brightness_ = kDefaultBrightness;
};

Store nvsStore();

}  // namespace brooklyn::config
