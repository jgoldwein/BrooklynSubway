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
bool cueMode = false;
bool feedReadyMode = false;
bool brightnessMode = false;
bool knobMode=false,probePriority=false,eggMode=false,readyMode=false;
uint32_t lastFeedbackMs=0;
bool serviceMode=false;
bool settingsCueMode=false;uint32_t soundNow=0;
uint32_t millis(){return soundNow;}
bool upgradeCueMode=false;uint32_t upgradeCueMs=0;int upgradeNotes=0;
int readyBeeps = 0;
int beepStarts = 0, cueMs = 0, cueOnMs = 0, abortAtMs = -1;
bool recoveryRequested = false;
uint32_t feedDuty = 0;
int feedSetups = 0, feedAttaches = 0, feedDetaches = 0;
bool failFeedSetup = false;
namespace brooklyn::controls {
bool pollWifiRecoveryDuringDiagnostic() { return recoveryRequested; }
}
constexpr uint32_t expectedHz[] = {2500, 3000, 3500, 4000, 655, 530};
constexpr uint32_t expectedMs[] = {80, 80, 100, 180, 600, 750};
constexpr uint32_t expectedGaps[] = {45, 45, 45, 250, 90, 0};
uint32_t soundMs = 0;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
void pinMode(uint8_t pin, int mode) {
  if (pin == brooklyn::pins::BUZZER) { CHECK(mode == OUTPUT); buzzerOutput = true; }
}
int digitalRead(uint8_t pin) {
  if (pin == brooklyn::pins::BUTTON_POWER) {
    CHECK(cueMode);
    return abortAtMs >= 0 && cueMs >= abortAtMs ? LOW : HIGH;
  }
  CHECK(pin == brooklyn::pins::BUTTON_DIM); return dim;
}
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
  if (channel == 4) {
    CHECK(hz == 2000 && resolution == 8);
    ++feedSetups; return failFeedSetup ? 0 : hz;
  }
  CHECK(channel == 7 && resolution == 8 && duty == 0);
  if(serviceMode){CHECK(hz==700 || hz==1000 || hz==1300);}
  else if(settingsCueMode){CHECK(hz==1200 || hz==1800 || hz==2000 || hz==800);}
  else if(upgradeCueMode){CHECK(hz==(upgradeNotes?900u:1200u));++upgradeNotes;}
  else if(readyMode) {CHECK(hz==1000 || hz==1500 || hz==2000);}
  else if(eggMode) {CHECK(hz>=100 && hz<=4000);}
  else if (knobMode) {CHECK(hz==2000);}
  else if (brightnessMode) { CHECK(hz >= 800 && hz <= 3200); }
  else if (feedReadyMode) { CHECK(hz == 2000 && feedDuty == 8); }
  else if (cueMode) { CHECK(setupCalls == 0 && hz == 4000); }
  else { CHECK(setupCalls < 6 && hz == expectedHz[setupCalls]); }
  const bool fail = setupCalls++ == failSetupAt;
  frequency = hz;
  return fail ? 0 : hz;
}
void ledcAttachPin(uint8_t pin, uint8_t channel) {
  if (pin == brooklyn::pins::AVI_LOCATION_LED) {
    CHECK(channel == 4); ++feedAttaches; return;
  }
  CHECK(pin == brooklyn::pins::BUZZER && channel == 7 && buzzerOutput);
  ++attachCalls;
}
void ledcWrite(uint8_t channel, uint32_t value) {
  if (channel == 4) { CHECK(value <= 8); feedDuty = value; return; }
  CHECK(channel == 7 && (value == 0 || value == 128 || ((upgradeCueMode || settingsCueMode || serviceMode) && value==64)));
  if (cueMode && value == 128) ++beepStarts;
  duty = value;
}
void ledcDetachPin(uint8_t pin) {
  if (pin == brooklyn::pins::AVI_LOCATION_LED) { CHECK(feedDuty == 0); ++feedDetaches; return; }
  CHECK(pin == brooklyn::pins::BUZZER && duty == 0);
  ++detachCalls;
}
void delay(uint32_t ms) {
  if(upgradeCueMode){CHECK((ms==100 && duty==64)||(ms==120 && duty==0));upgradeCueMs+=ms;return;}
  CHECK(!readyMode && !serviceMode);
  CHECK(enable == LOW);
  if (brightnessMode) { CHECK((ms == 45 || ms == 120) && duty == 128);lastFeedbackMs=ms;
    if(probePriority) {const auto before=frequency;brooklyn::hardware::serviceKnobClick(100,true);brooklyn::hardware::setFeedIndicatorAllowed(true);CHECK(frequency==before && duty==128);}
    return;
  }
  CHECK(!knobMode); // Click feedback must never call delay().
  if (feedReadyMode) { CHECK(ms == 100 && duty == 128 && feedDuty == 8); ++readyBeeps; return; }
  if (cueMode) {
    CHECK(ms <= 20 && indicator == LOW);
    cueMs += ms;
    if (duty) cueOnMs += ms;
    return;
  }
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
  brooklyn::hardware::serviceKnobClick(0,false);
  settingsCueMode=serviceMode=false;soundNow=0;
  knobMode=false;probePriority=false;readyMode=false;upgradeCueMode=false;upgradeCueMs=0;upgradeNotes=0;
  brightnessMode = false;
  cueMode = false;
  feedReadyMode = false;
  readyBeeps = 0;
  pulses = indicatorDelays = notes = gaps = 0;
  setupCalls = attachCalls = detachCalls = 0;
  duty = 0;
  soundMs = 0;
  failSetupAt = failAt;
  dim = muted ? LOW : HIGH;
  CHECK(brooklyn::hardware::beginSafe());
  CHECK(buzzerOutput && buzzer == LOW && enable == LOW);
}
void resetCue(bool muted = false, int abortAt = -1, int failAt = -1) {
  reset(muted, failAt);
  cueMode = true;
  beepStarts = cueMs = cueOnMs = 0;
  abortAtMs = abortAt;
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
  for (int number = 1; number <= 8; ++number) {
    resetCue();
    CHECK(brooklyn::hardware::signalDiagnosticChannel(static_cast<brooklyn::led::PhysicalChannel>(number)));
    CHECK(beepStarts == number && cueOnMs == number * 80);
    CHECK(cueMs == number * 80 + (number - 1) * 80 + 300);
    CHECK(duty == 0 && buzzer == LOW && enable == LOW && detachCalls == 1);
  }
  resetCue(true);
  CHECK(brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH2));
  CHECK(beepStarts == 2);
  resetCue(false, 0);
  CHECK(!brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH1));
  CHECK(attachCalls == 0 && duty == 0);
  resetCue(false, 40);
  CHECK(!brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH8));
  CHECK(beepStarts == 1 && cueOnMs == 40 && duty == 0 && buzzer == LOW && detachCalls == 1);
  resetCue(false, -1, 0);
  CHECK(!brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH1));
  CHECK(attachCalls == 0 && duty == 0);
  resetCue();
  CHECK(!brooklyn::hardware::signalDiagnosticChannel(static_cast<brooklyn::led::PhysicalChannel>(0)));
  CHECK(!brooklyn::hardware::signalDiagnosticChannel(static_cast<brooklyn::led::PhysicalChannel>(9)));
  CHECK(setupCalls == 0 && duty == 0);
  recoveryRequested = true;
  resetCue();
  CHECK(!brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH1));
  CHECK(setupCalls == 0 && duty == 0);
  reset(false);
  feedReadyMode = true;
  brooklyn::hardware::setFeedIndicator(false); CHECK(feedDuty == 0 && feedSetups == 0);
  brooklyn::hardware::setFeedIndicator(true);
  CHECK(feedSetups == 1 && feedAttaches == 1 && feedDuty == 8 && enable == LOW);
  CHECK(readyBeeps == 1 && duty == 0 && buzzer == LOW && detachCalls == 1);
  brooklyn::hardware::setFeedIndicator(true); CHECK(feedSetups == 1);
  brooklyn::hardware::setFeedIndicator(false); CHECK(feedDuty == 0);
  brooklyn::hardware::setFeedIndicator(true); CHECK(feedDuty == 8 && feedSetups == 1);
  CHECK(readyBeeps == 1); // No repeats for unchanged state or OFF/ON recovery.
  brooklyn::hardware::setFeedIndicatorAllowed(false); CHECK(feedDuty == 0);
  brooklyn::hardware::setFeedIndicator(true); CHECK(feedDuty == 0);
  brooklyn::hardware::setFeedIndicatorAllowed(true); CHECK(feedDuty == 8 && readyBeeps == 1);
  CHECK(brooklyn::hardware::beginSafe()); CHECK(feedDuty == 0 && feedDetaches == 1);
  failFeedSetup = true;
  brooklyn::hardware::setFeedIndicator(true);
  const int failedCalls = feedSetups;
  brooklyn::hardware::setFeedIndicator(true);
  CHECK(feedSetups == failedCalls && feedDuty == 0 && enable == LOW);
  CHECK(readyBeeps == 1); // Failed indicator allocation cannot trigger a new beep.
  failFeedSetup = false;
  reset(true); feedReadyMode = true;
  brooklyn::hardware::setFeedIndicator(true);
  CHECK(feedDuty == 8 && readyBeeps == 0 && setupCalls == 0 && duty == 0);
  reset(false, 0); feedReadyMode = true;
  brooklyn::hardware::setFeedIndicator(true);
  CHECK(feedDuty == 8 && readyBeeps == 0 && setupCalls == 1 && duty == 0 && attachCalls == 0);
  brooklyn::hardware::setFeedIndicator(false); brooklyn::hardware::setFeedIndicator(true);
  CHECK(setupCalls == 1); // Failed buzzer setup is not retried repeatedly.
  reset(false); feedReadyMode = true;
  brooklyn::hardware::setFeedIndicator(true);
  CHECK(readyBeeps == 1 && feedDuty == 8 && duty == 0); // New boot rearms the cue.
  puts("Feed-ready cue: U23 ON before 100 ms / 2000 Hz beep, once per boot, mute and failure cleanup PASS");
  puts("U23 feed indicator: GPIO14, dim duty 8/255, separate PWM timer, OFF/stale path, reboot and allocation failure cleanup PASS");
  reset(false); brightnessMode = true;
  brooklyn::hardware::signalBrightness(0,255);
  CHECK(frequency==800 && duty==0 && buzzer==LOW && detachCalls==1);
  brooklyn::hardware::signalBrightness(16,16);
  CHECK(frequency==3200 && duty==0 && detachCalls==2);
  brooklyn::hardware::signalBrightness(255,16); CHECK(frequency==3200 && duty==0);
  reset(false,0); brightnessMode = true;
  brooklyn::hardware::signalBrightness(5,16);
  CHECK(attachCalls==0 && duty==0 && buzzer==LOW);
  puts("Brightness cue: scaled pitch, bounded 45 ms tone, PWM cleanup and setup failure PASS");
  using brooklyn::hardware::requestKnobClick;using brooklyn::hardware::serviceKnobClick;
  reset(false);knobMode=true;
  requestKnobClick();serviceKnobClick(0,true);CHECK(duty==128 && frequency==2000 && attachCalls==1);
  serviceKnobClick(14,true);CHECK(duty==128);
  serviceKnobClick(15,true);CHECK(duty==0 && detachCalls==1);
  requestKnobClick();serviceKnobClick(20,true);CHECK(duty==0);
  serviceKnobClick(30,true);CHECK(duty==128 && attachCalls==2);
  serviceKnobClick(45,false);CHECK(duty==0 && detachCalls==2);
  requestKnobClick();serviceKnobClick(50,false);serviceKnobClick(55,true);CHECK(duty==0 && attachCalls==2);
  requestKnobClick();serviceKnobClick(UINT32_MAX-5,true);serviceKnobClick(8,true);CHECK(duty==128);
  serviceKnobClick(9,true);CHECK(duty==0);
  serviceKnobClick(10,false);
  reset(false);knobMode=true;
  for(int i=0;i<30;++i) requestKnobClick();
  for(uint32_t ms=0;ms<=400;ms+=15) serviceKnobClick(ms,true);
  CHECK(attachCalls==8 && detachCalls==8 && duty==0); // Bounded queue, no long tail.
  serviceKnobClick(405,false);
  reset(false,0);knobMode=true;requestKnobClick();serviceKnobClick(0,true);
  CHECK(duty==0 && attachCalls==0 && buzzer==LOW);
  reset(false);knobMode=true;requestKnobClick();serviceKnobClick(0,true);CHECK(duty==128);
  knobMode=false;brightnessMode=true;probePriority=true;
  brooklyn::hardware::signalBrightness(16,16);CHECK(duty==0 && frequency==3200);
  serviceKnobClick(200,true);CHECK(duty==0 && frequency==3200); // No stale click cleanup interrupts priority cue.
  puts("Knob clicks: nonblocking 15ms/2000Hz, gap, bounded queue, cancellation, rollover, PWM failure and brightness priority PASS");
  brooklyn::hardware::setQuiet(true);reset(false);feedReadyMode=true;
  brooklyn::hardware::setFeedIndicator(true);CHECK(readyBeeps==0 && duty==0);
  brooklyn::hardware::signalBrightness(16,16);CHECK(duty==0 && attachCalls==0);
  requestKnobClick();serviceKnobClick(0,true);CHECK(duty==0 && attachCalls==0);
  resetCue();CHECK(!brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH1)); // Recovery still wins over quiet bypass.
  recoveryRequested=false;CHECK(brooklyn::hardware::signalDiagnosticChannel(brooklyn::led::PhysicalChannel::CH1));CHECK(duty==0 && setupCalls==0);
  reset(false);brooklyn::hardware::indicateStartup(true);CHECK(notes==0 && pulses==2 && duty==0);
  brooklyn::hardware::setQuiet(true,0);reset(false);brooklyn::hardware::indicateStartup(true);CHECK(pulses==0 && notes==0);
  brooklyn::hardware::setQuiet(true,1);brooklyn::hardware::setFeedIndicatorAllowed(true,1);CHECK(feedDuty==1);
  brooklyn::hardware::setFeedIndicatorAllowed(false,1);CHECK(feedDuty==0);
  brooklyn::hardware::setFeedIndicatorAllowed(true,8);CHECK(feedDuty==8);
  brooklyn::hardware::setQuiet(false);
  puts("Quiet hours: startup/feed/brightness/rotary/diagnostic cues muted, visual tests preserved PASS");
  reset(false);eggMode=true;
  brooklyn::hardware::setEasterEggTone(700);CHECK(duty==128 && frequency==700);
  const int attached=attachCalls;
  brooklyn::hardware::setEasterEggTone(700);CHECK(attachCalls==attached);
  serviceKnobClick(0,false);CHECK(duty==128); // Knob cleanup cannot silence effect ownership.
  brooklyn::hardware::setQuiet(true);brooklyn::hardware::setEasterEggTone(700);CHECK(duty==0 && buzzer==LOW);
  brooklyn::hardware::setQuiet(false);brooklyn::hardware::setEasterEggTone(1047);CHECK(duty==128 && frequency==1047);
  brooklyn::hardware::setEasterEggTone(0);CHECK(duty==0 && buzzer==LOW);
  brooklyn::hardware::setEasterEggTone(5000);CHECK(duty==0);
  reset(false,0);eggMode=true;brooklyn::hardware::setEasterEggTone(700);CHECK(duty==0 && attachCalls==0);
  brooklyn::hardware::setEasterEggTone(0);
  puts("Easter tone: shared PWM ownership, repeated pitch, quiet suppression, bounds and failure cleanup PASS");
  reset(false);readyMode=true;
  using brooklyn::hardware::observeStatusGreen;using brooklyn::hardware::enableStatusReadyCues;
  enableStatusReadyCues();observeStatusGreen(1);serviceKnobClick(0,true);CHECK(duty==128 && frequency==1000);
  CHECK(brooklyn::hardware::lastSound().sound==brooklyn::hardware::Sound::System);
  serviceKnobClick(90,true);serviceKnobClick(170,true);CHECK(attachCalls==1);
  observeStatusGreen(3);serviceKnobClick(171,true);CHECK(duty==128 && frequency==1500);
  serviceKnobClick(261,true);serviceKnobClick(341,true);CHECK(duty==128 && frequency==1500);
  serviceKnobClick(431,true);serviceKnobClick(511,true);CHECK(duty==0 && attachCalls==3);
  observeStatusGreen(7);serviceKnobClick(512,true);CHECK(duty==128 && frequency==2000);
  serviceKnobClick(612,true);serviceKnobClick(692,true);serviceKnobClick(792,true);serviceKnobClick(872,true);serviceKnobClick(972,true);serviceKnobClick(1052,true);
  CHECK(duty==0 && attachCalls==6 && brooklyn::hardware::lastSound().sequence==3);
  brooklyn::hardware::setFeedIndicator(true);CHECK(readyBeeps==0 && attachCalls==6);
  observeStatusGreen(3);observeStatusGreen(7);serviceKnobClick(1100,true);CHECK(duty==0 && attachCalls==6);
  const int cues=attachCalls;
  brooklyn::hardware::setQuiet(true);observeStatusGreen(7);serviceKnobClick(1200,true);CHECK(duty==0);
  brooklyn::hardware::setQuiet(false);observeStatusGreen(7);serviceKnobClick(1300,true);CHECK(attachCalls==cues);
  observeStatusGreen(0);observeStatusGreen(7);serviceKnobClick(1400,false);serviceKnobClick(1500,true);CHECK(duty==0 && attachCalls==cues);
  reset(false,0);readyMode=true;enableStatusReadyCues();observeStatusGreen(2);serviceKnobClick(0,true);CHECK(duty==0 && attachCalls==0);
  reset(true);readyMode=true;enableStatusReadyCues();observeStatusGreen(7);serviceKnobClick(0,true);CHECK(duty==0 && attachCalls==0);
  reset(false);brightnessMode=true;brooklyn::hardware::signalSetupReady();CHECK(lastFeedbackMs==120 && frequency==1600 && duty==0 && buzzer==LOW && attachCalls==1 && detachCalls==1);
  brooklyn::hardware::setQuiet(true);brooklyn::hardware::signalSetupReady();CHECK(attachCalls==1 && duty==0);brooklyn::hardware::setQuiet(false);
  reset(false,0);brightnessMode=true;brooklyn::hardware::signalSetupReady();CHECK(duty==0 && buzzer==LOW && attachCalls==0);
  puts("Ready cues: one/two/three distinct pulses, once per service per boot, no U23 duplicate, quiet/DIM/maintenance/power cancellation and PWM failure PASS");
  reset(false);upgradeCueMode=true;brooklyn::hardware::signalUpgradeStarting();CHECK(upgradeNotes==2 && upgradeCueMs==320 && duty==0 && buzzer==LOW && attachCalls==2 && detachCalls==2);
  brooklyn::hardware::setQuiet(true);brooklyn::hardware::signalUpgradeStarting();CHECK(upgradeNotes==2 && upgradeCueMs==320 && duty==0);brooklyn::hardware::setQuiet(false);
  reset(false,0);upgradeCueMode=true;brooklyn::hardware::signalUpgradeStarting();CHECK(attachCalls==0 && duty==0 && buzzer==LOW);
  reset(false,1);upgradeCueMode=true;brooklyn::hardware::signalUpgradeStarting();CHECK(attachCalls==1 && detachCalls==1 && duty==0 && buzzer==LOW);
  puts("Upgrade cue: two descending 100ms notes, 120ms gap, quarter-duty, quiet suppression and PWM failure cleanup PASS");
  reset(false);settingsCueMode=true;
  using brooklyn::hardware::requestSettingsSaved;
  requestSettingsSaved();serviceKnobClick(249,true);CHECK(duty==0);
  serviceKnobClick(250,true);CHECK(duty==64 && frequency==1200);
  serviceKnobClick(310,true);CHECK(duty==0);serviceKnobClick(360,true);CHECK(duty==64 && frequency==1800);
  serviceKnobClick(420,true);serviceKnobClick(470,true);CHECK(duty==0 && attachCalls==2 && detachCalls==2);
  reset(false);settingsCueMode=true;for(unsigned i=0;i<20;++i){soundNow=i;requestSettingsSaved();}
  serviceKnobClick(268,true);CHECK(duty==0);serviceKnobClick(269,true);CHECK(duty==64);
  brooklyn::hardware::setQuiet(true);serviceKnobClick(270,true);CHECK(duty==0);requestSettingsSaved();
  brooklyn::hardware::setQuiet(false);serviceKnobClick(600,true);CHECK(attachCalls==1 && duty==0);
  reset(false,0);settingsCueMode=true;requestSettingsSaved();serviceKnobClick(250,true);CHECK(duty==0 && attachCalls==0);
  reset(false);settingsCueMode=true;requestSettingsSaved();serviceKnobClick(250,false);serviceKnobClick(500,true);CHECK(duty==0 && attachCalls==0);
  reset(false);settingsCueMode=true;soundNow=UINT32_MAX-100;requestSettingsSaved();serviceKnobClick(148,true);CHECK(duty==0);serviceKnobClick(149,true);CHECK(duty==64);
  serviceKnobClick(209,true);serviceKnobClick(259,true);serviceKnobClick(319,true);CHECK(duty==0 && attachCalls==2);
  reset(false,1);settingsCueMode=true;requestSettingsSaved();serviceKnobClick(250,true);serviceKnobClick(310,true);serviceKnobClick(360,true);CHECK(duty==0 && attachCalls==1 && detachCalls==1);serviceKnobClick(500,true);CHECK(attachCalls==1);
  reset(false);settingsCueMode=brightnessMode=true;requestSettingsSaved();brooklyn::hardware::signalBrightness(0,255);brightnessMode=false;serviceKnobClick(250,true);CHECK(frequency==1200 && duty==64);serviceKnobClick(310,false);CHECK(duty==0);
  puts("Saved settings cue: two nonblocking ascending notes, coalescing/debounce, quiet cancellation, maintenance, failure and timer rollover PASS");
  reset(false);serviceMode=true;
  using brooklyn::hardware::observeService;
  using brooklyn::hardware::Sound;
  observeService(true,true,true);serviceKnobClick(0,true);
  observeService(false,true,true);serviceKnobClick(1,true);serviceKnobClick(60000,true);CHECK(attachCalls==0);
  serviceKnobClick(60001,true);CHECK(duty==64 && frequency==1300 && brooklyn::hardware::lastSound().sound==Sound::Lost);
  serviceKnobClick(60101,true);serviceKnobClick(60181,true);CHECK(frequency==1000 && duty==64);
  serviceKnobClick(60281,true);serviceKnobClick(60361,true);CHECK(frequency==700 && duty==64);
  serviceKnobClick(60461,true);serviceKnobClick(60541,true);serviceKnobClick(200000,true);CHECK(duty==0 && attachCalls==3);
  observeService(true,true,true);serviceKnobClick(200001,true);CHECK(duty==64 && frequency==700 && brooklyn::hardware::lastSound().sound==Sound::Restored);
  serviceKnobClick(200101,true);serviceKnobClick(200181,true);CHECK(frequency==1000 && duty==64);
  serviceKnobClick(200281,true);serviceKnobClick(200361,true);CHECK(frequency==1300 && duty==64);
  serviceKnobClick(200461,true);serviceKnobClick(200541,true);CHECK(duty==0 && attachCalls==6);
  observeService(false,true,true);serviceKnobClick(300000,true);serviceKnobClick(360000,true);CHECK(duty==64);
  brooklyn::hardware::setQuiet(true);serviceKnobClick(360001,true);CHECK(duty==0);
  observeService(true,true,true);brooklyn::hardware::setQuiet(false);serviceKnobClick(360002,true);CHECK(duty==0 && attachCalls==7);
  reset(false,0);serviceMode=true;observeService(true,true,true);serviceKnobClick(0,true);observeService(false,true,true);serviceKnobClick(1,true);serviceKnobClick(60001,true);
  CHECK(duty==0 && attachCalls==0);observeService(true,true,true);serviceKnobClick(60002,true);CHECK(attachCalls==0);
  reset(false);serviceMode=true;observeService(true,true,false);serviceKnobClick(0,true);observeService(false,true,false);serviceKnobClick(1,true);serviceKnobClick(60001,true);CHECK(attachCalls==0);
  puts("Service cue: exact descending/ascending three notes, 60s delay, no repeats, quiet cancellation, opt-out, PWM failure and sound telemetry PASS");
  puts("Startup: U23 pulses, four rising notes plus descending pair, 2265 ms, 50%-duty PWM, DIM mute, failed-check silence, PWM failure cleanup, addressable outputs disabled PASS");
}
