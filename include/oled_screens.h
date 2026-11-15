// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "indication_types.h"
#include "semantic_api.h"
#include "train_frame.h"
#include <stdint.h>
namespace brooklyn::oled {
enum class Screen : uint8_t { Main, Activity, Connection, Device, Lines, Dedication };
constexpr uint8_t kScreenCount = static_cast<uint8_t>(Screen::Dedication)+1;
constexpr Screen nextScreen(Screen screen) {
  return static_cast<Screen>((static_cast<uint8_t>(screen) + 1u) % kScreenCount);
}
enum class Action : uint8_t { NextScreen, OpenLines, TurnPrevious, TurnNext, Select, ToggleAuto };
struct NavigationView {
  Screen screen = Screen::Main;
  uint8_t line = 1;
  bool automatic = false;
  bool lineBrowsing = false;
  uint32_t changedAtMs = 0;
};
class Navigation {
 public:
  bool input(Action action, uint32_t nowMs);
  bool update(uint32_t nowMs, bool active = true);
  void setReturnSeconds(uint16_t seconds, uint32_t nowMs);
  NavigationView view() const { return view_; }
 private:
  NavigationView view_{};
  uint32_t lastInputAtMs_ = 0, autoAtMs_ = 0;
  uint16_t returnSeconds_ = 60;
};
struct LineData { uint32_t trains = 0; uint16_t lights = 0; bool atLeast = false; };
enum class FeedHealth : uint8_t { Waiting, Fresh, Partial, Held, Stale, NoData };
struct ScreenData {
  display::IndicationCounts indications{};
  uint32_t sampledAtMs = 0, updateAgeSeconds = 0;
  uint64_t uptimeSeconds = 0;
  int16_t rssi = 0;
  uint8_t feedsSucceeded = 0, feedsTotal = 0;
  bool wifiConnected = false, hasUpdate = false, lightsEnabled = false;
  bool wifiConnecting = false;
  FeedHealth feedHealth = FeedHealth::Waiting;
  char ip[16]{};
  char firmware[16]{};
  char ssid[33]{};
  uint32_t routeMask = semantic::kAllRoutesVisibleMask;
  LineData lines[semantic::kRouteCount+1]{};
};
struct ScreenText { char lines[5][22]{}; };
// Five 6x12-font lines, each <=21 characters (126 pixels).
void formatScreen(Screen screen, const ScreenData &data, uint32_t nowMs, ScreenText &out,
                  const NavigationView &view = {});
void scrollName(const char *name, uint32_t elapsedMs, char (&out)[22]);
void setLineCounts(ScreenData &data, const semantic::TrainFrame &frame,
                   const uint16_t (&lights)[semantic::kRouteCount+1]);
const char *screenName(Screen screen);
}
