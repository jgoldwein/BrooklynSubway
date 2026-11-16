// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "led_types.h"

namespace brooklyn::hardware {
enum class Sound : uint8_t {None,Startup,System,Network,Feed,Saved,Upgrade,Setup,Knob,Brightness,Diagnostic,Extra,Lost,Restored};
struct SoundStatus {Sound sound=Sound::None;uint8_t detail=0;uint32_t atMs=0;uint32_t sequence=0;};
SoundStatus lastSound();
inline const char *soundMeaning(Sound sound) {
  switch(sound) {
    case Sound::Startup:return "Startup tune";
    case Sound::System:return "System ready: one low beep";
    case Sound::Network:return "Wi-Fi ready: two middle beeps";
    case Sound::Feed:return "MTA feed ready: three high beeps";
    case Sound::Saved:return "Setting saved: two rising notes";
    case Sound::Upgrade:return "Update starting: two spaced descending notes";
    case Sound::Setup:return "Wi-Fi setup ready: one confirmation beep";
    case Sound::Knob:return "Knob turn: short tick";
    case Sound::Brightness:return "Brightness feedback";
    case Sound::Diagnostic:return "Channel test: beep count identifies channel";
    case Sound::Extra:return "Secret extra sound";
    case Sound::Lost:return "Service lost: three descending notes";
    case Sound::Restored:return "Service restored: three rising notes";
    default:return "No sound recorded this boot";
  }
}

// Initializes only Phase 1 hardware. Returns false if I2C initialization fails.
bool beginSafe();
// Two brief U23 pulses and a short passive-buzzer chime after local checks.
// Holding DIM at boot mutes the chime. Never indicates feed health.
// Leaves the Avi-location output LOW for its normal runtime use.
void indicateStartup(bool checksPassed);
// Manual diagnostic cue: one to eight beeps; POWER aborts; leaves GPIO21 LOW.
bool signalDiagnosticChannel(led::PhysicalChannel channel);
// U23/GPIO14: dim PWM feed-health indicator, gated by the selected POWER mode.
// The MTA startup readiness pattern serves this indicator too; no duplicate cue.
void setFeedIndicator(bool freshComplete);
void setFeedIndicatorAllowed(bool allowed,uint8_t brightnessCap=8);
void signalBrightness(uint8_t value, uint8_t maximum);
// One 120 ms confirmation after the setup AP successfully starts; quiet-aware.
void signalSetupReady();
// Two gentle descending notes on the shared buzzer timer; quiet-aware.
void signalUpgradeStarting();
// Bounded, nonblocking knob feedback; serviced by the live input task.
void requestKnobClick();
// Successful persistence only; coalesced, nonblocking two-note confirmation.
void requestSettingsSaved();
void serviceKnobClick(uint32_t nowMs, bool allowed);
void setQuiet(bool quiet,uint8_t mapBrightness=255);
// Manual easter-egg tone; zero stops. Shared timer/quiet policy, no delay.
void setEasterEggTone(uint16_t frequency);
void enableStatusReadyCues();
void observeStatusGreen(uint8_t bits);
// Input-task-owned, optional 60-second loss/restoration alerts. No bus access.
void observeService(bool available,bool operating,bool enabled);

}  // namespace brooklyn::hardware
