// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "indication_types.h"
#include <stdint.h>
namespace brooklyn::oled {
enum class Screen : uint8_t { Main, Activity, Connection, Device };
constexpr Screen nextScreen(Screen screen) {
  return static_cast<Screen>((static_cast<uint8_t>(screen) + 1u) % 4u);
}
enum class FeedHealth : uint8_t { Waiting, Fresh, Partial, Held, Stale, NoData };
struct ScreenData {
  display::IndicationCounts indications{};
  uint32_t sampledAtMs = 0, updateAgeSeconds = 0;
  int16_t rssi = 0;
  uint8_t feedsSucceeded = 0, feedsTotal = 0;
  bool wifiConnected = false, hasUpdate = false, lightsEnabled = false;
  FeedHealth feedHealth = FeedHealth::Waiting;
  char ip[16]{};
  char firmware[16]{};
};
struct ScreenText { char lines[5][22]{}; };
// Five 6x12-font lines, each <=21 characters (126 pixels).
void formatScreen(Screen screen, const ScreenData &data, uint32_t nowMs, ScreenText &out);
const char *screenName(Screen screen);
}
