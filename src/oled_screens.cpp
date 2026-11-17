// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "oled_screens.h"
#include "personalization.h"
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
bool upgradeActive(const UpgradeView &v,uint32_t nowMs) {
  if(v.phase==UpgradePhase::None)return false;
  const uint32_t duration=v.phase==UpgradePhase::Complete?8000:v.phase==UpgradePhase::Failed?12000:0;
  return !duration || static_cast<uint32_t>(nowMs-v.shownAtMs)<duration;
}
void formatUpgrade(const UpgradeView &v,ScreenText &out) {
  out={};TextWriter write(out);
  write(0,v.phase==UpgradePhase::Starting?"Update starting":v.phase==UpgradePhase::Complete?"Update complete":v.phase==UpgradePhase::Failed?"Update failed":"Firmware update");
  write(1,"Version %.13s",v.version[0]?v.version:"pending");
  if(v.phase==UpgradePhase::Complete){write(2,"Startup checks passed");write(3,"Returning to display");return;}
  if(v.phase==UpgradePhase::Failed){write(2,"See web update page");write(3,"Please try again");return;}
  switch(v.phase){
    case UpgradePhase::Starting:write(2,"Starting shortly");break;
    case UpgradePhase::Downloading:write(2,"Downloading: %u%%",unsigned(v.percent>99?99:v.percent));break;
    case UpgradePhase::Verifying:write(2,"Verifying update");break;
    case UpgradePhase::Ready:write(2,"Ready to restart");break;
    case UpgradePhase::Restarting:write(2,"Restarting...");break;
    case UpgradePhase::Checking:write(2,"Checking startup");break;
    default:break;
  }
  write(3,"Do not unplug power");
  write(4,v.phase==UpgradePhase::Ready?"Approve on web page":"Please wait");
}
const char *screenName(Screen screen) {
  switch (screen) {
    case Screen::Activity: return "Train activity";
    case Screen::Connection: return "Connection";
    case Screen::Device: return "Device";
    case Screen::Lines: return "Subway lines";
    case Screen::Dedication: return "Dedication";
    case Screen::Clock: return "Time & Date";
    case Screen::Weather: return "Brooklyn weather";
    default: return "Main";
  }
}
void formatLineTest(const LineTestView &test,ScreenText &out) {
  out={};const TextWriter write(out);
  const char *colors[]={"RED","GREEN","BLUE"};
  write(0,"Testing line %s",semantic::routeName(test.route));
  write(1,"%s %s",test.individual?"LED walk":"Whole line",colors[test.color<3?test.color:0]);
  if(test.individual) write(2,"Light %u / %u",unsigned(test.current),unsigned(test.total));
  else write(2,"All %u map lights",unsigned(test.total));
  write(3,"Line %u of %u",unsigned(test.route),unsigned(semantic::kRouteCount));
  write(4,"POWER: stop test");
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
void formatRouteFocus(semantic::Route route,const ScreenData &data,ScreenText &out) {
  out={};const TextWriter write(out);
  const uint8_t line=static_cast<uint8_t>(route);
  if(!line || line>semantic::kRouteCount) return;
  const auto &counts=data.lines[line];
  write(0,"Line %s only",semantic::routeName(route));
  if(data.hasUpdate) write(1,"Trains mapped: %s%lu",counts.atLeast?">=":"",static_cast<unsigned long>(counts.trains>99999?99999:counts.trains));
  else write(1,"Waiting for trains");
  write(2,"Map lights: %u%s",unsigned(counts.lights),data.lightsEnabled?"":" off");
  write(3,"Turn knob: pick line");write(4,"MODE: all saved lines");
}
namespace {
void scrollText(const char *text, unsigned limit, uint32_t elapsedMs, char (&out)[22]) {
  unsigned length=0;
  if(text) while(length<limit && text[length]) ++length;
  unsigned offset=0;
  if(length>21) {
    const unsigned distance=length-21, travel=distance*1000, period=4000+2*travel;
    const unsigned phase=elapsedMs%period;
    if(phase<2000) offset=0;
    else if(phase<2000+travel) offset=(phase-2000)/1000;
    else if(phase<4000+travel) offset=distance;
    else offset=distance-(phase-4000-travel)/1000;
  }
  unsigned count=0;
  for(;count<21 && offset+count<length;++count) {
    const unsigned char c=text[offset+count];out[count]=(c>=32 && c<=126)?static_cast<char>(c):'?';
  }
  out[count]=0;
}
}
void scrollName(const char *name,uint32_t elapsedMs,char (&out)[22]) {scrollText(name,32,elapsedMs,out);}
void scrollDedicationNames(uint32_t elapsedMs,char (&out)[22]) {const auto p=personal::snapshot();scrollText(p.names,sizeof(p.names)-1,elapsedMs,out);}
void formatScreen(Screen screen, const ScreenData &data, uint32_t nowMs, ScreenText &out, const NavigationView &view) {
  out = {};
  const TextWriter write(out);
  const uint32_t elapsed = static_cast<uint32_t>(nowMs-data.sampledAtMs)/1000;
  switch (screen) {
    case Screen::Activity:
      write(0, data.lightsEnabled ? "Map lights       2/8" : "Map lights OFF   2/8");
      write(1, "Approaching: %u", unsigned(data.indications.approaching));
      write(2, "Stopped: %u", unsigned(data.indications.stopped));
      write(3, "Leaving: %u", unsigned(data.indications.leaving));
      write(4, "Uncertain: %u", unsigned(data.indications.uncertain));
      break;
    case Screen::Connection: {
      write(0, "Connection       3/8");
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
      write(0, "Device           4/8");
      write(1, "Firmware %.12s", data.firmware[0] ? data.firmware : "unknown");
      if (data.wifiConnected && data.ip[0]) write(2, "IP %.15s", data.ip);
      else write(2, "IP: not connected");
      {
        const uint64_t uptime=data.uptimeSeconds>UINT64_MAX-elapsed?UINT64_MAX:data.uptimeSeconds+elapsed;
        const uint64_t days=uptime/86400;
        if(days>9999) write(3,"Uptime >9999 days");
        else write(3,"Uptime %lud %02u:%02u:%02u",static_cast<unsigned long>(days),
            unsigned((uptime/3600)%24),unsigned((uptime/60)%60),unsigned(uptime%60));
      }
      write(4,"MAC %.17s",data.mac[0]?data.mac:"unavailable");
      break;
    case Screen::Lines: {
      const uint8_t line=view.line>=1 && view.line<=semantic::kRouteCount?view.line:1;
      const auto &counts=data.lines[line];
      write(0,"Line %s %s 5/8",semantic::routeName(static_cast<semantic::Route>(line)),view.automatic?"Auto":"Paused");
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
    case Screen::Dedication: {
      const auto p=personal::snapshot();
      write(0,"%s",p.greeting);
      write(1,"%s",p.shortName);
      write(2,"%s",p.from);
      personal::dateText(p,out.lines[3]);
      scrollDedicationNames(static_cast<uint32_t>(nowMs-view.changedAtMs),out.lines[4]);
      break;}
    case Screen::Weather: {
      write(0,"Brooklyn weather 7/8");
      const uint32_t now=data.unixTime>UINT32_MAX-elapsed?UINT32_MAX:data.unixTime+elapsed;
      const auto &conditions=data.weather;
      const bool known=now>=1700000000 && conditions.sourceUnix<=now;
      const uint32_t age=known?now-conditions.sourceUnix:UINT32_MAX;
      if(!conditions.available || age>10800) {
        write(1,conditions.attempted?"Weather unavailable":"Waiting for weather");
        write(2,conditions.attempted?"Will retry later":"Needs Wi-Fi and time");
      } else {
        const int value=conditions.fahrenheitTenths;
        write(1,"%s%d.%d \xB0" "F",value<0?"-":"",(value<0?-value:value)/10,(value<0?-value:value)%10);
        write(2,"%s",weather::description(conditions.code));
        write(3,"%s %lum old",age>3600?"Old data:":conditions.lastFailed?"Delayed:":"Data:",static_cast<unsigned long>(age/60));
      }
      break;
    }
    default: break; // Main keeps the existing title / trains / large clock layout.
  }
}
}
