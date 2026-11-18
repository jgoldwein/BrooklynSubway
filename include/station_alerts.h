// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace brooklyn { namespace alerts {
constexpr const char *kUrl="https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/camsys%2Fsubway-alerts";
constexpr size_t kMaxBody=1024u*1024u;
constexpr uint8_t kMaxItems=6;
constexpr uint32_t kPollMs=300000, kStaleSeconds=900, kUpcomingSeconds=172800;
struct Preferences {uint8_t stations=1;bool enabled=true,lineWide=true;};
inline uint32_t settingsWord(const Preferences &p) {return p.stations|(p.enabled?8u:0u)|(p.lineWide?16u:0u);}
inline bool validPreferences(const Preferences &p) {return !(p.stations&~7u) && (!p.enabled || p.stations);}
struct Item {
  char id[96]{},headline[161]{},description[513]{};
  uint32_t start=0,end=0;
  uint8_t stations=0;
  bool lineWide=false,shortened=false;
};
struct Snapshot {
  Item items[kMaxItems]{};
  uint32_t sourceUnix=0,checkedUnix=0;
  uint16_t matched=0;
  uint8_t count=0;
  bool available=false,overflow=false;
};
struct State {
  Preferences preferences{};
  Snapshot snapshot{};
  uint32_t attemptedAtMs=0;
  bool attempted=false,lastFailed=false;
};
// State is owned by loop(): worker decodes only into its separate candidate.
inline State &state() {static State s;return s;}
inline const char *stationName(uint8_t mask) {return mask==1?"York St (F)":mask==2?"High St (A/C)":mask==4?"Jay St-MetroTech":"My stations";}
inline void configure(const Preferences &p) {
  if(!validPreferences(p))return;
  auto &s=state();s.preferences=p;
  // Avoid a multi-kilobyte temporary on the HTTP/loop stack.
  memset(&s.snapshot,0,sizeof(s.snapshot));s.attemptedAtMs=0;s.attempted=false;s.lastFailed=false;
}
inline bool current(const Item &i,uint32_t now) {return i.start<=now && (!i.end || now<i.end);}
inline bool upcoming(const Item &i,uint32_t now) {return i.start>now && i.start-now<=kUpcomingSeconds;}
inline bool usable(const State &s,uint32_t now) {return s.snapshot.available && now>=s.snapshot.checkedUnix && now-s.snapshot.checkedUnix<=kStaleSeconds && !s.lastFailed;}
struct Summary {
  char station[22]{},headline[64]{};
  uint32_t checkedUnix=0;
  uint8_t nowCount=0,soonCount=0;
  bool enabled=false,available=false,attempted=false,lastFailed=false,overflow=false;
};
// A current matching notice, not an urgency classification. Fail closed on old/unknown data.
inline bool mainIndicator(const Summary &s,uint32_t now) {
  return s.enabled && s.available && s.nowCount && now>=1700000000 &&
      now>=s.checkedUnix && now-s.checkedUnix<=kStaleSeconds;
}
inline Summary summary(uint32_t now) {
  const auto &s=state();Summary v;v.enabled=s.preferences.enabled;v.available=usable(s,now);
  v.attempted=s.attempted;v.lastFailed=s.lastFailed;v.checkedUnix=s.snapshot.checkedUnix;v.overflow=s.snapshot.overflow;
  const char *name=stationName(s.preferences.stations);strncpy(v.station,name,sizeof(v.station)-1);
  for(uint8_t n=0;n<s.snapshot.count;++n) {
    const auto &i=s.snapshot.items[n];const bool active=current(i,now),soon=upcoming(i,now);
    if(active)++v.nowCount;else if(soon)++v.soonCount;
    if(!v.headline[0] && (active || soon)) {
      strncpy(v.headline,i.headline,sizeof(v.headline)-1);
      if(strlen(i.headline)>=sizeof(v.headline))memcpy(v.headline+60,"...",4);
    }
  }return v;
}
inline bool due(uint32_t nowMs,uint32_t cycleAtMs) {
  const auto &s=state();return s.preferences.enabled && uint32_t(nowMs-cycleAtMs)<1000 &&
      (!s.attempted || uint32_t(nowMs-s.attemptedAtMs)>=kPollMs);
}
bool decode(const uint8_t *data,size_t length,const Preferences &preferences,uint32_t nowUnix,Snapshot &out);
bool loadPreferences();
bool savePreferences(const Preferences &preferences);
}}
