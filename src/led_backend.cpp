// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "led_backend.h"
#include "hardware.h"
#include "controls.h"

#include "board_pins.h"
#include "led_map.h"
#include "semantic_api.h"
#include "led_power.h"
#include "physical_output_safety.h"
#include "live_display.h"
#include "oled_clock.h"

#include <Arduino.h>
#include <string.h>
#include <time.h>

#if defined(BROOKLYN_TARGET_PCB)
#include <Adafruit_NeoPixel.h>

#if !defined(BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT)
#error "BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT must be explicitly defined"
#endif
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT != 0 && \
    BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT != 1
#error "BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT must be 0 or 1"
#endif

namespace {

using brooklyn::led::DisplayState;
using brooklyn::led::Rgb;
namespace safety = brooklyn::led::safety;

Adafruit_NeoPixel *productionStrips() {
  static Adafruit_NeoPixel strips[8] = {
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[0],
                        brooklyn::pins::LED_DATA[0],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[1],
                        brooklyn::pins::LED_DATA[1],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[2],
                        brooklyn::pins::LED_DATA[2],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[3],
                        brooklyn::pins::LED_DATA[3],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[4],
                        brooklyn::pins::LED_DATA[4],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[5],
                        brooklyn::pins::LED_DATA[5],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[6],
                        brooklyn::pins::LED_DATA[6],
                        NEO_GRB + NEO_KHZ800),
      Adafruit_NeoPixel(brooklyn::pins::LED_COUNT[7],
                        brooklyn::pins::LED_DATA[7],
                        NEO_GRB + NEO_KHZ800),
  };
  return strips;
}

bool initialized = false;
safety::Controller outputController;

void forceDataPinsLow() {
  for (uint8_t pin : brooklyn::pins::LED_DATA) {
    digitalWrite(pin, LOW);
  }
}

bool setEnable(void *, bool enabled) {
  if (enabled) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
    digitalWrite(brooklyn::pins::LED_ENABLE, HIGH);
    return true;
#else
    return false;
#endif
  }
  digitalWrite(brooklyn::pins::LED_ENABLE, LOW);
  return true;
}

bool initializeStrips(void *) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  initialized = false;
  auto *strips = productionStrips();
  for (uint8_t channel = 0; channel < brooklyn::led::map::CHANNEL_COUNT;
       ++channel) {
    if (strips[channel].getPixels() == nullptr) {
      Serial.printf("LED backend initialization failed: CH%u buffer allocation\n",
                    channel + 1);
      forceDataPinsLow();
      initialized = false;
      return false;
    }
  }
  for (uint8_t channel = 0; channel < brooklyn::led::map::CHANNEL_COUNT;
       ++channel) {
    strips[channel].begin();
    digitalWrite(brooklyn::pins::LED_DATA[channel], LOW);
  }
  forceDataPinsLow();
  initialized = true;
  return true;
#else
  return false;
#endif
}

bool setDataPinsLow(void *) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  forceDataPinsLow();
  return true;
#else
  return false;
#endif
}

bool prepareBlack(void *, const DisplayState *black, DisplayState *limited,
                  uint16_t count) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  if (black == nullptr || limited == nullptr ||
      count != brooklyn::led::map::TOTAL_LED_COUNT) {
    return false;
  }
  brooklyn::led::power::FramePowerReport report{};
  brooklyn::led::power::limitFrameToBudget(
      black, count, 255,
      brooklyn::led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP, limited,
      report);
  return safety::isBlackFrame(limited, count);
#else
  (void)black;
  (void)limited;
  (void)count;
  return false;
#endif
}

bool transmitOnHardware(void *, const DisplayState *frame, uint16_t count) {
#if BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1 || BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  if (!initialized || frame == nullptr ||
      count != brooklyn::led::map::TOTAL_LED_COUNT) {
    return false;
  }
  auto *strips = productionStrips();
  uint16_t start = 0;
  for (uint8_t channel = 0; channel < brooklyn::led::map::CHANNEL_COUNT;
       ++channel) {
    for (uint8_t index = 0;
         index < brooklyn::pins::LED_COUNT[channel]; ++index) {
      const Rgb color = frame[start + index].color;
      strips[channel].setPixelColor(index, color.red, color.green, color.blue);
    }
    strips[channel].show();
    digitalWrite(brooklyn::pins::LED_DATA[channel], LOW);
    start += brooklyn::pins::LED_COUNT[channel];
  }
  forceDataPinsLow();
  return true;
