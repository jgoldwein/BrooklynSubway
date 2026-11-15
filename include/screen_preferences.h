// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <string.h>
namespace brooklyn::config {
constexpr uint64_t kStartupDedicationBit=2ull;
constexpr uint64_t kDefaultScreenPreferences=kStartupDedicationBit|(1320u<<2)|(420u<<13)|(1u<<24);
struct ScreenPreferences {
  bool quietEnabled=false, startupDedication=true;
  uint16_t startMinute=1320, endMinute=420;
  uint8_t quietContrast=1, quietMapBrightness=0; // 0=off, 1..16=dim ceiling, 255=unchanged.
};
inline ScreenPreferences unpackScreenPreferences(uint64_t word) {
  ScreenPreferences p;p.quietEnabled=word&1;p.startupDedication=word&2;
  p.startMinute=(word>>2)&2047;p.endMinute=(word>>13)&2047;p.quietContrast=(word>>24)&255;p.quietMapBrightness=(word>>32)&255;return p;
}
inline uint64_t packScreenPreferences(const ScreenPreferences &p) {
  return uint64_t(p.quietEnabled)|(uint64_t(p.startupDedication)<<1)|(uint64_t(p.startMinute)<<2)|
      (uint64_t(p.endMinute)<<13)|(uint64_t(p.quietContrast)<<24)|(uint64_t(p.quietMapBrightness)<<32);
}
inline bool validScreenPreferences(uint64_t word) {
  const auto p=unpackScreenPreferences(word);
  return !(word>>40) && p.startMinute<1440 && p.endMinute<1440 && p.startMinute!=p.endMinute && (p.quietMapBrightness<=16 || p.quietMapBrightness==255);
}
inline bool quietActive(uint64_t word,bool clockKnown,uint16_t minute) {
  const auto p=unpackScreenPreferences(word);
  if(!p.quietEnabled) return false;
  if(!clockKnown) return true; // Respect enabled quiet hours before time sync.
  return p.startMinute<p.endMinute ? minute>=p.startMinute && minute<p.endMinute : minute>=p.startMinute || minute<p.endMinute;
}
inline bool parseClockMinute(const char *text,uint16_t &minute) {
  if(!text || strlen(text)!=5 || text[2]!=':') return false;
  for(unsigned i=0;i<5;++i) if(i!=2 && (text[i]<'0'||text[i]>'9')) return false;
  const unsigned hour=(text[0]-'0')*10+text[1]-'0', mins=(text[3]-'0')*10+text[4]-'0';
  if(hour>23 || mins>59) return false;minute=hour*60+mins;return true;
}
}
