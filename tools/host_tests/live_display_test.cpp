#include "live_display_policy.h"
#include "brightness_button_policy.h"
#include "led_power.h"
#include "semantic_api.h"
#include "led_map.h"
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
void testIndicationHistory() {
  live::Controller c;
  auto s=snapshot();semantic::clearFrame(s.frame);
  CHECK(semantic::addValidatedLed(s.frame,0));
  s.frame.animationEpochUnix=100;s.frame.stoppedEpochUnix=100;s.frame.blueSecondsRemaining[0]=20;
  s.clockUnix=100;s.clockAtMs=0;s.brightness=5;
  c.publish(s);CHECK(c.enable(0));c.service(0,false,false,ops);
  CHECK(c.stats().indications.stopped==1);
  display::IndicationReport report{};c.readIndications(report,1234);
  CHECK(report.activeTotal==1 && report.active[0].durationMs==1234 && report.eventCount==0);
  c.service(19960,false,false,ops);CHECK(c.stats().indications.stopped==1);
  c.service(20000,false,false,ops);c.readIndications(report,20000);
  CHECK(report.counts.stopped==0 && report.counts.uncertain==1);
  CHECK(report.eventCount==1 && report.events[0].endedAtMs-report.events[0].startedAtMs==20000);
  CHECK(report.events[0].reason==display::IndicationEnd::StopWindowExpired);
  c.service(20040,false,false,ops);c.readIndications(report,20040);CHECK(report.newestSequence==1);
  // Same configured RGB for two states must not merge their meaning.
  s.frame.stoppedEpochUnix=121;s.frame.stoppedFreshnessLimited[0]=1;s.clockUnix=121;s.clockAtMs=21000;
  s.colors.uncertain=s.colors.stopped;c.publish(s);c.service(21000,false,false,ops);
  c.service(41000,false,false,ops);c.readIndications(report,41000);
  CHECK(report.events[1].reason==display::IndicationEnd::SourceExpired);
  CHECK(report.counts.uncertain==1 && report.counts.stopped==0);
  // Brief changes between HTTP polls remain in history; route filtering is explicit.
  s.frame.stoppedEpochUnix=142;s.clockUnix=142;s.clockAtMs=42000;c.publish(s);c.service(42000,false,false,ops);
  s.routeMask=0;c.publish(s);c.service(42280,false,false,ops);c.readIndications(report,42280);
  CHECK(report.events[2].endedAtMs-report.events[2].startedAtMs==280);
  CHECK(report.events[2].reason==display::IndicationEnd::RouteHidden);
  // Failed transmissions never start a stopped indication.
  s.routeMask=semantic::kAllRoutesVisibleMask;c.publish(s);writeOk=false;c.service(42320,false,false,ops);
  c.readIndications(report,42320);CHECK(report.activeTotal==0 && report.newestSequence==3);writeOk=true;
  CHECK(c.enable(42360));c.service(42360,false,false,ops);c.disable(ops,42400,display::IndicationEnd::HardwareTest);
  c.readIndications(report,42400);CHECK(report.events[3].reason==display::IndicationEnd::HardwareTest);
  CHECK(report.counts.stopped==0);
  // Fill/overwrite the bounded ring, then retrieve all retained endings in pages.
  for(unsigned n=0;n<80;++n) {
    const uint32_t t=43000+n*80;s.frameAtMs=t;s.frame.stoppedEpochUnix=0;s.frame.animationEpochUnix=0;
    c.publish(s);CHECK(c.enable(t));c.service(t,false,false,ops);c.disable(ops,t+40);
  }
  c.readIndications(report,50000,0,false);CHECK(report.gap && report.more && report.eventCount==12);
  CHECK(report.oldestSequence==21 && report.newestSequence==84);
  unsigned count=0;uint32_t cursor=report.oldestSequence-1;
  do {c.readIndications(report,50000,cursor,false);CHECK(!report.gap);count+=report.eventCount;cursor=report.nextAfter;}while(report.more);
  CHECK(count==display::kIndicationHistoryCapacity && cursor==84);
  // Monotonic duration subtraction survives millis rollover.
  live::Controller wrap;auto w=snapshot(0xfffffff0u);semantic::clearFrame(w.frame);
  CHECK(semantic::addValidatedLed(w.frame,0));wrap.publish(w);CHECK(wrap.enable(0xfffffff0u));
  wrap.service(0xfffffff0u,false,false,ops);wrap.disable(ops,40);wrap.readIndications(report,40);
  CHECK(report.events[0].endedAtMs-report.events[0].startedAtMs==56);
}
int main() {
  testIndicationHistory();
  {
    live::Controller automatic(true);
    automatic.service(0, false, false, ops);
    CHECK(!automatic.enabled());
    automatic.publish(snapshot());
    automatic.service(20, false, false, ops);
    CHECK(automatic.enabled() && automatic.screenEnabled());
    uint32_t t = 100;
    for (uint8_t mode = 1; mode <= 4; ++mode) {
      automatic.service(t, true, false, ops);
      automatic.service(t+40, true, false, ops);
      CHECK(automatic.stats().powerMode == (mode-1)%4); // No action on press.
      automatic.service(t+80, false, false, ops);
      automatic.service(t+120, false, false, ops);
      CHECK(automatic.stats().powerMode == mode%4);
      CHECK(automatic.enabled() == ((mode%2)==0));
      CHECK(automatic.screenEnabled() == ((mode%4)<2));
      t += 200;
    }
    automatic.service(t, true, false, ops);
    automatic.service(t+40, true, false, ops);
    automatic.service(t+3039, true, false, ops);
    CHECK(!automatic.consumeRebootRequest());
    automatic.service(t+3040, true, false, ops);
    CHECK(automatic.consumeRebootRequest() && !automatic.enabled());
    automatic.service(t+6000, true, false, ops);
    CHECK(!automatic.consumeRebootRequest());
    automatic.service(t+6040, false, false, ops);
    automatic.service(t+6080, false, false, ops);
    CHECK(automatic.stats().powerMode==0 && !automatic.enabled());
    live::Controller held(true);
    held.publish(snapshot());
    held.service(0, true, false, ops);
    held.service(4000, true, false, ops);
    CHECK(!held.consumeRebootRequest() && !held.enabled());
    held.service(4040, false, false, ops);
    held.service(4080, false, false, ops);
    CHECK(held.stats().powerMode==0 && held.enabled());
    held.disable(ops);
    live::Controller bounce(true);
    bounce.service(0,false,false,ops);
    bounce.service(100,true,false,ops);
    bounce.service(110,false,false,ops);
    bounce.service(150,false,false,ops);
    CHECK(bounce.stats().powerMode==0);
    live::Controller rollover;
    rollover.service(0xfffff700u,false,false,ops);
    rollover.service(0xfffff800u,true,false,ops);
    rollover.service(0xfffff828u,true,false,ops);
    rollover.service(uint32_t(0xfffff828u+3000u),true,false,ops);
    CHECK(rollover.consumeRebootRequest());
    arms = writes = stops = 0;
  }
  {
    using brooklyn::controls::BrightnessButtons;
    using brooklyn::controls::BrightnessRequest;
    using brooklyn::controls::steppedBrightness;
    BrightnessButtons buttons; BrightnessRequest request;
    CHECK(!buttons.update(false,false,0,0,request));
    for(uint8_t mode=0;mode<4;++mode) {
      const uint32_t t=100+mode*300;
      CHECK(!buttons.update(false,false,mode,t,request));
      CHECK(!buttons.update(true,false,mode,t+40,request));
      CHECK(!buttons.update(true,false,mode,t+80,request));
      CHECK(!buttons.update(false,false,mode,t+120,request));
      const bool event=buttons.update(false,false,mode,t+160,request);
      CHECK(event==(mode==1||mode==2));
      if(event) CHECK(request.brighter && request.mode==mode);
    }
    BrightnessButtons held;
    CHECK(!held.update(false,true,1,0,request));
    CHECK(!held.update(false,true,1,100,request));
    CHECK(!held.update(false,false,1,140,request));
    CHECK(!held.update(false,false,1,180,request)); // Boot-held DIM ignored.
    CHECK(!held.update(false,true,1,200,request));
    CHECK(!held.update(false,true,1,240,request));
    CHECK(!held.update(false,false,1,280,request));
    CHECK(held.update(false,false,1,320,request) && !request.brighter);
    CHECK(!held.update(true,false,1,400,request));
    CHECK(!held.update(true,false,1,440,request));
    CHECK(!held.update(true,false,2,480,request));
    CHECK(!held.update(false,false,2,520,request));
    CHECK(!held.update(false,false,2,560,request)); // No cross-mode release.
    CHECK(!held.update(true,true,2,600,request));
    CHECK(!held.update(true,true,2,640,request));
    CHECK(!held.update(true,false,2,680,request));
    CHECK(!held.update(true,false,2,720,request));
    CHECK(!held.update(false,false,2,760,request));
    CHECK(!held.update(false,false,2,800,request)); // Opposing buttons cancel.
    CHECK(steppedBrightness(5,false,true)==6 && steppedBrightness(5,false,false)==4);
    CHECK(steppedBrightness(1,false,false)==1 && steppedBrightness(16,false,true)==16);
    CHECK(steppedBrightness(255,false,true)==16);
    CHECK(steppedBrightness(0,false,true)==1 && steppedBrightness(0,false,false)==0);
    CHECK(steppedBrightness(207,true,false)==191 && steppedBrightness(207,true,true)==223);
    CHECK(steppedBrightness(1,true,false)==0 && steppedBrightness(255,true,true)==255);
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
  // No main-loop publishes during a simulated network stall: worker keeps approaching LEDs continuously lit.
  c.service(499, false, false, ops); CHECK(writes == 2);
  c.service(520, false, false, ops); CHECK(writes == 3 && lit(0));
  c.service(1000, false, false, ops); CHECK(writes == 4 && lit(0));
  CHECK(c.stats().framesWritten == static_cast<uint32_t>(writes));
  CHECK(c.stats().mapChanges == 1 && c.stats().blinkingLeds == 0);
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
    live::Controller counts;auto frame=snapshot();counts.publish(frame);CHECK(counts.enable(0));
    counts.service(0,false,false,ops);CHECK(counts.stats().visibleMapLeds==1);
    CHECK(counts.stats().routeLights[static_cast<uint8_t>(semantic::routeForLed(0))]==1);
    frame.brightness=0;counts.publish(frame);counts.service(40,false,false,ops);
    CHECK(counts.stats().visibleMapLeds==0 && counts.stats().darkOccupiedMapLeds==1);
    frame.brightness=5;frame.routeMask=0;counts.publish(frame);counts.service(80,false,false,ops);
    CHECK(counts.stats().visibleMapLeds==0 && counts.stats().darkOccupiedMapLeds==1);
    frame=snapshot();CHECK(semantic::addValidatedLed(frame.frame,0)); // Two trains share one LED.
    counts.publish(frame);counts.service(120,false,false,ops);
    CHECK(frame.frame.resolvedTrains==2 && counts.stats().visibleMapLeds==1);
    semantic::clearFrame(frame.frame);CHECK(semantic::addUncertainLed(frame.frame,0));
    frame.frame.animationEpochUnix=1;frame.frame.lastReportedFadeStartUnix[0]=1;
    frame.clockUnix=30;frame.clockAtMs=0;frame.unknownTiming=(20u<<10)|(2u<<8)|5;
    counts.publish(frame);counts.service(160,false,false,ops);
    CHECK(counts.stats().visibleMapLeds==0 && counts.stats().darkOccupiedMapLeds==1);
    counts.disable(ops);CHECK(counts.stats().visibleMapLeds==0);
    for(auto count:counts.stats().routeLights) CHECK(count==0);
  }
  {
    live::Controller pages;auto frame=snapshot();pages.publish(frame);CHECK(pages.enable(0));
    led::PhysicalAddress address{};led::GlobalPhysicalIndex index{};
    CHECK(led::map::statusLedAddress(led::StatusLed::Mode,address));
    CHECK(led::map::toGlobalIndex(address,index));
    pages.service(0,false,false,ops);CHECK(!lit(index.value));
    const auto before=last[0].color;
    pages.setOledScreen(1);pages.service(1,false,false,ops); // Even within the same render tick.
    CHECK(last[index.value].color.green==16 && last[index.value].color.blue==16);
    CHECK(last[0].color.red==before.red && last[0].color.green==before.green && last[0].color.blue==before.blue);
    frame.status.provisioning=true;pages.publish(frame);pages.service(2,false,false,ops);
    CHECK(!pages.enabled()); // Recovery still disables physical LED output.
  }
  puts("Live display: safe startup, automatic/manual enable, continuous color, four POWER modes, release debounce, 3-second reboot once, boot-held suppression, test restoration, maintenance/fault guards, stale timeout, brightness/current caps, MODE overlay and rollover PASS");
}