#else
  (void)frame;
  (void)count;
  return false;
#endif
}

const safety::Operations kOutputOperations = {
    nullptr, setEnable, initializeStrips, setDataPinsLow, prepareBlack,
    transmitOnHardware};

}  // namespace
#endif

namespace brooklyn::led::backend {
namespace {
bool diagnosticSystemReady = false;
}

void setDiagnosticSystemReady(bool ready) {
  diagnosticSystemReady = ready;
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  if (!ready) display::live::disable(display::IndicationEnd::OutputFailure);
#endif
  if (!ready) disarmLedOutputs();
}

namespace {
bool runDiagnostic(PhysicalChannel channel, bool walkChannel) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  display::live::DiagnosticGuard diagnosticGuard;
  display::live::disable(display::IndicationEnd::HardwareTest);
#endif
  const uint32_t durationMs = walkChannel ? 500 : 2000;
  const uint8_t channelNumber = static_cast<uint8_t>(channel);
  constexpr uint8_t kRed = 4;
  DisplayState frame[map::TOTAL_LED_COUNT]{};
  GlobalPhysicalIndex index{};
  map::PhysicalLed physical{};
  disarmLedOutputs();
  if (!diagnosticSystemReady) {
    Serial.println("LED diagnostic refused: startup checks have not passed");
    return false;
  }
  if (channelNumber < 1 || channelNumber > map::CHANNEL_COUNT) {
    Serial.println("LED diagnostic refused: invalid channel");
    return false;
  }
  const uint8_t channelIndex = channelNumber - 1;
  const uint8_t count = map::channelLedCount(channel);
  constexpr uint16_t kFirstDesignators[] = {42, 208, 226, 220, 190, 194, 198, 252};
  if (count != brooklyn::pins::LED_COUNT[channelIndex]) {
    Serial.println("LED diagnostic refused: channel count mismatch");
    return false;
  }
  const uint8_t pixelCount = walkChannel ? count : 1;
  // Validate the full requested physical range before enabling any outputs.
  for (uint8_t pixel = 0; pixel < pixelCount; ++pixel) {
    const bool isStatus = channel == PhysicalChannel::CH8 && pixel < 4;
    if (!map::toGlobalIndex({channel, pixel}, index) ||
        !map::physicalLed(index, physical) ||
        physical.address.channel != channel || physical.address.index != pixel ||
        physical.role != (isStatus ? LedRole::Status : LedRole::Map) ||
        (pixel == 0 && physical.designator.value != kFirstDesignators[channelIndex])) {
      Serial.println("LED diagnostic refused: physical mapping mismatch");
      return false;
    }
    if (isStatus) {
      PhysicalAddress expected{};
      BoardLedDesignator designator{};
      const StatusLed status = static_cast<StatusLed>(pixel);
      if (!map::statusLedAddress(status, expected) ||
          !map::statusLedDesignator(status, designator) ||
          expected.channel != channel || expected.index != pixel ||
          designator.value != physical.designator.value) {
        Serial.println("LED diagnostic refused: status mapping mismatch");
        return false;
      }
    }
  }
  Serial.printf("LED diagnostic: CH%u GPIO%u, %u pixel(s), red=4/255, %lu ms per pixel; GPIO1 enable; POWER aborts walk\n",
                channelNumber, brooklyn::pins::LED_DATA[channelIndex], pixelCount,
                static_cast<unsigned long>(durationMs));
  if (!outputController.initialize(kOutputOperations, true) ||
      !outputController.arm(kOutputOperations, true)) {
    disarmLedOutputs();
    Serial.println("LED diagnostic FAIL: black-first initialization/arming");
    return false;
  }
  bool lit = true;
  for (uint8_t pixel = 0; pixel < pixelCount; ++pixel) {
    if (walkChannel && (digitalRead(brooklyn::pins::BUTTON_POWER) == LOW ||
                        controls::pollWifiRecoveryDuringDiagnostic())) {
      Serial.println("LED diagnostic: aborted by POWER/recovery");
      lit = false;
      break;
    }
    if (!map::toGlobalIndex({channel, pixel}, index) ||
        !map::physicalLed(index, physical)) {
      lit = false;
      break;
    }
    Serial.printf("LED diagnostic: CH%u index %u LED%u\n", channelNumber, pixel,
                  physical.designator.value);
    if (channel == PhysicalChannel::CH8 && pixel < 4) {
      constexpr const char *kLabels[] = {"SYSTEM", "NETWORK", "MTA FEED", "MODE"};
      Serial.printf("LED diagnostic: status %s\n", kLabels[pixel]);
    }
    frame[index.value].color = {kRed, 0, 0};
    lit = outputController.transmit(kOutputOperations, true, frame,
                                     map::TOTAL_LED_COUNT);
    if (lit) delay(durationMs);
    // Previous pixel is black in the next transmitted frame.
    frame[index.value].color = {0, 0, 0};
    if (!lit) break;
  }
  const bool cleared = outputController.transmit(kOutputOperations, true, frame,
                                                map::TOTAL_LED_COUNT);
  disarmLedOutputs();
  Serial.printf("LED diagnostic %s; black sent=%s; GPIO1 LOW; visual confirmation required\n",
                lit && cleared ? "sequence complete" : "FAIL",
                cleared ? "yes" : "no");
  return lit && cleared;
#else
  (void)channel;
  (void)walkChannel;
  Serial.println("LED diagnostic unavailable on this target");
  return false;
#endif
}

}  // namespace

