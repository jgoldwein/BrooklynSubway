#include "hardware.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <stdlib.h>

FakeSerial Serial;
FakeWire Wire;
int pulses = 0;
int delays = 0;
int enable = LOW;
int indicator = LOW;
bool testing = false;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
void pinMode(uint8_t, int) {}
void digitalWrite(uint8_t pin, int value) {
  if (pin == brooklyn::pins::LED_ENABLE) {
    CHECK(value == LOW);
    enable = value;
  }
  if (testing) CHECK(pin == brooklyn::pins::AVI_LOCATION_LED);
  if (pin == brooklyn::pins::AVI_LOCATION_LED) {
    indicator = value;
    if (value == HIGH) ++pulses;
  }
}
void delay(uint32_t ms) {
  CHECK(ms == 150);
  CHECK(enable == LOW);
  CHECK(indicator == (delays % 2 == 0 ? HIGH : LOW));
  ++delays;
}
int main() {
  CHECK(brooklyn::hardware::beginSafe());
  testing = true;
  brooklyn::hardware::indicateStartup(false);
  CHECK(pulses == 0 && delays == 0 && indicator == LOW);
  brooklyn::hardware::indicateStartup(true);
  CHECK(pulses == 2 && delays == 3 && indicator == LOW && enable == LOW);
  brooklyn::hardware::indicateStartup(false);
  CHECK(pulses == 2 && delays == 3 && indicator == LOW);
  puts("Startup indicator: two U23-only pulses, no addressable enable, no success signal on failed checks PASS");
}
