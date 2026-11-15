// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "oled_screens.h"
#include <stdio.h>
namespace brooklyn::oled {
namespace {
class TextWriter {
 public:
  explicit TextWriter(ScreenText &out) : out_(out) {}
  void operator()(unsigned line, const char *text) const {
    snprintf(out_.lines[line], sizeof(out_.lines[line]), "%s", text);
  }
  template<typename... Args>
  void operator()(unsigned line, const char *format, Args... args) const {
    snprintf(out_.lines[line], sizeof(out_.lines[line]), format, args...);
  }
 private:
  ScreenText &out_;
};
}
const char *screenName(Screen screen) {
  switch (screen) {
    case Screen::Activity: return "Train activity";
    case Screen::Connection: return "Connection";
    case Screen::Device: return "Device";
    default: return "Main";
  }
}
void formatScreen(Screen screen, const ScreenData &data, uint32_t nowMs, ScreenText &out) {
  out = {};
  const TextWriter write(out);
  const uint32_t elapsed = static_cast<uint32_t>(nowMs-data.sampledAtMs)/1000;
  switch (screen) {
    case Screen::Activity:
      write(0, data.lightsEnabled ? "Map lights       2/4" : "Map lights OFF   2/4");
      write(1, "Approaching: %u", unsigned(data.indications.approaching));
      write(2, "Stopped: %u", unsigned(data.indications.stopped));
      write(3, "Leaving: %u", unsigned(data.indications.leaving));
      write(4, "Uncertain: %u", unsigned(data.indications.uncertain));
      break;
    case Screen::Connection: {
      write(0, "Connection       3/4");
      if (data.wifiConnected) write(1, "Wi-Fi: %d dBm", int(data.rssi));
      else write(1, "Wi-Fi: not connected");
      const char *health = "Waiting";
      switch (data.feedHealth) {
        case FeedHealth::Fresh: health="Fresh"; break;
        case FeedHealth::Partial: health="Partial"; break;
        case FeedHealth::Held: health="Held"; break;
        case FeedHealth::Stale: health="Stale"; break;
        case FeedHealth::NoData: health="No data"; break;
        default: break;
      }
      if (data.feedsTotal) write(2, "Feeds %u/%u %s", unsigned(data.feedsSucceeded), unsigned(data.feedsTotal), health);
      else write(2, "Feeds: waiting");
      if (data.hasUpdate) {
        const uint32_t age = data.updateAgeSeconds > UINT32_MAX-elapsed ? UINT32_MAX : data.updateAgeSeconds+elapsed;
        if (age < 3600) write(3, "Updated %lu sec ago", static_cast<unsigned long>(age));
        else write(3, "Updated >1 hour ago");
      } else write(3, "No usable update yet");
      write(4, "MODE: next screen");
      break;
    }
    case Screen::Device:
      write(0, "Device           4/4");
      write(1, "Firmware %.12s", data.firmware[0] ? data.firmware : "unknown");
      if (data.wifiConnected && data.ip[0]) write(2, "IP %.15s", data.ip);
      else write(2, "IP: not connected");
      write(3, "Up %lu min", static_cast<unsigned long>(nowMs/60000));
      write(4, "MODE: main screen");
      break;
    default: break; // Main keeps the existing title / trains / large clock layout.
  }
}
}