bool runFirstLedDiagnostic() { return runDiagnostic(PhysicalChannel::CH1, false); }
bool runChannelDiagnostic(PhysicalChannel channel) { return runDiagnostic(channel, true); }
bool runChannelOneDiagnostic() { return runChannelDiagnostic(PhysicalChannel::CH1); }

bool runAllChannelDiagnostics() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  display::live::DiagnosticGuard diagnosticGuard;
  display::live::disable(display::IndicationEnd::HardwareTest);
#endif
  disarmLedOutputs();
  if (!diagnosticSystemReady) return false;
  Serial.println("LED diagnostic: all-channel sequence starting; POWER aborts entire sequence");
  for (uint8_t number = 1; number <= map::CHANNEL_COUNT; ++number) {
    const auto channel = static_cast<PhysicalChannel>(number);
    if (!hardware::signalDiagnosticChannel(channel) || !runChannelDiagnostic(channel)) {
      disarmLedOutputs();
      Serial.printf("LED diagnostic: all-channel sequence stopped at CH%u; GPIO1 LOW\n", number);
      return false;
    }
  }
  disarmLedOutputs();
  Serial.println("LED diagnostic: all eight channels complete; GPIO1 LOW; visual confirmation required");
  return true;
#else
  return false;
#endif
}

bool runAllRouteDiagnostics(RouteDiagnosticSpeed speed, RouteDiagnosticStyle style) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  display::live::DiagnosticGuard diagnosticGuard;
  display::live::disable(display::IndicationEnd::HardwareTest);
