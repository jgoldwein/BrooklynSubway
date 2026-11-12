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
int expectedPixels = 1;
int abortAfter = -1;
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
      CHECK(frame >= 1 && frame <= expectedPixels);
      CHECK(pin == 4 && i == frame - 1 && pixels[i] == 0x040000);
      ++colored;
    }
  }
  CHECK(colored <= 1);
  coloredShows += colored;
  ++shows;
}
void delay(uint32_t milliseconds) {
  CHECK(milliseconds == (expectedPixels == 1 ? 2000u : 500u));
  CHECK(shows == (delays + 2) * 8 && coloredShows == delays + 1 && enable == HIGH);
  ++delays;
}
int digitalRead(uint8_t pin) {
  CHECK(pin == brooklyn::pins::BUTTON_POWER);
  return abortAfter >= 0 && delays >= abortAfter ? LOW : HIGH;
}
void resetWalk(int abortAt) {
  shows = coloredShows = delays = 0;
  expectedPixels = 37;
  abortAfter = abortAt;
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
  backend::setDiagnosticSystemReady(true);
  resetWalk(-1);
  CHECK(backend::runChannelOneDiagnostic());
  CHECK(shows == 39 * 8 && coloredShows == 37 && delays == 37);
  CHECK(enable == LOW && !backend::physicalLedOutputsArmed());
  resetWalk(3);
  CHECK(!backend::runChannelOneDiagnostic());
  CHECK(shows == 5 * 8 && coloredShows == 3 && delays == 3 && enable == LOW);
  resetWalk(0);
  CHECK(!backend::runChannelOneDiagnostic());
  CHECK(shows == 2 * 8 && coloredShows == 0 && delays == 0 && enable == LOW);
  CHECK(!backend::armLedOutputs());
  puts("LED diagnostic: CH1 37-pixel walk/POWER abort plus startup interlock, allocation failure, black/color/black, exact duration, normal-output lock PASS");
}
