// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "hardware.h"

#include "board_pins.h"

#if defined(BROOKLYN_TARGET_PCB)
#include <Wire.h>
#include <esp32-hal-ledc.h>
#endif

namespace brooklyn::hardware {

#if defined(BROOKLYN_TARGET_PCB)
namespace {
bool startupMuted = false;

void playStartupChime() {
  // Reserve channel 7 (LEDC timer 3) for BUZZER3. No tone task is allocated.
  constexpr uint8_t kBuzzerChannel = 7;
  constexpr uint8_t kResolutionBits = 8;
  constexpr uint8_t kQuietDuty = 16;
  constexpr uint32_t kNoteMs = 80;
  constexpr uint32_t kGapMs = 60;
  constexpr uint16_t kNotesHz[] = {3000, 4000};
  digitalWrite(pins::BUZZER, LOW);
  if (startupMuted) {
    Serial.println("Startup buzzer: muted by DIM at boot; GPIO21 LOW");
    return;
  }
  Serial.println("Startup buzzer: BUZZER3 GPIO21, 3000/4000 Hz, 80 ms each, duty 16/255, 60 ms gap");
  bool attached = false;
  bool complete = true;
  for (uint8_t note = 0; note < 2; ++note) {
    if (ledcSetup(kBuzzerChannel, kNotesHz[note], kResolutionBits) == 0) {
      complete = false;
      break;
    }
    if (!attached) {
      ledcAttachPin(pins::BUZZER, kBuzzerChannel);
      attached = true;
    }
    ledcWrite(kBuzzerChannel, kQuietDuty);
    delay(kNoteMs);
    ledcWrite(kBuzzerChannel, 0);
    if (note == 0) delay(kGapMs);
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
