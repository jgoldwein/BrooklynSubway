#include "controls.h"
#include "board_pins.h"
#include "test_button_policy.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
FakeSerial Serial;
uint32_t nowMs = 1;
int levels[64];
uint32_t millis() { return nowMs; }
int digitalRead(uint8_t pin) { return levels[pin]; }
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
void tick(uint32_t time, int level) {
  nowMs = time; levels[brooklyn::pins::BUTTON_TEST] = level;
  brooklyn::controls::update();
}
int main() {
  using namespace brooklyn::controls;
  for (int &level : levels) level = HIGH;
  begin();
  tick(10, LOW); tick(45, LOW);
  CHECK(!consumeLedDiagnosticRequest());
  tick(70, HIGH); tick(110, HIGH);
  CHECK(consumeLedDiagnosticRequest()); CHECK(!consumeLedDiagnosticRequest());
  tick(120, LOW); tick(125, HIGH); tick(160, HIGH);
  CHECK(!consumeLedDiagnosticRequest());
  tick(170, LOW); tick(210, LOW); tick(1000, LOW);
  CHECK(!consumeLedDiagnosticRequest());
  tick(1010, HIGH); tick(1050, HIGH);
  CHECK(consumeLedDiagnosticRequest()); CHECK(!consumeLedDiagnosticRequest());
  nowMs = 2000; levels[brooklyn::pins::BUTTON_TEST] = LOW; begin();
  tick(2010, HIGH); tick(2050, HIGH);
  CHECK(!consumeLedDiagnosticRequest());
  puts("TEST button: normal press/release queues one fast all-lines request; bounce and held-at-boot ignored PASS");
  brooklyn::controls::TestButton background;
  CHECK(!background.update(true, 0));
  CHECK(!background.update(true, 50));
  CHECK(!background.update(false, 60));
  CHECK(!background.update(false, 100));
  CHECK(!background.update(true, 110));
  CHECK(!background.update(true, 150));
  CHECK(!background.update(false, 160));
  CHECK(background.update(false, 200));
  CHECK(!background.update(false, 250));
  background.reset(true, 300); // a diagnostic ending with TEST held cannot retrigger
  CHECK(!background.update(false, 310));
  CHECK(!background.update(false, 350));
  puts("Background TEST sampler: release event, boot-held guard and diagnostic reset PASS");
}
