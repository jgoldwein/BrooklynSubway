// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <stdint.h>
namespace brooklyn::semantic {
enum class UnknownTimingMode : uint8_t { Hidden=0, Orange=1, LastReported=2 };
constexpr uint16_t kDefaultLastReportedFadeSeconds=60;
constexpr uint16_t kMaxLastReportedFadeSeconds=300;
inline UnknownTimingMode unknownTimingMode(uint32_t setting) {
  return static_cast<UnknownTimingMode>((setting >> 8) & 3);
}
inline uint16_t lastReportedFadeSeconds(uint32_t setting) {
  return setting >> 10 ? static_cast<uint16_t>(setting >> 10) : kDefaultLastReportedFadeSeconds;
}
inline bool validUnknownTiming(uint32_t setting) {
  return ((setting >> 8) & 3) <= 2 && (setting >> 10) <= kMaxLastReportedFadeSeconds;
}
inline const char *unknownTimingModeName(UnknownTimingMode mode) {
  return mode==UnknownTimingMode::LastReported ? "last-reported" : mode==UnknownTimingMode::Orange ? "orange" : "hidden";
}
}
