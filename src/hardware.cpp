// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "hardware.h"
#include "service_sound_policy.h"

#include "board_pins.h"
#include "controls.h"

#if defined(BROOKLYN_TARGET_PCB)
#include <Wire.h>
#include <esp32-hal-ledc.h>
#endif

namespace brooklyn::hardware {

#if defined(BROOKLYN_TARGET_PCB)
namespace {
bool startupMuted = false;
uint32_t soundSequence=0,soundCode=0,soundAtMs=0;
void recordSound(Sound sound,uint8_t detail=0) {
  __atomic_add_fetch(&soundSequence,1u,__ATOMIC_ACQ_REL);
  __atomic_store_n(&soundCode,uint32_t(sound)|(uint32_t(detail)<<8),__ATOMIC_RELAXED);
  __atomic_store_n(&soundAtMs,millis(),__ATOMIC_RELAXED);
  __atomic_add_fetch(&soundSequence,1u,__ATOMIC_RELEASE);
}
uint32_t quietWord=0;
uint32_t quietMapWord=255, feedCapWord=8;
uint32_t statusCuesEnabled=0,observedGreen=0,readyPending=0;
uint32_t readySeen=0,serviceWord=0;
uint8_t readyNotes=0;uint32_t readyToneBit=0;
audio::ServicePolicy servicePolicy;
audio::ServiceEvent servicePending=audio::ServiceEvent::None,serviceKind=audio::ServiceEvent::None;
uint8_t serviceStep=0;bool knobService=false;
uint8_t lastFeedDuty=0;
bool isQuiet() {return __atomic_load_n(&quietWord,__ATOMIC_RELAXED)!=0;}
bool feedIndicatorAttached = false;
bool feedIndicatorOn = false;
bool feedIndicatorUnavailable = false;
bool feedReadyBeepAttempted = false;
void stopKnobToneLocked(bool preserveSettings=false);
void lockIndicator();
void unlockIndicator();

void beepFeedReadyOnce() {
  if (feedReadyBeepAttempted) return;
  feedReadyBeepAttempted = true;
  if(__atomic_load_n(&statusCuesEnabled,__ATOMIC_RELAXED)) {
    Serial.println("Feed-ready buzzer: delegated to green MTA status cue");return;
  }
  if (startupMuted || isQuiet()) {
    Serial.println("Feed-ready buzzer: muted by DIM at boot or quiet hours"); return;
  }
  constexpr uint8_t kBuzzerChannel = 7; // Timer 3, independent of U23 timer 2.
  constexpr uint16_t kFrequencyHz = 2000;
  constexpr uint16_t kDurationMs = 100;
  stopKnobToneLocked();
  digitalWrite(pins::BUZZER, LOW);
  if (ledcSetup(kBuzzerChannel, kFrequencyHz, 8) == 0) {
    Serial.println("Feed-ready buzzer: PWM setup failed; GPIO21 LOW"); return;
  }
  ledcWrite(kBuzzerChannel, 0);
  ledcAttachPin(pins::BUZZER, kBuzzerChannel);
  ledcWrite(kBuzzerChannel, 128);
  delay(kDurationMs);
  ledcWrite(kBuzzerChannel, 0);
  ledcDetachPin(pins::BUZZER);
  digitalWrite(pins::BUZZER, LOW);
  Serial.println("Feed-ready buzzer: U23 ON, GPIO21 2000 Hz / 100 ms; once per boot");
}

void playStartupChime() {
  // Reserve channel 7 (LEDC timer 3) for BUZZER3. No tone task is allocated.
  constexpr uint8_t kBuzzerChannel = 7;
  constexpr uint8_t kResolutionBits = 8;
  // 50% square wave, matching the datasheet waveform, at the existing 3.3 V.
  constexpr uint8_t kStartupDuty = 128;
  struct Note {
    uint16_t frequencyHz;
    uint16_t durationMs;
    uint16_t gapAfterMs;
  };
  // Preserve the startup motif, then a subway-style descending pair.
  // Lower the ending pair one octave from 1310/1060 Hz to 655/530 Hz.
  // These are approximated reference pitches, not official MTA pitches.
  constexpr Note kNotes[] = {
      {2500, 80, 45}, {3000, 80, 45}, {3500, 100, 45}, {4000, 180, 250},
      {655, 600, 90}, {530, 750, 0},
  };
  constexpr uint8_t kNoteCount = sizeof(kNotes) / sizeof(kNotes[0]);
  digitalWrite(pins::BUZZER, LOW);
  if (startupMuted || isQuiet()) {
    Serial.println("Startup buzzer: muted by DIM at boot or quiet hours; GPIO21 LOW");
    return;
  }
  Serial.println("Startup buzzer: BUZZER3 GPIO21, four rising notes then subway-style ding-dong, 50% PWM");
  bool attached = false;
  bool complete = true;
  for (uint8_t note = 0; note < kNoteCount; ++note) {
    if (ledcSetup(kBuzzerChannel, kNotes[note].frequencyHz, kResolutionBits) == 0) {
      complete = false;
      break;
    }
    if (!attached) {
      ledcAttachPin(pins::BUZZER, kBuzzerChannel);
      attached = true;
    }
    Serial.printf("Startup buzzer: note %u, %u Hz, %u ms\n", note + 1,
                  kNotes[note].frequencyHz, kNotes[note].durationMs);
    ledcWrite(kBuzzerChannel, kStartupDuty);
    if(note==0)recordSound(Sound::Startup);
    delay(kNotes[note].durationMs);
    ledcWrite(kBuzzerChannel, 0);
    if (kNotes[note].gapAfterMs != 0) delay(kNotes[note].gapAfterMs);
  }
  if (attached) {
    ledcWrite(kBuzzerChannel, 0);
    ledcDetachPin(pins::BUZZER);
  }
  digitalWrite(pins::BUZZER, LOW);
  Serial.printf("Startup buzzer: %s; GPIO21 LOW\n",
                complete ? "sequence complete" : "PWM setup FAILED");
}
}  // namespace
#endif

bool signalDiagnosticChannel(led::PhysicalChannel channel) {
#if defined(BROOKLYN_TARGET_PCB)
  constexpr uint8_t kChannel = 7;
  constexpr uint32_t kBeepMs = 80;
  constexpr uint32_t kGapMs = 80;
  constexpr uint32_t kLeadInMs = 300;
  const uint8_t number = static_cast<uint8_t>(channel);
  lockIndicator();stopKnobToneLocked();unlockIndicator();
  digitalWrite(pins::BUZZER, LOW);
  if (number < 1 || number > 8 || digitalRead(pins::BUTTON_POWER) == LOW ||
      controls::pollWifiRecoveryDuringDiagnostic()) return false;
  if(isQuiet()) {Serial.println("Diagnostic buzzer: quiet hours; visual test continues");return true;}
  const auto wait = [](uint32_t ms) {
    constexpr uint32_t kSliceMs = 20;
    while (ms > 0) {
      if (digitalRead(pins::BUTTON_POWER) == LOW ||
          controls::pollWifiRecoveryDuringDiagnostic()) return false;
      const uint32_t slice = ms < kSliceMs ? ms : kSliceMs;
      if(isQuiet()) ledcWrite(7,0);
      delay(slice);
      ms -= slice;
    }
    return digitalRead(pins::BUTTON_POWER) != LOW &&
           !controls::pollWifiRecoveryDuringDiagnostic();
  };
  Serial.printf("Diagnostic buzzer: CH%u -> %u beep(s), GPIO21 4000 Hz\n", number, number);
  if (ledcSetup(kChannel, 4000, 8) == 0) {
    Serial.println("Diagnostic buzzer: PWM setup FAILED; GPIO21 LOW");
    return false;
  }
  ledcAttachPin(pins::BUZZER, kChannel);
  bool ok = true;
  for (uint8_t beep = 0; beep < number && ok; ++beep) {
    if(beep==0)recordSound(Sound::Diagnostic,number);
    ledcWrite(kChannel, isQuiet()?0:128);
    ok = wait(kBeepMs);
    ledcWrite(kChannel, 0);
    if (ok && beep + 1 < number) ok = wait(kGapMs);
  }
  if (ok) ok = wait(kLeadInMs);
  ledcWrite(kChannel, 0);
  ledcDetachPin(pins::BUZZER);
  digitalWrite(pins::BUZZER, LOW);
  Serial.printf("Diagnostic buzzer: %s; GPIO21 LOW\n", ok ? "cue complete" : "aborted by POWER/recovery");
  return ok;
#else
  (void)channel;
  return false;
#endif
}

void indicateStartup(bool checksPassed) {
#if defined(BROOKLYN_TARGET_PCB)
  constexpr uint8_t kPulseCount = 2;
  constexpr uint32_t kPulseMs = 150;
  digitalWrite(pins::AVI_LOCATION_LED, LOW);
  if (!checksPassed) {
    digitalWrite(pins::BUZZER, LOW);
    Serial.println("Startup indicator: local checks FAILED; U23/GPIO14 remains LOW");
    return;
  }
  if(isQuiet() && __atomic_load_n(&quietMapWord,__ATOMIC_RELAXED)!=255) {
    Serial.println("Startup indicator: quiet LED setting; U23 pulses omitted");return;
  }
  Serial.println("Startup indicator: U23/GPIO14 two flashes; local checks passed (Wi-Fi/feed readiness is separate)");
  for (uint8_t pulse = 0; pulse < kPulseCount; ++pulse) {
    digitalWrite(pins::AVI_LOCATION_LED, HIGH);
    delay(kPulseMs);
    digitalWrite(pins::AVI_LOCATION_LED, LOW);
    if (pulse + 1 < kPulseCount) delay(kPulseMs);
  }
  playStartupChime();
#else
  (void)checksPassed;
#endif
}

void applyFeedIndicator(bool freshComplete) {
#if defined(BROOKLYN_TARGET_PCB)
  // Channel 4 uses timer 2, separate from buzzer channel 7 / timer 3.
  constexpr uint8_t kIndicatorChannel = 4;
  const uint8_t cap=__atomic_load_n(&feedCapWord,__ATOMIC_RELAXED);
  const uint8_t kDimDuty=cap<8?cap:8;
  const uint8_t desiredDuty=freshComplete?kDimDuty:0;
  if (freshComplete == feedIndicatorOn && desiredDuty==lastFeedDuty) return;
  if (freshComplete && feedIndicatorUnavailable) return;
  if (freshComplete && !feedIndicatorAttached) {
    if (ledcSetup(kIndicatorChannel, 2000, 8) == 0) {
      digitalWrite(pins::AVI_LOCATION_LED, LOW);
      feedIndicatorUnavailable = true;
      Serial.println("Feed indicator: U23/GPIO14 PWM setup failed; remains OFF");
      return;
    }
    ledcWrite(kIndicatorChannel, 0);
    ledcAttachPin(pins::AVI_LOCATION_LED, kIndicatorChannel);
    feedIndicatorAttached = true;
  }
  if (feedIndicatorAttached) ledcWrite(kIndicatorChannel, freshComplete ? kDimDuty : 0);
  else digitalWrite(pins::AVI_LOCATION_LED, LOW);
  lastFeedDuty=desiredDuty;
  if (freshComplete != feedIndicatorOn) {
    if(freshComplete) Serial.printf("Feed indicator: U23/GPIO14 dim ON, duty %u/255\n",unsigned(kDimDuty));
    else Serial.println("Feed indicator: U23/GPIO14 OFF (feed not fresh/complete)");
    feedIndicatorOn = freshComplete;
    if (freshComplete) beepFeedReadyOnce();
  }
#else
  (void)freshComplete;
#endif
}

namespace {
uint32_t indicatorLock = 0;
bool indicatorRequested = false, indicatorAllowed = true;
uint32_t allowedWord = 1;
#if defined(BROOKLYN_TARGET_PCB)
uint32_t knobPending = 0,settingsPending=0,settingsAtMs=0;
uint8_t settingsStep=0;bool knobSettings=false;
bool knobActive = false, knobGap = false;
bool knobReady=false;
uint32_t knobUntilMs = 0;
void stopKnobToneLocked(bool preserveSettings) {
  if(knobActive) {
    ledcWrite(7,0);ledcDetachPin(pins::BUZZER);digitalWrite(pins::BUZZER,LOW);
  }
  knobActive=knobGap=false;
  knobReady=knobSettings=false;settingsStep=0;
  knobService=false;serviceStep=0;serviceKind=servicePending=audio::ServiceEvent::None;
  readyNotes=0;readyToneBit=0;
  if(!preserveSettings)__atomic_store_n(&settingsPending,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&readyPending,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&knobPending,0u,__ATOMIC_RELAXED);
}
#endif
void lockIndicator() {
  while (__atomic_exchange_n(&indicatorLock, 1u, __ATOMIC_ACQUIRE)) delay(1);
}
void unlockIndicator() { __atomic_store_n(&indicatorLock, 0u, __ATOMIC_RELEASE); }
}
void setFeedIndicator(bool freshComplete) {
  lockIndicator();
  indicatorRequested = freshComplete;
  indicatorAllowed = __atomic_load_n(&allowedWord,__ATOMIC_RELAXED)!=0;
  applyFeedIndicator(indicatorRequested && indicatorAllowed);
  unlockIndicator();
}
void setFeedIndicatorAllowed(bool allowed,uint8_t brightnessCap) {
#if defined(BROOKLYN_TARGET_PCB)
  __atomic_store_n(&feedCapWord,uint32_t(brightnessCap),__ATOMIC_RELAXED);
#else
  (void)brightnessCap;
#endif
  __atomic_store_n(&allowedWord,uint32_t(allowed),__ATOMIC_RELAXED);
  // Never stall input sampling behind a main-loop brightness/feed-ready cue.
  if(__atomic_exchange_n(&indicatorLock,1u,__ATOMIC_ACQUIRE)) return;
  indicatorAllowed = __atomic_load_n(&allowedWord,__ATOMIC_RELAXED)!=0;
  applyFeedIndicator(indicatorRequested && indicatorAllowed);
  unlockIndicator();
}

void setQuiet(bool quiet,uint8_t mapBrightness) {
#if defined(BROOKLYN_TARGET_PCB)
  __atomic_store_n(&quietMapWord,uint32_t(quiet?mapBrightness:255),__ATOMIC_RELAXED);
  if(__atomic_exchange_n(&quietWord,uint32_t(quiet),__ATOMIC_RELAXED)!=uint32_t(quiet))
    Serial.println(quiet?"Quiet hours: buzzer muted":"Quiet hours: buzzer enabled");
#else
  (void)quiet;(void)mapBrightness;
#endif
}
void setEasterEggTone(uint16_t frequency) {
#if defined(BROOKLYN_TARGET_PCB)
  static uint16_t playing=0;
  lockIndicator();
  if(isQuiet() || frequency<100 || frequency>4000) frequency=0;
  stopKnobToneLocked();
  if(frequency!=playing) {
    if(playing) {ledcWrite(7,0);ledcDetachPin(pins::BUZZER);digitalWrite(pins::BUZZER,LOW);playing=0;}
    if(frequency && ledcSetup(7,frequency,8)!=0) {
      ledcWrite(7,0);ledcAttachPin(pins::BUZZER,7);ledcWrite(7,128);playing=frequency;recordSound(Sound::Extra);
    }
  }
  unlockIndicator();
#else
  (void)frequency;
#endif
}
void requestKnobClick() {
#if defined(BROOKLYN_TARGET_PCB)
  if(isQuiet()) return;
  uint32_t pending=__atomic_load_n(&knobPending,__ATOMIC_RELAXED);
  while(pending<8 && !__atomic_compare_exchange_n(&knobPending,&pending,pending+1,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
#endif
}
void requestSettingsSaved() {
#if defined(BROOKLYN_TARGET_PCB)
  if(isQuiet())return;
  __atomic_store_n(&settingsAtMs,millis()+250u,__ATOMIC_RELAXED);
  __atomic_store_n(&settingsPending,1u,__ATOMIC_RELEASE);
  Serial.println("Settings saved: confirmation queued");
#endif
}
void enableStatusReadyCues() {
#if defined(BROOKLYN_TARGET_PCB)
  __atomic_store_n(&statusCuesEnabled,1u,__ATOMIC_RELAXED);
#endif
}
void observeStatusGreen(uint8_t bits) {
#if defined(BROOKLYN_TARGET_PCB)
  if(!__atomic_load_n(&statusCuesEnabled,__ATOMIC_RELAXED)) return;
  bits&=7;
  __atomic_store_n(&observedGreen,uint32_t(bits),__ATOMIC_RELAXED);
  const uint32_t prior=__atomic_fetch_or(&readySeen,uint32_t(bits),__ATOMIC_RELAXED);
  __atomic_fetch_and(&readyPending,uint32_t(bits),__ATOMIC_RELAXED);
  if(!isQuiet() && !startupMuted) __atomic_fetch_or(&readyPending,uint32_t(bits)&~prior,__ATOMIC_RELAXED);
#else
  (void)bits;
#endif
}
void observeService(bool available,bool operating,bool enabled) {
#if defined(BROOKLYN_TARGET_PCB)
  __atomic_store_n(&serviceWord,uint32_t(available)|(uint32_t(operating)<<1)|(uint32_t(enabled)<<2),__ATOMIC_RELAXED);
#else
  (void)available;(void)operating;(void)enabled;
#endif
}
SoundStatus lastSound() {
#if defined(BROOKLYN_TARGET_PCB)
  for(unsigned attempt=0;attempt<3;++attempt) {
    const auto sequence=__atomic_load_n(&soundSequence,__ATOMIC_ACQUIRE);
    if(sequence&1)continue;
    const auto code=__atomic_load_n(&soundCode,__ATOMIC_RELAXED);
    const auto at=__atomic_load_n(&soundAtMs,__ATOMIC_RELAXED);
    if(sequence==__atomic_load_n(&soundSequence,__ATOMIC_ACQUIRE)) {
      SoundStatus result;result.sound=static_cast<Sound>(code&255);result.detail=static_cast<uint8_t>(code>>8);result.atMs=at;result.sequence=sequence/2;return result;
    }
  }
#endif
  return {};
}
void serviceKnobClick(uint32_t nowMs,bool allowed) {
#if defined(BROOKLYN_TARGET_PCB)
  if(__atomic_exchange_n(&indicatorLock,1u,__ATOMIC_ACQUIRE)) return;
  const uint32_t service=__atomic_load_n(&serviceWord,__ATOMIC_RELAXED);
  const bool serviceEnabled=(service&6)==6;
  const auto event=servicePolicy.observe(nowMs,service&1,serviceEnabled,isQuiet() || !allowed);
  if((service&1) && servicePending==audio::ServiceEvent::Lost)servicePending=audio::ServiceEvent::None;
  if(event!=audio::ServiceEvent::None)servicePending=event;
  if(!serviceEnabled || isQuiet() || !allowed) {
    servicePending=audio::ServiceEvent::None;
    if(knobService)stopKnobToneLocked();
  }
  if(!allowed || isQuiet() || (knobReady && !__atomic_load_n(&observedGreen,__ATOMIC_RELAXED))) stopKnobToneLocked();
  else if(knobActive) {
    if(static_cast<int32_t>(nowMs-knobUntilMs)>=0) {
      ledcWrite(7,0);ledcDetachPin(pins::BUZZER);digitalWrite(pins::BUZZER,LOW);
      const uint32_t gap=knobService?80:knobSettings?50:knobReady?80:15;
      if(knobService){if(++serviceStep==3){serviceStep=0;serviceKind=audio::ServiceEvent::None;}}
      if(knobReady && readyNotes && --readyNotes==0)readyToneBit=0;
      if(knobSettings)settingsStep=settingsStep==1?2:0;
      knobActive=knobReady=knobSettings=knobService=false;knobGap=true;knobUntilMs=nowMs+gap;
    }
  } else if(!knobGap || static_cast<int32_t>(nowMs-knobUntilMs)>=0) {
    knobGap=false;
    const uint32_t ready=__atomic_load_n(&readyPending,__ATOMIC_RELAXED);
    const bool savedDue=__atomic_load_n(&settingsPending,__ATOMIC_ACQUIRE) &&
        static_cast<int32_t>(nowMs-__atomic_load_n(&settingsAtMs,__ATOMIC_RELAXED))>=0;
    if(settingsStep || savedDue) {
      if(!settingsStep){__atomic_store_n(&settingsPending,0u,__ATOMIC_RELAXED);settingsStep=1;}
      const uint16_t hz=settingsStep==1?1200:1800;
      if(ledcSetup(7,hz,8)!=0) {
        ledcWrite(7,0);ledcAttachPin(pins::BUZZER,7);ledcWrite(7,64);
        knobActive=knobSettings=true;knobUntilMs=nowMs+60;
        if(settingsStep==1)recordSound(Sound::Saved);
      } else {settingsStep=0;digitalWrite(pins::BUZZER,LOW);}
    } else if(serviceKind!=audio::ServiceEvent::None || servicePending!=audio::ServiceEvent::None) {
      if(serviceKind==audio::ServiceEvent::None){serviceKind=servicePending;servicePending=audio::ServiceEvent::None;serviceStep=0;}
      const uint16_t hz=serviceKind==audio::ServiceEvent::Lost?1300-serviceStep*300:700+serviceStep*300;
      if(ledcSetup(7,hz,8)!=0) {
        ledcWrite(7,0);ledcAttachPin(pins::BUZZER,7);ledcWrite(7,64);
        knobActive=knobService=true;knobUntilMs=nowMs+100;
        if(serviceStep==0) {
          if(serviceKind==audio::ServiceEvent::Lost)servicePolicy.lossAnnounced();
          recordSound(serviceKind==audio::ServiceEvent::Lost?Sound::Lost:Sound::Restored);
          Serial.println(serviceKind==audio::ServiceEvent::Lost?"Service sound: lost, three descending notes":"Service sound: restored, three ascending notes");
        }
      } else {servicePolicy.reset();serviceKind=servicePending=audio::ServiceEvent::None;serviceStep=0;digitalWrite(pins::BUZZER,LOW);}
    } else if(readyToneBit || ready) {
      const uint32_t bit=readyToneBit?readyToneBit:ready&(~ready+1u);
      if(!readyToneBit){readyToneBit=bit;readyNotes=bit==1?1:bit==2?2:3;__atomic_fetch_and(&readyPending,~bit,__ATOMIC_RELAXED);}
      const uint16_t hz=bit==1?1000:bit==2?1500:2000;
      if(ledcSetup(7,hz,8)!=0) {
        ledcWrite(7,0);ledcAttachPin(pins::BUZZER,7);ledcWrite(7,128);
        knobActive=knobReady=true;knobUntilMs=nowMs+(bit==4?100:90);
        if(readyNotes==(bit==1?1:bit==2?2:3))recordSound(bit==1?Sound::System:bit==2?Sound::Network:Sound::Feed);
        Serial.printf("Ready beep: %s green, %u Hz\n",bit==1?"SYSTEM":bit==2?"NETWORK":"MTA",unsigned(hz));
      } else {readyToneBit=0;readyNotes=0;digitalWrite(pins::BUZZER,LOW);}
    } else if(__atomic_load_n(&knobPending,__ATOMIC_RELAXED)) {
      __atomic_sub_fetch(&knobPending,1u,__ATOMIC_RELAXED);
      if(ledcSetup(7,2000,8)!=0) {
        ledcWrite(7,0);ledcAttachPin(pins::BUZZER,7);ledcWrite(7,128);
        knobActive=true;knobUntilMs=nowMs+15;
        // Routine navigation clicks must not replace the last meaningful cue.
      } else digitalWrite(pins::BUZZER,LOW);
    }
  }
  unlockIndicator();
#else
  (void)nowMs;(void)allowed;
#endif
}

static void signalFeedback(uint8_t value, uint8_t maximum,uint16_t durationMs,Sound sound=Sound::Brightness) {
#if defined(BROOKLYN_TARGET_PCB)
  if(isQuiet()) return;
  // Main-loop caller; serialize with the feed-ready cue on the same PWM timer.
  lockIndicator();
  stopKnobToneLocked(true);
  constexpr uint8_t channel = 7;
  const uint8_t bounded = value < maximum ? value : maximum;
  const uint16_t frequency = 800 + (maximum ? uint32_t(bounded) * 2400 / maximum : 0);
  if (ledcSetup(channel, frequency, 8) != 0) {
    ledcWrite(channel, 0); ledcAttachPin(pins::BUZZER, channel);
    recordSound(sound,value);
    ledcWrite(channel, 128); delay(durationMs); ledcWrite(channel, 0);
    ledcDetachPin(pins::BUZZER);
  }
  digitalWrite(pins::BUZZER, LOW);
  unlockIndicator();
#else
  (void)value; (void)maximum;(void)durationMs;(void)sound;
#endif
}

void signalBrightness(uint8_t value,uint8_t maximum) {signalFeedback(value,maximum,45);}
void signalSetupReady() {
  signalFeedback(1,3,120,Sound::Setup);
#if defined(BROOKLYN_TARGET_PCB)
  Serial.println(isQuiet()?"Setup AP ready: confirmation muted by quiet hours":"Setup AP ready: confirmation beep complete");
#endif
}

void signalUpgradeStarting() {
#if defined(BROOKLYN_TARGET_PCB)
  __atomic_store_n(&serviceWord,0u,__ATOMIC_RELAXED); // No service alerts during an upgrade.
  if(isQuiet()){Serial.println("Upgrade cue: muted by quiet hours");return;}
  lockIndicator();stopKnobToneLocked();
  constexpr uint8_t channel=7,kGentleDuty=64;
  const uint16_t frequencies[]={1200,900};
  for(uint8_t note=0;note<2;++note) {
    if(isQuiet() || ledcSetup(channel,frequencies[note],8)==0)break;
    ledcWrite(channel,0);ledcAttachPin(pins::BUZZER,channel);ledcWrite(channel,kGentleDuty);
    if(note==0)recordSound(Sound::Upgrade);
    delay(100);ledcWrite(channel,0);ledcDetachPin(pins::BUZZER);digitalWrite(pins::BUZZER,LOW);
    if(note==0)delay(120);
  }
  digitalWrite(pins::BUZZER,LOW);unlockIndicator();
  Serial.println("Upgrade cue: complete (1200/900 Hz, gentle duty 64/255)");
#endif
}

bool beginSafe() {
#if defined(BROOKLYN_TARGET_PCB)
  digitalWrite(pins::LED_ENABLE, LOW);
  pinMode(pins::LED_ENABLE, OUTPUT);
  digitalWrite(pins::LED_ENABLE, LOW);

  pinMode(pins::ROTARY_A, INPUT);
  pinMode(pins::ROTARY_B, INPUT);
  pinMode(pins::ROTARY_PUSH, INPUT);

  pinMode(pins::BUTTON_TEST, INPUT);
  pinMode(pins::BUTTON_ROUTE, INPUT);
  pinMode(pins::BUTTON_DIM, INPUT);
  pinMode(pins::BUTTON_POWER, INPUT);
  pinMode(pins::BUTTON_MODE, INPUT);
  pinMode(pins::BUTTON_BRIGHT, INPUT);

  pinMode(pins::POWER_STAT, INPUT);

  digitalWrite(pins::AVI_LOCATION_LED, LOW);
  if (feedIndicatorAttached) {
    ledcWrite(4, 0);
    ledcDetachPin(pins::AVI_LOCATION_LED);
    feedIndicatorAttached = false;
  }
  feedIndicatorOn = false;
  lastFeedDuty=0;
  feedIndicatorUnavailable = false;
  feedReadyBeepAttempted = false;
  __atomic_store_n(&statusCuesEnabled,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&observedGreen,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&readyPending,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&readySeen,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&serviceWord,0u,__ATOMIC_RELAXED);servicePolicy.reset();
  __atomic_store_n(&soundSequence,0u,__ATOMIC_RELAXED);__atomic_store_n(&soundCode,0u,__ATOMIC_RELAXED);__atomic_store_n(&soundAtMs,0u,__ATOMIC_RELAXED);
  pinMode(pins::AVI_LOCATION_LED, OUTPUT);

  digitalWrite(pins::BUZZER, LOW);
  pinMode(pins::BUZZER, OUTPUT);
  digitalWrite(pins::BUZZER, LOW);
  startupMuted = digitalRead(pins::BUTTON_DIM) == LOW;

  return Wire.begin(pins::I2C_SDA, pins::I2C_SCL);
#else
  return true;
#endif
}

}  // namespace brooklyn::hardware
