// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "oled_screens.h"
#include <stdio.h>
#include <string.h>
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
    case Screen::Lines: return "Subway lines";
    default: return "Main";
  }
}
void setLineCounts(ScreenData &data, const semantic::TrainFrame &frame,
                   const uint16_t (&lights)[semantic::kRouteCount+1]) {
  for(uint8_t line=0;line<=semantic::kRouteCount;++line) {data.lines[line]={};data.lines[line].lights=lights[line];}
  for(uint16_t i=0;i<semantic::kFrameLedCount;++i) {
    const uint8_t line=static_cast<uint8_t>(semantic::routeForLed(i));
    if(!line || line>semantic::kRouteCount) continue;
    data.lines[line].trains+=frame.occupancy[i];
    if(frame.occupancy[i]==semantic::kOccupancyMax) data.lines[line].atLeast=true;
  }
}
void scrollName(const char *name, uint32_t elapsedMs, char (&out)[22]) {
  char safe[33]{};unsigned length=0;
  if(name) for(;length<32 && name[length];++length) safe[length]=(name[length]>=32 && name[length]<=126)?name[length]:'?';
  unsigned offset=0;
  if(length>21) {
    const unsigned distance=length-21, travel=distance*1000, period=4000+2*travel;
    const unsigned phase=elapsedMs%period;
    if(phase<2000) offset=0;
    else if(phase<2000+travel) offset=(phase-2000)/1000;
    else if(phase<4000+travel) offset=distance;
    else offset=distance-(phase-4000-travel)/1000;
  }
  snprintf(out,sizeof(out),"%.21s",safe+offset);
}
void formatScreen(Screen screen, const ScreenData &data, uint32_t nowMs, ScreenText &out, const NavigationView &view) {
  out = {};
  const TextWriter write(out);
  const uint32_t elapsed = static_cast<uint32_t>(nowMs-data.sampledAtMs)/1000;
  switch (screen) {
    case Screen::Activity:
      write(0, data.lightsEnabled ? "Map lights       2/5" : "Map lights OFF   2/5");
      write(1, "Approaching: %u", unsigned(data.indications.approaching));
      write(2, "Stopped: %u", unsigned(data.indications.stopped));
      write(3, "Leaving: %u", unsigned(data.indications.leaving));
      write(4, "Uncertain: %u", unsigned(data.indications.uncertain));
      break;
    case Screen::Connection: {
      write(0, "Connection       3/5");
      if(data.wifiConnected) scrollName(data.ssid[0]?data.ssid:"Wi-Fi connected",static_cast<uint32_t>(nowMs-view.changedAtMs),out.lines[1]);
      else write(1,data.wifiConnecting?"Connecting to Wi-Fi":"Wi-Fi not connected");
      if(data.wifiConnected) write(2,"%ddBm Feeds %u/%u",int(data.rssi),unsigned(data.feedsSucceeded),unsigned(data.feedsTotal));
      else write(2,"Check Wi-Fi / signal");
      const uint32_t age=data.updateAgeSeconds>UINT32_MAX-elapsed?UINT32_MAX:data.updateAgeSeconds+elapsed;
      if(!data.hasUpdate) write(3,"Waiting for trains");
      else if(age>90 || data.feedHealth==FeedHealth::Stale) write(3,"Data is old: %lus",static_cast<unsigned long>(age>99999?99999:age));
      else if(data.feedHealth==FeedHealth::Held || data.feedHealth==FeedHealth::Partial) write(3,"Some feeds missing");
      else write(3,"Updated %lu sec ago",static_cast<unsigned long>(age));
      write(4,"Turn knob: screen");
      break;
    }
    case Screen::Device:
      write(0, "Device           4/5");
      write(1, "Firmware %.12s", data.firmware[0] ? data.firmware : "unknown");
      if (data.wifiConnected && data.ip[0]) write(2, "IP %.15s", data.ip);
      else write(2, "IP: not connected");
      write(3, "Up %lu min", static_cast<unsigned long>(nowMs/60000));
      write(4, "Turn knob: screen");
      break;
    case Screen::Lines: {
      const uint8_t line=view.line>=1 && view.line<=semantic::kRouteCount?view.line:1;
      const auto &counts=data.lines[line];
      write(0,"Line %s %s 5/5",semantic::routeName(static_cast<semantic::Route>(line)),view.automatic?"Auto":"Paused");
      write(1,"Trains mapped: %s%lu",counts.atLeast?">=":"",static_cast<unsigned long>(counts.trains>99999?99999:counts.trains));
      const bool visible=semantic::routeVisible(data.routeMask,static_cast<semantic::Route>(line));
      write(2,"Map lights: %u%s",unsigned(counts.lights),!visible?" hidden":!data.lightsEnabled?" off":"");
      const uint32_t age=data.updateAgeSeconds>UINT32_MAX-elapsed?UINT32_MAX:data.updateAgeSeconds+elapsed;
      if(!data.hasUpdate) write(3,"Waiting for trains");
      else if(age>90 || data.feedHealth==FeedHealth::Stale) write(3,"Data is old: %lus",static_cast<unsigned long>(age>99999?99999:age));
      else if(data.feedHealth==FeedHealth::Held || data.feedHealth==FeedHealth::Partial) write(3,"Using older map data");
      else write(3,"Map data: %lus old",static_cast<unsigned long>(age));
      write(4,view.lineBrowsing?"Turn:line Press:back":"Turn:screen Press:go");
      break;
    }
    default: break; // Main keeps the existing title / trains / large clock layout.
  }
}
}
