// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "personalization.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <Preferences.h>
namespace brooklyn { namespace personal {
namespace {
struct Record {uint32_t version;Settings settings;uint32_t checksum;};
uint32_t checksum(const Record &r) {uint32_t h=2166136261u;const auto *p=reinterpret_cast<const uint8_t *>(&r);for(size_t i=0;i<offsetof(Record,checksum);++i)h=(h^p[i])*16777619u;return h;}
}
bool load() {
  Settings defaults;apply(defaults);Preferences p;if(!p.begin("bkmap_cfg",true)) {Serial.println("Personalization: original defaults (no saved settings)");return true;}
  const size_t n=p.getBytesLength("personal_v1");Record r{};bool ok=!n;
  if(n==sizeof(r))ok=p.getBytes("personal_v1",&r,sizeof(r))==sizeof(r) && r.version==1 && checksum(r)==r.checksum && valid(r.settings);
  p.end();if(n && ok)apply(r.settings);
  Serial.printf("Personalization: %s\n",!n?"original defaults":ok?"saved settings loaded":"invalid saved record; original defaults used");return ok;
}
bool save(const Settings &s) {
  if(!valid(s))return false;
  Record r{};r.version=1;r.settings=s;r.checksum=checksum(r);Preferences p;
  if(!p.begin("bkmap_cfg",false))return false;
  const bool ok=p.putBytes("personal_v1",&r,sizeof(r))==sizeof(r);p.end();
  if(ok) {apply(s);Serial.println("Personalization: saved; OLED and web text updated");}return ok;
}
}}
#endif