#endif
  disarmLedOutputs();
  if (!diagnosticSystemReady || !semantic::selfCheck()) return false;
  DisplayState frame[map::TOTAL_LED_COUNT]{};
  struct OverlayGuard {~OverlayGuard(){oled::clearLineTest();}} overlayGuard;
  constexpr Rgb colors[] = {{4, 0, 0}, {0, 4, 0}, {0, 0, 4}};
  const uint16_t dwellMs = speed == RouteDiagnosticSpeed::Fast ? 25 : 250;
  const bool wholeLine=style==RouteDiagnosticStyle::WholeLine;
  const uint16_t wholeDwellMs=speed==RouteDiagnosticSpeed::Fast?200:750;
  const uint16_t gapMs=speed==RouteDiagnosticSpeed::Fast?100:250;
  const auto wait=[&](uint32_t ms) {
    while(ms) {
      if(digitalRead(pins::BUTTON_POWER)==LOW || controls::pollWifiRecoveryDuringDiagnostic()) return false;
      const uint32_t slice=ms<20?ms:20;delay(slice);ms-=slice;
    }
    return digitalRead(pins::BUTTON_POWER)!=LOW && !controls::pollWifiRecoveryDuringDiagnostic();
  };
  bool ok = true;
  uint16_t verifiedLights=0;
  for (uint8_t number = 1; number <= semantic::kRouteCount && ok; ++number) {
    const auto route = static_cast<semantic::Route>(number);
    uint16_t count=0;
    for(uint16_t i=0;i<map::TOTAL_LED_COUNT;++i) {const auto *m=semantic::semanticForLed(i);if(m && m->route==route) ++count;}
    if(!count) {ok=false;break;}
    Serial.printf("Route diagnostic: %s; %u mapped LEDs; %s\n",semantic::routeName(route),count,wholeLine?"complete-line RGB":"individual walk in PCB wiring order");
    Serial.printf("Route diagnostic: line %s; RGB 4/255; %u ms per colour; POWER aborts\n",
                  semantic::routeName(route), wholeLine ? wholeDwellMs : dwellMs);
    if (!outputController.initialize(kOutputOperations, true) ||
        !outputController.arm(kOutputOperations, true)) {
      disarmLedOutputs();
      return false;
    }
    if(wholeLine) {
      for(uint8_t phase=0;phase<3 && ok;++phase) {
        if(digitalRead(pins::BUTTON_POWER)==LOW || controls::pollWifiRecoveryDuringDiagnostic()) {ok=false;break;}
        for(uint16_t i=0;i<map::TOTAL_LED_COUNT;++i) {
          const auto *m=semantic::semanticForLed(i);frame[i].color=(m && m->route==route)?colors[phase]:Rgb{0,0,0};
        }
        oled::showLineTest(route,phase,count,count,false);
        if(power::estimateFrameCurrentMa(frame,map::TOTAL_LED_COUNT,255)>power::FIRMWARE_LED_CURRENT_BUDGET_MA) {ok=false;break;}
        ok=outputController.transmit(kOutputOperations,true,frame,map::TOTAL_LED_COUNT) && wait(wholeDwellMs);
      }
      for(auto &pixel:frame) pixel.color={0,0,0};
    } else {
    uint16_t position=0;
    for (uint16_t index = 0; index < map::TOTAL_LED_COUNT && ok; ++index) {
      const auto *mapped = semantic::semanticForLed(index);
      if (!mapped || mapped->route != route) continue;
      ++position;
      Serial.printf("Route diagnostic: line %s LED%u CH%u index %u\n",
                    semantic::routeName(route), mapped->designator,
                    mapped->channel, mapped->channelIndex);
      for (uint8_t phase=0;phase<3;++phase) {
        const auto &color=colors[phase];
        oled::showLineTest(route,phase,position,count,true);
        if (digitalRead(pins::BUTTON_POWER) == LOW ||
            controls::pollWifiRecoveryDuringDiagnostic()) { ok = false; break; }
        frame[index].color = color;
        ok = outputController.transmit(kOutputOperations, true, frame, map::TOTAL_LED_COUNT);
        if (ok) delay(dwellMs);
        frame[index].color = {0, 0, 0};
        if (!ok) break;
      }
      // Clear the previous physical LED before advancing across channels.
      if (ok) ok = outputController.transmit(kOutputOperations, true, frame,
                                             map::TOTAL_LED_COUNT);
    }
    }
    const bool cleared = outputController.transmit(kOutputOperations, true, frame, map::TOTAL_LED_COUNT);
    disarmLedOutputs();
    ok = ok && cleared;
    if(ok) {verifiedLights+=count;if(wholeLine) ok=wait(gapMs);}
  }
  ok=ok && verifiedLights==semantic::kMapLedCount;
  Serial.printf("Route diagnostic coverage: %u / %u mapped LEDs, status LEDs excluded\n",verifiedLights,unsigned(semantic::kMapLedCount));
  Serial.printf("Route diagnostic %s; GPIO1 LOW; visual confirmation required\n",
                ok ? "complete" : "stopped/failed");
  return ok;
