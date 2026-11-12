#include "led_backend.h"
#include "board_pins.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>

FakeSerial Serial;
bool failAllocation = false;
int enable = LOW;
int shows = 0;
int coloredShows = 0;
int delays = 0;
int enableHighs = 0;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
namespace backend = brooklyn::led::backend;
void digitalWrite(uint8_t pin, int level) {
  if (pin == brooklyn::pins::LED_ENABLE) {
    enable = level;
    if (level == HIGH) ++enableHighs;
  }
}
void observeStrip(uint8_t pin, const uint32_t *pixels, uint16_t count) {
  CHECK(enable == HIGH);
  const int frame = shows / 8;
  int colored = 0;
  for (uint16_t i = 0; i < count; ++i) {
    if (pixels[i]) {
      CHECK(frame == 1);
      CHECK(pin == 4 && i == 0 && pixels[i] == 0x040000);
      ++colored;
    }
  }
  CHECK(colored <= 1);
  coloredShows += colored;
  ++shows;
}
void delay(uint32_t milliseconds) {
  CHECK(milliseconds == 2000);
  CHECK(shows == 16 && coloredShows == 1 && enable == HIGH);
  ++delays;
}
int main() {
  CHECK(!backend::initializePhysicalLedBackend());
  CHECK(!backend::armLedOutputs());
  CHECK(!backend::runFirstLedDiagnostic());
  CHECK(enableHighs == 0 && shows == 0);
  backend::setDiagnosticSystemReady(true);
  failAllocation = true;
  CHECK(!backend::runFirstLedDiagnostic());
  CHECK(enable == LOW && shows == 0);
  failAllocation = false;
  CHECK(backend::runFirstLedDiagnostic());
  CHECK(enable == LOW && !backend::physicalLedOutputsArmed());
  CHECK(shows == 24 && coloredShows == 1 && delays == 1);
  CHECK(!backend::physicalLedOutputPermissionEnabled());
  CHECK(!backend::armLedOutputs());
  brooklyn::led::DisplayState frame[306]{};
  CHECK(!backend::transmitFrame(frame, 306));
  CHECK(enable == LOW && shows == 24);
  backend::setDiagnosticSystemReady(false);
  CHECK(!backend::runFirstLedDiagnostic());
  CHECK(enable == LOW && shows == 24);
  puts("LED diagnostic: startup interlock, allocation failure, black/color/black, exact duration, normal-output lock PASS");
}
