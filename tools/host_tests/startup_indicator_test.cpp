#include "hardware.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <stdlib.h>

FakeSerial Serial;
FakeWire Wire;
int pulses = 0, indicatorDelays = 0, notes = 0, gaps = 0;
int enable = LOW, indicator = LOW, buzzer = LOW, dim = HIGH;
int setupCalls = 0, attachCalls = 0, detachCalls = 0, failSetupAt = -1;
uint32_t frequency = 0, duty = 0;
bool buzzerOutput = false;
constexpr uint32_t expectedHz[] = {2500, 3000, 3500, 4000, 2620, 2120};
constexpr uint32_t expectedMs[] = {80, 80, 100, 180, 600, 750};
constexpr uint32_t expectedGaps[] = {45, 45, 45, 250, 90, 0};
uint32_t soundMs = 0;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
void pinMode(uint8_t pin, int mode) {
  if (pin == brooklyn::pins::BUZZER) { CHECK(mode == OUTPUT); buzzerOutput = true; }
}
int digitalRead(uint8_t pin) { CHECK(pin == brooklyn::pins::BUTTON_DIM); return dim; }
void digitalWrite(uint8_t pin, int value) {
  CHECK(pin == brooklyn::pins::LED_ENABLE || pin == brooklyn::pins::AVI_LOCATION_LED || pin == brooklyn::pins::BUZZER);
  if (pin == brooklyn::pins::LED_ENABLE) { CHECK(value == LOW); enable = value; }
  if (pin == brooklyn::pins::BUZZER) { CHECK(value == LOW); buzzer = value; }
  if (pin == brooklyn::pins::AVI_LOCATION_LED) {
    indicator = value;
    if (value == HIGH) ++pulses;
  }
}
uint32_t ledcSetup(uint8_t channel, uint32_t hz, uint8_t resolution) {
  CHECK(channel == 7 && resolution == 8 && duty == 0);
  CHECK(setupCalls < 6 && hz == expectedHz[setupCalls]);
  const bool fail = setupCalls++ == failSetupAt;
  frequency = hz;
  return fail ? 0 : hz;
}
void ledcAttachPin(uint8_t pin, uint8_t channel) {
  CHECK(pin == brooklyn::pins::BUZZER && channel == 7 && buzzerOutput);
  ++attachCalls;
}
void ledcWrite(uint8_t channel, uint32_t value) {
  CHECK(channel == 7 && (value == 0 || value == 128));
  duty = value;
}
void ledcDetachPin(uint8_t pin) {
  CHECK(pin == brooklyn::pins::BUZZER && duty == 0);
  ++detachCalls;
}
void delay(uint32_t ms) {
  CHECK(enable == LOW);
  if (ms != 150) soundMs += ms;
  if (ms == 150) {
    CHECK(duty == 0 && indicator == (indicatorDelays % 2 == 0 ? HIGH : LOW));
    ++indicatorDelays;
  } else if (duty != 0) {
    CHECK(notes < 6 && ms == expectedMs[notes]);
    CHECK(duty == 128 && indicator == LOW && frequency == expectedHz[notes]);
    ++notes;
  } else {
    CHECK(duty == 0 && notes >= 1 && notes <= 5 && gaps + 1 == notes);
    CHECK(ms == expectedGaps[notes - 1]);
    ++gaps;
  }
}
void reset(bool muted, int failAt = -1) {
  pulses = indicatorDelays = notes = gaps = 0;
  setupCalls = attachCalls = detachCalls = 0;
  duty = 0;
  soundMs = 0;
  failSetupAt = failAt;
  dim = muted ? LOW : HIGH;
  CHECK(brooklyn::hardware::beginSafe());
  CHECK(buzzerOutput && buzzer == LOW && enable == LOW);
}
int main() {
  reset(false);
  brooklyn::hardware::indicateStartup(false);
  CHECK(pulses == 0 && setupCalls == 0 && indicator == LOW && buzzer == LOW);
  brooklyn::hardware::indicateStartup(true);
  CHECK(pulses == 2 && indicatorDelays == 3 && notes == 6 && gaps == 5 && soundMs == 2265);
  CHECK(attachCalls == 1 && detachCalls == 1 && duty == 0 && buzzer == LOW && enable == LOW);
  reset(true);
  brooklyn::hardware::indicateStartup(true);
  CHECK(pulses == 2 && setupCalls == 0 && notes == 0 && buzzer == LOW);
  for (int failAt = 0; failAt < 6; ++failAt) {
    reset(false, failAt);
    brooklyn::hardware::indicateStartup(true);
    CHECK(notes == failAt && duty == 0 && buzzer == LOW && enable == LOW);
    CHECK(attachCalls == (failAt > 0 ? 1 : 0) && detachCalls == attachCalls);
  }
  puts("Startup: U23 pulses, four rising notes plus descending pair, 2265 ms, 50%-duty PWM, DIM mute, failed-check silence, PWM failure cleanup, addressable outputs disabled PASS");
}