#else
  (void)speed;(void)style;
  return false;
#endif
}

bool runEasterEgg(eggs::Kind kind) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LED_DIAGNOSTIC == 1
  if(!diagnosticSystemReady || !semantic::selfCheck() || !eggs::duration(kind)) return false;
#if BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  const uint8_t powerMode=display::live::stats().powerMode;
  const bool lightsAllowed=powerMode==0 || powerMode==2;
  display::live::DiagnosticGuard diagnosticGuard;
  display::live::disable(display::IndicationEnd::HardwareTest);
#else
  const bool lightsAllowed=false;
#endif
  disarmLedOutputs();
  DisplayState frame[map::TOTAL_LED_COUNT]{};
  struct Cleanup {
    DisplayState (&frame)[map::TOTAL_LED_COUNT];
    ~Cleanup(){
      hardware::setEasterEggTone(0);
      for(auto &pixel:frame) pixel.color={0,0,0};
      if(physicalLedOutputsArmed()) (void)outputController.transmit(kOutputOperations,true,frame,map::TOTAL_LED_COUNT);
      disarmLedOutputs();oled::clearEasterEgg();
    }
  } cleanup{frame};
  oled::ScreenText previousText{};
  const uint32_t started=millis();uint32_t previousFrame=UINT32_MAX;
  bool armed=false;
  Serial.printf("Easter egg: %u; duration %lu ms; dim map cap4; POWER cancels\n",unsigned(kind),static_cast<unsigned long>(eggs::duration(kind)));
  while(static_cast<uint32_t>(millis()-started)<eggs::duration(kind)) {
    if(digitalRead(pins::BUTTON_POWER)==LOW || controls::pollWifiRecoveryDuringDiagnostic()) return false;
    const uint32_t elapsed=millis()-started;
    eggs::Sample sample;
    if(!eggs::sample(kind,elapsed,sample)) break;
    const bool quiet=oled::quietHoursActive(time(nullptr));
    const uint8_t quietCap=quiet?oled::quietMapBrightness():255;
    hardware::setQuiet(quiet,quietCap);
    hardware::setEasterEggTone(sample.frequency);
    if(memcmp(&previousText,&sample.text,sizeof(previousText))!=0) {
      previousText=sample.text;oled::showEasterEgg(sample.text);
    }
    if(elapsed/40!=previousFrame) {
      previousFrame=elapsed/40;
      const uint8_t cap=quietCap<4?quietCap:4;
      const uint16_t chaser=sample.position%semantic::kMapLedCount;
      uint16_t mappedPosition=0;
      for(uint16_t i=0;i<map::TOTAL_LED_COUNT;++i) {
        const auto *mapped=semantic::semanticForLed(i);
        const bool lit=mapped && (sample.visual==eggs::Visual::Line?mapped->route==sample.route:
            sample.visual==eggs::Visual::Chaser && mappedPosition==chaser);
        frame[i].color=lit?sample.color:Rgb{0,0,0};
        if(frame[i].color.red>cap) frame[i].color.red=cap;
        if(frame[i].color.green>cap) frame[i].color.green=cap;
        if(frame[i].color.blue>cap) frame[i].color.blue=cap;
        if(mapped) ++mappedPosition;
      }
      const bool wantsLights=lightsAllowed && cap && sample.visual!=eggs::Visual::None;
      if(wantsLights && !armed) {
        if(!outputController.initialize(kOutputOperations,true) || !outputController.arm(kOutputOperations,true)) return false;
        armed=true;
      }
      if(armed) {
        if(!wantsLights) {
          for(auto &pixel:frame) pixel.color={0,0,0};
          if(!outputController.transmit(kOutputOperations,true,frame,map::TOTAL_LED_COUNT)) return false;
          disarmLedOutputs();armed=false;
        }
        else if(power::estimateFrameCurrentMa(frame,map::TOTAL_LED_COUNT,255)>power::FIRMWARE_LED_CURRENT_BUDGET_MA ||
                !outputController.transmit(kOutputOperations,true,frame,map::TOTAL_LED_COUNT)) return false;
      }
    }
    delay(20);
  }
  if(armed) {
    for(auto &pixel:frame) pixel.color={0,0,0};
    if(!outputController.transmit(kOutputOperations,true,frame,map::TOTAL_LED_COUNT)) return false;
  }
  disarmLedOutputs();
  Serial.println("Easter egg: complete; buzzer/OLED overlay/map cleared");
  return true;
