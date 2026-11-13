#include "led_backend.h"
#include "board_pins.h"
#include "led_diagnostic_policy.h"
#include "led_map.h"
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
uint8_t expectedPin = 4;
constexpr uint8_t kExpectedPins[] = {4, 5, 6, 7, 10, 11, 12, 13};
constexpr int kExpectedCounts[] = {37, 38, 37, 37, 42, 37, 38, 40};
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
      CHECK(pin == expectedPin && i == frame - 1 && pixels[i] == 0x040000);
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
void resetWalk(int abortAt, int channelIndex = 0) {
  shows = coloredShows = delays = 0;
  expectedPixels = kExpectedCounts[channelIndex];
  expectedPin = kExpectedPins[channelIndex];
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
  using brooklyn::led::PhysicalChannel;
  using namespace brooklyn::led::diagnostic;
  Selection selection{};
  CHECK(parseSelection("first", selection) && selection.kind == Kind::FirstLed);
  for (int channelIndex = 0; channelIndex < 8; ++channelIndex) {
    char name[] = "ch1";
    name[2] = static_cast<char>('1' + channelIndex);
    CHECK(parseSelection(name, selection) && selection.kind == Kind::ChannelWalk);
    CHECK(static_cast<int>(selection.channel) == channelIndex + 1);
    const int count = kExpectedCounts[channelIndex];
    const auto channel = static_cast<PhysicalChannel>(channelIndex + 1);
    const int stops[] = {-1, 0, 3, count - 1};
    for (int stop : stops) {
      resetWalk(stop, channelIndex);
      CHECK(backend::runChannelDiagnostic(channel) == (stop == -1));
      const int expectedSteps = stop == -1 ? count : stop;
      CHECK(shows == (expectedSteps + 2) * 8);
      CHECK(coloredShows == expectedSteps && delays == expectedSteps);
      CHECK(enable == LOW && !backend::physicalLedOutputsArmed());
      CHECK(!backend::armLedOutputs());
      CHECK(!backend::transmitFrame(frame, 306));
    }
  }
  const char *badValues[] = {"", "all", "ch0", "ch9", "ch10", "ch2x", "CH2", " ch2", "ch2 ", "ch-1"};
  for (const char *bad : badValues) {
    CHECK(!parseSelection(bad, selection));
  }
  CHECK(!parseSelection(nullptr, selection));
  const int badChannels[] = {0, 9, 255};
  for (int bad : badChannels) {
    resetWalk(-1);
    CHECK(!backend::runChannelDiagnostic(static_cast<PhysicalChannel>(bad)));
    CHECK(shows == 0 && enable == LOW);
  }
  constexpr uint16_t kStatusDesignators[] = {252, 251, 306, 307};
  for (uint8_t index = 0; index < 4; ++index) {
    brooklyn::led::GlobalPhysicalIndex global{};
    brooklyn::led::map::PhysicalLed physical{};
    CHECK(brooklyn::led::map::toGlobalIndex({PhysicalChannel::CH8, index}, global));
    CHECK(brooklyn::led::map::physicalLed(global, physical));
    CHECK(physical.designator.value == kStatusDesignators[index]);
    CHECK(physical.role == brooklyn::led::LedRole::Status);
  }
  puts("LED diagnostic: all 8 channel walks, exact counts/pins/order, CH8 status mapping, POWER abort, strict selection, startup interlock, black cleanup and normal-output lock PASS");
}
