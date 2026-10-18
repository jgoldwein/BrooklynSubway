// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "normal_config.h"

#include <stdio.h>

namespace brooklyn::config {

bool DeviceConfiguration::load() {
  brightness_ = kDefaultBrightness;
  routeVisibilityMask_ = kDefaultRouteVisibilityMask;
  dnsOverrideEnabled_ = false;
  dnsOverrideIpv4_ = 0;
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
  bool dnsEnabled = false;
  uint32_t dnsIpv4 = 0;
  if (store_.loadDnsOverride != nullptr &&
      store_.loadDnsOverride(store_.context, dnsEnabled, dnsIpv4)) {
    if (dnsIpv4 == 0 || validDnsIpv4(dnsIpv4)) {
      dnsOverrideEnabled_ = dnsEnabled && dnsIpv4 != 0;
      dnsOverrideIpv4_ = dnsIpv4;
    }
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

bool DeviceConfiguration::setDnsOverride(bool enabled, uint32_t ipv4) {
  if ((enabled && !validDnsIpv4(ipv4)) ||
      (ipv4 != 0 && !validDnsIpv4(ipv4)) ||
      store_.saveDnsOverride == nullptr ||
      !store_.saveDnsOverride(store_.context, enabled, ipv4)) {
    return false;
  }
  dnsOverrideEnabled_ = enabled;
  dnsOverrideIpv4_ = ipv4;
  return true;
}

bool validDnsIpv4(uint32_t ipv4) {
  const uint8_t first = static_cast<uint8_t>(ipv4 >> 24);
  return first != 0 && first != 127 && first < 224;
}

bool parseDnsIpv4(const char *text, uint32_t &ipv4) {
  if (text == nullptr || *text == '\0') return false;
  uint8_t octets[4] = {};
  size_t octetIndex = 0;
  unsigned value = 0;
  size_t digits = 0;
  for (const char *cursor = text;; ++cursor) {
    const char character = *cursor;
    if (character >= '0' && character <= '9') {
      if (++digits > 3) return false;
      value = value * 10 + static_cast<unsigned>(character - '0');
      if (value > 255) return false;
      continue;
    }
    if (character != '.' && character != '\0') return false;
    if (digits == 0 || octetIndex >= 4) return false;
    octets[octetIndex++] = static_cast<uint8_t>(value);
    value = 0;
    digits = 0;
    if (character == '\0') break;
  }
  if (octetIndex != 4) return false;
  const uint32_t parsed = (static_cast<uint32_t>(octets[0]) << 24) |
                          (static_cast<uint32_t>(octets[1]) << 16) |
                          (static_cast<uint32_t>(octets[2]) << 8) |
                          octets[3];
  if (!validDnsIpv4(parsed)) return false;
  ipv4 = parsed;
  return true;
}

size_t formatDnsIpv4(uint32_t ipv4, char *out, size_t capacity) {
  if (out == nullptr || capacity == 0) return 0;
  const int written = snprintf(
      out, capacity, "%u.%u.%u.%u", static_cast<unsigned>(ipv4 >> 24),
      static_cast<unsigned>((ipv4 >> 16) & 0xff),
      static_cast<unsigned>((ipv4 >> 8) & 0xff),
      static_cast<unsigned>(ipv4 & 0xff));
  if (written < 0 || static_cast<size_t>(written) >= capacity) {
    out[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(written);
}

}  // namespace brooklyn::config
