// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "hardware.h"

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
bool feedIndicatorAttached = false;
bool feedIndicatorOn = false;
bool feedIndicatorUnavailable = false;

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
  if (startupMuted) {
    Serial.println("Startup buzzer: muted by DIM at boot; GPIO21 LOW");
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
  digitalWrite(pins::BUZZER, LOW);
  if (number < 1 || number > 8 || digitalRead(pins::BUTTON_POWER) == LOW ||
      controls::pollWifiRecoveryDuringDiagnostic()) return false;
  const auto wait = [](uint32_t ms) {
    constexpr uint32_t kSliceMs = 20;
    while (ms > 0) {
      if (digitalRead(pins::BUTTON_POWER) == LOW ||
          controls::pollWifiRecoveryDuringDiagnostic()) return false;
      const uint32_t slice = ms < kSliceMs ? ms : kSliceMs;
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
    ledcWrite(kChannel, 128);
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

void setFeedIndicator(bool freshComplete) {
#if defined(BROOKLYN_TARGET_PCB)
  // Channel 4 uses timer 2, separate from buzzer channel 7 / timer 3.
  constexpr uint8_t kIndicatorChannel = 4;
  constexpr uint8_t kDimDuty = 8;  // 8/255, about 3% of the discrete LED's on-current
  if (freshComplete == feedIndicatorOn) return;
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
  if (freshComplete != feedIndicatorOn) {
    Serial.println(freshComplete ? "Feed indicator: U23/GPIO14 dim ON, duty 8/255" : "Feed indicator: U23/GPIO14 OFF (feed not fresh/complete)");
    feedIndicatorOn = freshComplete;
  }
#else
  (void)freshComplete;
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
  feedIndicatorUnavailable = false;
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
