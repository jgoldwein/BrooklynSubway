#include "live_display_policy.h"
#include "led_power.h"
#include "semantic_api.h"
#include <stdio.h>
#include <stdlib.h>

namespace live = brooklyn::display::live;
namespace display = brooklyn::display;
namespace semantic = brooklyn::semantic;
namespace led = brooklyn::led;
int arms = 0, writes = 0, stops = 0;
bool armOk = true, writeOk = true;
led::DisplayState last[306]{};
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
bool arm(void *) { ++arms; return armOk; }
bool transmit(void *, const led::DisplayState *frame, uint16_t count) {
  CHECK(count == 306);
  ++writes;
  for (uint16_t i = 0; i < count; ++i) {
    last[i] = frame[i];
    CHECK(frame[i].color.red <= 16 && frame[i].color.green <= 16 && frame[i].color.blue <= 16);
  }
  CHECK(led::power::estimateFrameCurrentMa(frame, count, 255) <= led::power::FIRMWARE_LED_CURRENT_BUDGET_MA);
  return writeOk;
}
void stop(void *) { ++stops; }
const live::Operations ops{nullptr, arm, transmit, stop};
bool lit(uint16_t i) { return last[i].color.red || last[i].color.green || last[i].color.blue; }
live::Snapshot snapshot(uint32_t at = 0) {
  live::Snapshot s{};
  s.hasFrame = true;
  s.frameAtMs = at;
  s.status.systemHealthy = true;
  s.status.displayState = display::State::Fresh;
  s.brightness = 255;
  CHECK(semantic::addValidatedLed(s.frame, 0, true));
  return s;
}
int main() {
  live::Controller c;
  CHECK(!c.enable(0));
  c.service(0, false, false, ops);
  CHECK(arms == 0 && writes == 0);
  auto s = snapshot();
  c.publish(s);
  c.service(0, false, false, ops);
  CHECK(!c.enabled() && arms == 0);
  CHECK(!c.enable(display::kPartialGraceMs + 1));
  CHECK(c.enable(0));
  c.service(0, false, false, ops);
  CHECK(arms == 1 && writes == 1 && lit(0));
  // No main-loop publishes during a simulated network stall: worker still blinks.
  c.service(499, false, false, ops); CHECK(writes == 1);
  c.service(500, false, false, ops); CHECK(writes == 2 && !lit(0));
  c.service(1000, false, false, ops); CHECK(writes == 3 && lit(0));
  c.service(1020, true, false, ops);
  CHECK(!c.enabled() && stops == 1);
  c.service(1040, false, false, ops); CHECK(arms == 1);
  CHECK(c.enable(1040)); c.service(1040, false, true, ops);
  CHECK(!c.enabled());
  CHECK(c.enable(1040)); c.service(1040, false, false, ops);
  c.service(300000, false, false, ops); CHECK(!c.enabled());
  CHECK(!c.enable(300000));
  s = snapshot(400000); c.publish(s); CHECK(c.enable(400000));
  c.service(400000, false, false, ops);
  s.status.provisioning = true; c.publish(s);
  c.service(400020, false, false, ops); CHECK(!c.enabled());
  s.status.provisioning = false; s.status.systemHealthy = false; c.publish(s);
  CHECK(!c.enable(400020));
  s.status.systemHealthy = true; s.status.displayState = display::State::StaleHold; c.publish(s);
  CHECK(!c.enable(400020));
  s.status.displayState = display::State::Fresh; c.publish(s);
  armOk = false; CHECK(c.enable(400020));
  c.service(400020, false, false, ops); CHECK(!c.enabled());
  armOk = true; writeOk = false; CHECK(c.enable(400020));
  c.service(400020, false, false, ops); CHECK(!c.enabled());
  writeOk = true;
  s = snapshot(0xffffff00u); c.publish(s); CHECK(c.enable(0xfffffff0u));
  c.service(0xfffffff0u, false, false, ops);
  c.service(500, false, false, ops); CHECK(c.enabled());
  c.disable(ops); CHECK(!c.enabled());
  // An all-map frame at full requested brightness stays capped and budgeted.
  s = snapshot(); semantic::clearFrame(s.frame);
  for (uint16_t i = 0; i < 306; ++i)
    if (semantic::semanticForLed(i)) CHECK(semantic::addValidatedLed(s.frame, i));
  c.publish(s); CHECK(c.enable(0)); c.service(0, false, false, ops);
  CHECK(c.enabled());
  s.status.displayState = display::State::NoData; c.publish(s);
  c.service(20, false, false, ops); CHECK(!c.enabled());
  puts("Live display: OFF at boot, explicit enable, independent blink during network stall, POWER/recovery stop, stale timeout, failure cleanup, brightness/current caps and rollover PASS");
}
