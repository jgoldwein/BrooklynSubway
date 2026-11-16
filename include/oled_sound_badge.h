#pragma once
#include "hardware.h"
namespace brooklyn::oled {
// Eight monochrome XBM rows (least-significant bit is the left pixel).
constexpr uint8_t kSoundCheck[]={0x00,0x80,0x40,0x22,0x14,0x08,0x00,0x00};
constexpr uint8_t kSoundWifi[]={0x3c,0x42,0x81,0x3c,0x42,0x18,0x00,0x18};
constexpr uint8_t kSoundTrain[]={0x3c,0x42,0x7e,0x5a,0x42,0x7e,0x24,0x42};
constexpr uint8_t kSoundDown[]={0x18,0x18,0x18,0x18,0x99,0x5a,0x3c,0x18};
constexpr uint8_t kSoundUp[]={0x18,0x3c,0x5a,0x99,0x18,0x18,0x18,0x18};
struct SoundBadge {
  const uint8_t *pixels=nullptr;
  const char *label=nullptr;
};
inline SoundBadge soundBadge(const hardware::SoundStatus &sound,uint32_t nowMs) {
  SoundBadge badge;
  if(!sound.sequence || uint32_t(nowMs-sound.atMs)>=4000u)return badge;
  switch(sound.sound) {
    case hardware::Sound::System:badge.pixels=kSoundCheck;badge.label="System ready";break;
    case hardware::Sound::Network:badge.pixels=kSoundWifi;badge.label="Connected";break;
    case hardware::Sound::Feed:badge.pixels=kSoundTrain;badge.label="Feed ready";break;
    case hardware::Sound::Saved:badge.pixels=kSoundCheck;badge.label="Saved";break;
    case hardware::Sound::Setup:badge.pixels=kSoundWifi;badge.label="Setup ready";break;
    case hardware::Sound::Lost:badge.pixels=kSoundDown;badge.label="Service lost";break;
    case hardware::Sound::Restored:badge.pixels=kSoundUp;badge.label="Restored";break;
    case hardware::Sound::Upgrade:badge.pixels=kSoundDown;badge.label="Updating";break;
    default:break; // Other cues have their own full-screen feedback.
  }
  return badge;
}
}
