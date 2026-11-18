// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "station_alerts.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <Preferences.h>
namespace brooklyn { namespace alerts {
namespace {
constexpr uint32_t kMagic=0xa17e0100u;
}
bool loadPreferences() {
  configure(Preferences{});::Preferences p;uint32_t word=kMagic|25u;
  if(p.begin("bkmap_cfg",true)) {word=p.getUInt("station_alerts",word);p.end();}
  Preferences next;next.stations=word&7;next.enabled=word&8;next.lineWide=word&16;
  const bool ok=(word&~31u)==kMagic && validPreferences(next);
  if(ok)configure(next);
  Serial.printf("Station alerts: %s; enabled %s; station mask %u; line-wide %s; silent\n",ok?"preferences loaded":"invalid preferences; defaults used",state().preferences.enabled?"yes":"no",unsigned(state().preferences.stations),state().preferences.lineWide?"yes":"no");return ok;
}
bool savePreferences(const Preferences &next) {
  if(!validPreferences(next))return false;::Preferences p;
  if(!p.begin("bkmap_cfg",false))return false;
  const bool ok=p.putUInt("station_alerts",kMagic|settingsWord(next))==sizeof(uint32_t);p.end();
  if(ok) {configure(next);Serial.printf("Station alerts: preferences saved; station mask %u; enabled %s\n",unsigned(next.stations),next.enabled?"yes":"no");}return ok;
}
}}
#endif
