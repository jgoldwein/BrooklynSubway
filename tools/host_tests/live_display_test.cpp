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
  {
    live::Controller automatic(true);
    const int armsBefore = arms;
    automatic.service(0, false, false, ops);
    CHECK(!automatic.enabled() && arms == armsBefore);
    live::Snapshot waiting{};
    waiting.status.systemHealthy = true;
    automatic.publish(waiting);
    automatic.service(20, false, false, ops);
    CHECK(!automatic.enabled() && arms == armsBefore);
    automatic.publish(snapshot());
    automatic.service(40, false, false, ops);
    CHECK(automatic.enabled() && arms == armsBefore + 1);
    automatic.service(60, true, false, ops);
    automatic.service(100, true, false, ops);
    CHECK(!automatic.enabled() && automatic.stats().powerCanResume);
    automatic.service(120, false, false, ops);
    automatic.service(160, false, false, ops);
    automatic.service(180, true, false, ops);
    automatic.service(220, true, false, ops);
    CHECK(automatic.enabled());
    automatic.disable(ops);
    automatic.service(240, false, false, ops);
    CHECK(!automatic.enabled());  // Maintenance disable never auto-restarts.
    automatic.publish(snapshot(260));
    automatic.service(280, false, false, ops);
    CHECK(!automatic.enabled() && automatic.stats().powerCanResume);
    automatic.service(300, true, false, ops);
    automatic.service(340, true, false, ops);
    CHECK(automatic.enabled()); // Fresh data permits a physical POWER retry only.
    automatic.disable(ops);
    live::Controller held(true);
    held.service(0, true, false, ops);
    held.publish(snapshot());
    held.service(40, true, false, ops);
    CHECK(!held.enabled());
    held.service(60, false, false, ops);
    held.service(100, false, false, ops);
    held.service(120, true, false, ops);
    held.service(160, true, false, ops);
    CHECK(held.enabled());
    held.disable(ops);
    held.allowPowerResume();
    held.service(200, false, false, ops);
    held.service(240, false, false, ops);
    CHECK(!held.enabled() && held.stats().powerCanResume);
    held.service(260, true, false, ops);
    held.service(300, true, false, ops);
    CHECK(held.enabled());
    held.disable(ops);
    arms = writes = stops = 0;
    live::Controller latePress(true);
    latePress.service(0, false, false, ops);
    latePress.publish(snapshot());
    latePress.service(20, true, false, ops);
    latePress.service(60, true, false, ops);
    CHECK(!latePress.enabled()); // cancelling auto-start cannot also toggle ON
    latePress.service(80, false, false, ops);
    latePress.service(120, false, false, ops);
    latePress.service(140, true, false, ops);
    latePress.service(180, true, false, ops);
    CHECK(latePress.enabled());
    latePress.disable(ops);
    arms = writes = stops = 0;
  }
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
  c.service(499, false, false, ops); CHECK(writes == 2);
  c.service(520, false, false, ops); CHECK(writes == 3 && !lit(0));
  c.service(1000, false, false, ops); CHECK(writes == 4 && lit(0));
  c.service(1020, true, false, ops);
  CHECK(c.enabled());  // Debounce before toggling; a hold must not repeat.
  c.service(1050, true, false, ops);
  CHECK(!c.enabled() && stops == 1);
  CHECK(c.stats().powerCanResume);
  c.service(1100, true, false, ops); CHECK(arms == 1 && !c.enabled());
  c.service(1120, false, false, ops);
  c.service(1150, false, false, ops);
  c.service(1170, true, false, ops);
  c.service(1200, true, false, ops);
  CHECK(c.enabled() && arms == 2 && !c.stats().powerCanResume);
  c.service(1300, true, false, ops); CHECK(c.enabled() && arms == 2);
  CHECK(c.stats().framesWritten == static_cast<uint32_t>(writes));
  CHECK(c.stats().mapChanges == 1 && c.stats().blinkingLeds == 1);
  c.publish(s); CHECK(c.stats().mapChanges == 1);
  c.service(1320, false, false, ops);
  CHECK(c.enable(1340)); c.service(1340, false, true, ops);
  CHECK(!c.enabled());
  CHECK(!c.stats().powerCanResume);
  CHECK(c.enable(1360)); c.service(1360, false, false, ops);
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
  CHECK(!c.stats().powerCanResume);
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
  {
    live::Controller boot;
    boot.publish(snapshot());
    boot.service(0, true, false, ops);
    boot.service(100, true, false, ops);
    CHECK(!boot.enabled());
    boot.service(120, false, false, ops);
    boot.service(160, false, false, ops);
    boot.service(180, true, false, ops);
    boot.service(220, true, false, ops);
    CHECK(!boot.enabled());  // POWER cannot bypass first web enable after boot.
    CHECK(boot.enable(240));
    boot.service(240, false, false, ops);
    boot.service(280, false, false, ops);
    boot.service(300, true, false, ops);
    boot.service(310, false, false, ops);
    boot.service(350, false, false, ops);
    CHECK(boot.enabled());  // Short bounce does not toggle.
    boot.disable(ops);
    boot.service(400, true, false, ops);
    boot.service(440, true, false, ops);
    CHECK(!boot.enabled() && !boot.stats().powerCanResume);
  }
  puts("Live display: safe startup, automatic/manual enable, independent blink, POWER toggle, test restoration, maintenance/fault guards, stale timeout, brightness/current caps and rollover PASS");
}