#else
  (void)kind;return false;
#endif
}

bool armLiveOutput() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  return diagnosticSystemReady && semantic::selfCheck() && outputController.initialize(kOutputOperations, true) &&
      outputController.arm(kOutputOperations, true);
#else
  return false;
#endif
}

bool transmitLiveOutput(const DisplayState *frame, uint16_t count) {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  if (!diagnosticSystemReady || !frame || count != map::TOTAL_LED_COUNT) return false;
  // Reject any caller that bypasses the physical cap or total-current budget.
  for (uint16_t i = 0; i < count; ++i) {
    const auto color = frame[i].color;
    if (color.red > power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP ||
        color.green > power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP ||
        color.blue > power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP) return false;
  }
  if (power::estimateFrameCurrentMa(frame, count, 255) > power::FIRMWARE_LED_CURRENT_BUDGET_MA) return false;
  return outputController.transmit(kOutputOperations, true, frame, count);
#else
  (void)frame; (void)count;
  return false;
#endif
}

void stopLiveOutput() {
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
  DisplayState black[map::TOTAL_LED_COUNT]{};
  if (physicalLedOutputsArmed())
    outputController.transmit(kOutputOperations, true, black, map::TOTAL_LED_COUNT);
  disarmLedOutputs();
#endif
}

bool initializePhysicalLedBackend() {
#if defined(BROOKLYN_TARGET_PCB)
  const bool ready = outputController.initialize(
      kOutputOperations, physicalLedOutputPermissionEnabled());
  if (ready) {
    Serial.println(
        "Physical LED backend initialized; post-enable black required.");
  }
  return ready;
#else
  return false;
#endif
}

bool armLedOutputs() {
#if defined(BROOKLYN_TARGET_PCB)
  const bool armed = outputController.arm(
      kOutputOperations, physicalLedOutputPermissionEnabled());
  if (!armed) {
    Serial.println("LED output arm refused; black-first interlock not satisfied.");
  } else {
    Serial.println("Physical LED outputs ARMED after post-enable black frame.");
  }
  return armed;
#else
  Serial.println("LED output arm refused: XIAO is software-simulation only.");
  return false;
#endif
}

void disarmLedOutputs() {
#if defined(BROOKLYN_TARGET_PCB)
  outputController.disarm(kOutputOperations);
#endif
}

bool physicalLedOutputsArmed() {
#if defined(BROOKLYN_TARGET_PCB)
  return outputController.state() == safety::State::Armed;
#else
  return false;
#endif
}

bool physicalLedBackendInitialized() {
#if defined(BROOKLYN_TARGET_PCB)
  return initialized;
#else
  return false;
#endif
}

bool physicalLedOutputPermissionEnabled() {
#if defined(BROOKLYN_TARGET_PCB)
  return BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT == 1;
#else
  return false;
#endif
}

bool transmitFrame(const DisplayState *frame, uint16_t count) {
#if defined(BROOKLYN_TARGET_PCB)
  const bool transmitted = outputController.transmit(
      kOutputOperations, physicalLedOutputPermissionEnabled(), frame, count);
  if (!transmitted && outputController.state() == safety::State::Fault) {
    Serial.println("LED transmission fault; outputs disabled.");
  }
  return transmitted;
#else
  (void)frame;
  (void)count;
  return false;
#endif
}

}  // namespace brooklyn::led::backend
