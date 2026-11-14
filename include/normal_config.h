// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "semantic_api.h"

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::config {

constexpr uint16_t kDefaultUnknownTiming = 511; // bit 8 enabled, low byte brightness ceiling
constexpr const char *kUnknownTimingKey = "unknown_timing";
constexpr uint8_t kDefaultOledContrast = 207; // U8g2 SSD1306 initialization default
constexpr const char *kOledContrastKey = "oled_contrast";
constexpr uint32_t kDefaultBlueHoldSeconds = 20;
constexpr uint32_t kMaxBlueHoldSeconds = 300;
constexpr const char *kBlueHoldKey = "blue_hold_sec";
constexpr uint8_t kDefaultBrightness = 5;
constexpr uint8_t kDefaultOledAddress = 0x38;
constexpr const char *kOledAddressKey = "oled_addr";
constexpr uint32_t kDefaultRouteVisibilityMask =
    semantic::kAllRoutesVisibleMask;
constexpr const char *kNvsNamespace = "bkmap_cfg";
constexpr const char *kBrightnessKey = "brightness";
constexpr const char *kRouteVisibilityMaskKey = "route_mask";
constexpr const char *kDnsOverrideKey = "dns_ovr";
constexpr const char *kPollIntervalKey = "poll_sec";

struct Store {
  void *context;
  bool (*loadBrightness)(void *context, uint32_t &value);
  bool (*saveBrightness)(void *context, uint8_t value);
  bool (*loadRouteVisibilityMask)(void *context, uint32_t &value);
  bool (*saveRouteVisibilityMask)(void *context, uint32_t value);
  bool (*loadDnsOverride)(void *context, bool &enabled, uint32_t &ipv4);
  bool (*saveDnsOverride)(void *context, bool enabled, uint32_t ipv4);
  bool (*loadPollSeconds)(void *context, uint32_t &value);
  bool (*savePollSeconds)(void *context, uint32_t value);
  bool (*loadOledAddress)(void *context, uint32_t &value);
  bool (*saveOledAddress)(void *context, uint32_t value);
  bool (*loadBlueHoldSeconds)(void *context, uint32_t &value);
  bool (*saveBlueHoldSeconds)(void *context, uint32_t value);
  bool (*loadOledContrast)(void *context, uint32_t &value);
  bool (*saveOledContrast)(void *context, uint32_t value);
  bool (*loadUnknownTiming)(void *context, uint32_t &value);
  bool (*saveUnknownTiming)(void *context, uint32_t value);
};

class DeviceConfiguration {
 public:
  explicit DeviceConfiguration(Store store = {}) : store_(store) {}

  bool load();
  bool setBrightness(uint8_t brightness);
  bool setUnknownTiming(uint16_t setting);
  uint16_t unknownTiming() const { return unknownTiming_; }
  bool setOledContrast(uint8_t contrast);
  uint8_t oledContrast() const { return oledContrast_; }
  bool setBlueHoldSeconds(uint32_t seconds);
  uint32_t blueHoldSeconds() const { return blueHoldSeconds_; }
  bool setPollSeconds(uint32_t seconds);
  bool setOledAddress(uint8_t address);
  uint8_t oledAddress() const { return oledAddress_; }
  uint32_t pollSeconds() const { return pollSeconds_; }
  bool setRouteVisibilityMask(uint32_t mask);
  bool setDnsOverride(bool enabled, uint32_t ipv4);
  uint8_t brightness() const { return brightness_; }
  uint32_t routeVisibilityMask() const { return routeVisibilityMask_; }
  bool dnsOverrideEnabled() const { return dnsOverrideEnabled_; }
  uint32_t dnsOverrideIpv4() const { return dnsOverrideIpv4_; }

 private:
  Store store_;
  uint16_t unknownTiming_ = kDefaultUnknownTiming;
  uint8_t oledContrast_ = kDefaultOledContrast;
  uint8_t oledAddress_ = kDefaultOledAddress;
  uint32_t blueHoldSeconds_ = kDefaultBlueHoldSeconds;
  uint32_t pollSeconds_ = 15;
  uint8_t brightness_ = kDefaultBrightness;
  uint32_t routeVisibilityMask_ = kDefaultRouteVisibilityMask;
  bool dnsOverrideEnabled_ = false;
  uint32_t dnsOverrideIpv4_ = 0;
};

bool parseDnsIpv4(const char *text, uint32_t &ipv4);
bool validOledAddress(uint32_t address);
bool parseOledAddress(const char *text, uint8_t &address);
bool parseBlueHoldSeconds(const char *text, uint32_t &seconds);
bool parsePollSeconds(const char *text, uint32_t &seconds);
bool validDnsIpv4(uint32_t ipv4);
size_t formatDnsIpv4(uint32_t ipv4, char *out, size_t capacity);

Store nvsStore();

}  // namespace brooklyn::config
