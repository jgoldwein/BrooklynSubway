// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "normal_config.h"

#include "network_config.h"

#if BROOKLYN_NETWORK_ENABLED

#include <Preferences.h>

namespace brooklyn::config {

namespace {

bool loadBrightness(void *, uint32_t &value) {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, true)) return false;
  value = prefs.getUInt(kBrightnessKey, UINT32_MAX);
  prefs.end();
  return value != UINT32_MAX;
}

bool saveBrightness(void *, uint8_t value) {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) return false;
  const size_t written = prefs.putUInt(kBrightnessKey, value);
  prefs.end();
  return written == sizeof(uint32_t);
}

bool loadRouteVisibilityMask(void *, uint32_t &value) {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, true)) return false;
  const bool present = prefs.isKey(kRouteVisibilityMaskKey);
  if (present) {
    value = prefs.getUInt(kRouteVisibilityMaskKey, UINT32_MAX);
  }
  prefs.end();
  return present;
}

bool saveRouteVisibilityMask(void *, uint32_t value) {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) return false;
  const size_t written = prefs.putUInt(kRouteVisibilityMaskKey, value);
  prefs.end();
  return written == sizeof(uint32_t);
}

}  // namespace

Store nvsStore() {
  return {nullptr, loadBrightness, saveBrightness, loadRouteVisibilityMask,
          saveRouteVisibilityMask};
}

}  // namespace brooklyn::config

#else

namespace brooklyn::config {

namespace {

bool loadBrightness(void *, uint32_t &) { return false; }
bool saveBrightness(void *, uint8_t) { return false; }
bool loadRouteVisibilityMask(void *, uint32_t &) { return false; }
bool saveRouteVisibilityMask(void *, uint32_t) { return false; }

}  // namespace

Store nvsStore() {
  return {nullptr, loadBrightness, saveBrightness, loadRouteVisibilityMask,
          saveRouteVisibilityMask};
}

}  // namespace brooklyn::config

#endif
