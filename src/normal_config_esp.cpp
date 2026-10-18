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

bool loadDnsOverride(void *, bool &enabled, uint32_t &ipv4) {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, true)) return false;
  const bool present = prefs.isKey(kDnsOverrideKey);
  uint8_t stored[5] = {};
  const size_t read = present ? prefs.getBytes(kDnsOverrideKey, stored,
                                                sizeof(stored))
                              : 0;
  prefs.end();
  if (!present || read != sizeof(stored) || stored[0] > 1) return false;
  enabled = stored[0] != 0;
  ipv4 = (static_cast<uint32_t>(stored[1]) << 24) |
         (static_cast<uint32_t>(stored[2]) << 16) |
         (static_cast<uint32_t>(stored[3]) << 8) | stored[4];
  return true;
}

bool saveDnsOverride(void *, bool enabled, uint32_t ipv4) {
  if ((enabled && !validDnsIpv4(ipv4)) ||
      (ipv4 != 0 && !validDnsIpv4(ipv4))) {
    return false;
  }
  const uint8_t stored[5] = {
      static_cast<uint8_t>(enabled ? 1 : 0),
      static_cast<uint8_t>(ipv4 >> 24),
      static_cast<uint8_t>(ipv4 >> 16),
      static_cast<uint8_t>(ipv4 >> 8),
      static_cast<uint8_t>(ipv4)};
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) return false;
  const size_t written = prefs.putBytes(kDnsOverrideKey, stored,
                                        sizeof(stored));
  prefs.end();
  return written == sizeof(stored);
}

}  // namespace

Store nvsStore() {
  return {nullptr, loadBrightness, saveBrightness, loadRouteVisibilityMask,
          saveRouteVisibilityMask, loadDnsOverride, saveDnsOverride};
}

}  // namespace brooklyn::config

#else

namespace brooklyn::config {

namespace {

bool loadBrightness(void *, uint32_t &) { return false; }
bool saveBrightness(void *, uint8_t) { return false; }
bool loadRouteVisibilityMask(void *, uint32_t &) { return false; }
bool saveRouteVisibilityMask(void *, uint32_t) { return false; }
bool loadDnsOverride(void *, bool &, uint32_t &) { return false; }
bool saveDnsOverride(void *, bool, uint32_t) { return false; }

}  // namespace

Store nvsStore() {
  return {nullptr, loadBrightness, saveBrightness, loadRouteVisibilityMask,
          saveRouteVisibilityMask, loadDnsOverride, saveDnsOverride};
}

}  // namespace brooklyn::config

#endif
