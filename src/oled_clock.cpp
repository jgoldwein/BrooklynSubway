#include "oled_clock.h"
#if defined(BROOKLYN_TARGET_PCB)
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <stdio.h>
namespace brooklyn::oled {
namespace {
// Wire already owns GPIO8/9. Passing those pins to U8g2 makes its GPIO
// initializer call pinMode(OUTPUT), disrupting the active hardware I2C bus.
// Our byte callback reuses Wire, so U8g2 must not initialize any GPIO pins.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen(U8G2_R0, U8X8_PIN_NONE);
TaskHandle_t clockTask = nullptr;
uint8_t address = 0, requestedAddress = 0, transferError = 0;
uint32_t trackedTrainsWord = 0;
uint32_t visibleMapLights = 0;
uint32_t requestedEnabled = 1;
uint32_t setupMode = 0;
Navigation navigation;
uint32_t returnSeconds = 60;
portMUX_TYPE screenDataMux = portMUX_INITIALIZER_UNLOCKED;
ScreenData screenData{};
LineTestView lineTest{};
uint32_t feedbackWord = 0, feedbackSequence = 0;
uint32_t requestedContrast = 207; // Accessed atomically; only the OLED worker touches I2C.
uint32_t uiRevision = 0;
uint64_t screenPreferencesWord=config::kDefaultScreenPreferences;
uint64_t readScreenPreferences() {
  portENTER_CRITICAL(&screenDataMux);const uint64_t word=screenPreferencesWord;portEXIT_CRITICAL(&screenDataMux);return word;
}
void requestRefresh() { __atomic_add_fetch(&uiRevision,1u,__ATOMIC_RELAXED); }
uint32_t statusWord = 0; // Single atomic packed snapshot; no bus access by readers.
void setStatus(State state, uint8_t error = 0) {
  __atomic_store_n(&statusWord, static_cast<uint32_t>(state) | (uint32_t(requestedAddress) << 8) |
      (uint32_t(address) << 16) | (uint32_t(error) << 24), __ATOMIC_RELAXED);
}
void draw(const char *clock) {
  screen.clearBuffer();
  if (__atomic_load_n(&setupMode, __ATOMIC_RELAXED)) {
    screen.setFont(u8g2_font_6x12_tf);
    screen.drawStr((128-screen.getStrWidth("Wi-Fi setup"))/2, 12, "Wi-Fi setup");
    screen.drawStr((128-screen.getStrWidth("Join BrooklynMap-Avi"))/2, 28, "Join BrooklynMap-Avi");
    screen.drawStr((128-screen.getStrWidth("Open in browser:"))/2, 44, "Open in browser:");
    screen.setFont(u8g2_font_8x13_tf);
    screen.drawStr((128-screen.getStrWidth("192.168.4.1"))/2, 60, "192.168.4.1");
    screen.sendBuffer();
    return;
  }
  LineTestView diagnostic;
  portENTER_CRITICAL(&screenDataMux);diagnostic=lineTest;portEXIT_CRITICAL(&screenDataMux);
  if(diagnostic.active) {
    ScreenText text;formatLineTest(diagnostic,text);screen.setFont(u8g2_font_6x12_tf);
    for(uint8_t i=0;i<5;++i) screen.drawStr(0,11+i*12,text.lines[i]);
    screen.sendBuffer();return;
  }
  NavigationView view;
  portENTER_CRITICAL(&screenDataMux);
  view = navigation.view();
  portEXIT_CRITICAL(&screenDataMux);
  const Screen selected = view.screen;
  if (selected != Screen::Main) {
    ScreenData data;
    portENTER_CRITICAL(&screenDataMux);
    data = screenData;
    portEXIT_CRITICAL(&screenDataMux);
    ScreenText text;
    formatScreen(selected, data, millis(), text, view);
    screen.setFont(u8g2_font_6x12_tf);
    for (uint8_t i = 0; i < 5; ++i) {
      const int x = selected==Screen::Dedication && i<4 ? (128-screen.getStrWidth(text.lines[i]))/2 : 0;
      screen.drawStr(x, 11+i*12, text.lines[i]);
    }
    screen.sendBuffer();
    return;
  }
  screen.setFont(u8g2_font_6x12_tf);
  screen.drawStr((128 - screen.getStrWidth("Brooklyn Subway 1/6")) / 2, 16, "Brooklyn Subway 1/6");
  const uint32_t trains = __atomic_load_n(&trackedTrainsWord, __ATOMIC_RELAXED);
  char count[24];
  if (trains & 0x80000000u) snprintf(count, sizeof(count), "Trains:%lu Lit:%u", static_cast<unsigned long>(trains & 0x7fffffffu), unsigned(__atomic_load_n(&visibleMapLights,__ATOMIC_RELAXED)));
  else snprintf(count, sizeof(count), "Trains:-- Lit:%u", unsigned(__atomic_load_n(&visibleMapLights,__ATOMIC_RELAXED)));
  screen.drawStr((128 - screen.getStrWidth(count)) / 2, 34, count);
  screen.setFont(u8g2_font_8x13_tf);
  screen.drawStr((128 - screen.getStrWidth(clock)) / 2, 55, clock);
  screen.drawFrame(0,0,128,64);
  screen.sendBuffer();
}
// The board initializes Wire once. Avoid U8g2's second Wire.begin() and
// stop issuing transfers after the first failure (including framebuffer writes).
bool transportFailed = false;
uint8_t boundedI2c(u8x8_t *display, uint8_t message, uint8_t length, void *bytes) {
  if (transportFailed) return 0;
  switch (message) {
    case U8X8_MSG_BYTE_INIT: return 1;
    case U8X8_MSG_BYTE_SET_DC: return 1;
    case U8X8_MSG_BYTE_START_TRANSFER:
      Wire.beginTransmission(u8x8_GetI2CAddress(display) >> 1);
      return 1;
    case U8X8_MSG_BYTE_SEND:
      if (Wire.write(static_cast<uint8_t *>(bytes), length) != length) {
        (void)Wire.endTransmission(); // Release the Wire transaction before latching failure.
        Serial.printf("OLED clock: short I2C write, requested %u bytes\n", length);
        transferError = 1; transportFailed = true;
      }
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      {
        const uint8_t error = Wire.endTransmission();
        if (error != 0) {
          Serial.printf("OLED clock: I2C transfer error %u at 0x%02X (2=NACK, 4=other, 5=timeout)\n", error, address);
          transferError = error; transportFailed = true;
        }
      }
      break;
    default: return 0;
  }
  return transportFailed ? 0 : 1;
}
void run(void *) {
  Serial.printf("OLED clock: worker starting, SDA GPIO%u SCL GPIO%u\n", pins::I2C_SDA, pins::I2C_SCL);
  Wire.setTimeOut(50);
  Wire.setClock(100000);
  if (digitalRead(pins::I2C_SDA) == LOW || digitalRead(pins::I2C_SCL) == LOW) {
    setStatus(State::BusLow);
    Serial.println("OLED clock: SDA/SCL held LOW; OLED skipped, map continues");
    vTaskDelete(nullptr); return;
  }
  // Restore the working driver's fallback, trying the saved address first.
  // A second bounded pass allows a late-ready module to acknowledge.
  const uint8_t candidates[] = {requestedAddress, 0x38, 0x3c, 0x3d};
  uint8_t lastError = 0;
  for (uint8_t pass = 0; pass < 2 && !address; ++pass) {
    for (uint8_t i = 0; i < sizeof(candidates); ++i) {
      bool duplicate = false;
      for (uint8_t earlier = 0; earlier < i; ++earlier)
        if (candidates[earlier] == candidates[i]) duplicate = true;
      if (duplicate) continue;
      Wire.beginTransmission(candidates[i]);
      lastError = Wire.endTransmission();
      Serial.printf("OLED clock: probe 0x%02X -> %u (0=ACK, 2=NACK, 5=timeout)\n", candidates[i], lastError);
      if (!lastError) { address = candidates[i]; break; }
    }
    if (!address && pass == 0) delay(100);
  }
  if (!address) {
    setStatus(State::NoAck, lastError);
    Serial.println("OLED clock: no OLED address acknowledged; map continues");
    vTaskDelete(nullptr); return;
  }
  setStatus(State::Starting);
  Serial.printf("OLED clock: address 0x%02X acknowledged; initializing SSD1306 128x64\n", address);
  screen.setI2CAddress(address * 2);
  screen.setBusClock(100000);
  screen.getU8x8()->byte_cb = boundedI2c;
  screen.begin();
  uint8_t appliedContrast = 207;
  if (!transportFailed) {
    uint8_t desired = __atomic_load_n(&requestedContrast, __ATOMIC_RELAXED);
    if(quietHoursActive(time(nullptr))) {
      const uint8_t cap=config::unpackScreenPreferences(readScreenPreferences()).quietContrast;
      if(desired>cap) desired=cap;
    }
    if (desired != appliedContrast) { screen.setContrast(desired); appliedContrast = desired; }
  }
  if(!transportFailed && !__atomic_load_n(&setupMode,__ATOMIC_RELAXED) &&
      (readScreenPreferences()&config::kStartupDedicationBit)) {
    const bool wait=config::unpackScreenPreferences(readScreenPreferences()).startupUntilWifi;
    portENTER_CRITICAL(&screenDataMux);navigation.startDedication(millis(),wait);portEXIT_CRITICAL(&screenDataMux);
    requestRefresh();Serial.println(wait?"OLED: startup dedication until Wi-Fi has an IP, minimum 5 seconds; turn to skip":"OLED: five-second startup dedication; turn knob to skip");
  }
  if (!transportFailed) draw("Syncing time...");
  if (transportFailed) {
    setStatus(State::InitFailed, transferError);
    Serial.println("OLED clock: initialization transfer failed; OLED stopped, map continues");
    vTaskDelete(nullptr); return;
  }
  setStatus(State::Ready);
  Serial.println("OLED clock: initialization and first frame complete");
  bool synchronized = false;
  bool awake = true;
  uint32_t lastFeedback = 0;
  bool feedbackActive = false, firstRefresh = true;
  uint32_t feedbackAtMs = 0, lastRenderAtMs = 0, lastRevision = 0;
  for (;;) {
    const uint32_t nowMs = millis();
    const uint32_t revision = __atomic_load_n(&uiRevision,__ATOMIC_RELAXED);
    bool redraw=firstRefresh || revision!=lastRevision || static_cast<uint32_t>(nowMs-lastRenderAtMs)>=1000;
    char text[24]{};
    const bool valid = formatClock(time(nullptr), text, sizeof(text));
    if (valid && !synchronized) Serial.println("OLED clock: New York time synchronized (automatic DST)");
    synchronized = valid;
    uint8_t desired = __atomic_load_n(&requestedContrast, __ATOMIC_RELAXED);
    if(quietHoursActive(time(nullptr))) {
      const uint8_t cap=config::unpackScreenPreferences(readScreenPreferences()).quietContrast;
      if(desired>cap) desired=cap;
    }
    if (desired != appliedContrast && !transportFailed) {
      screen.setContrast(desired); appliedContrast = desired;
      redraw=true;
      Serial.printf("OLED clock: contrast %u/255\n", desired);
    }
    const uint32_t feedback = __atomic_load_n(&feedbackWord, __ATOMIC_RELAXED);
    if (feedback != lastFeedback) {
      lastFeedback = feedback; feedbackActive=feedback!=0;feedbackAtMs=nowMs;redraw=true;
    }
    if(feedbackActive && static_cast<uint32_t>(nowMs-feedbackAtMs)>=3000) {feedbackActive=false;redraw=true;}
    const bool desiredEnabled = __atomic_load_n(&requestedEnabled, __ATOMIC_RELAXED) != 0 || feedbackActive;
    if (!transportFailed && desiredEnabled != awake) {
      screen.setPowerSave(desiredEnabled ? 0 : 1);
      awake = desiredEnabled;
      redraw=true;
      Serial.println(awake ? "OLED clock: POWER display ON" : "OLED clock: POWER display OFF");
    }
    if (!transportFailed && awake && redraw) {
      if (feedbackActive) {
        char level[24];
        snprintf(level, sizeof(level), "%u / %u", unsigned(feedback & 255), unsigned((feedback >> 8) & 255));
        const char *label = (feedback & (1u << 16)) ? "OLED brightness" : "LED brightness";
        screen.clearBuffer(); screen.setFont(u8g2_font_6x12_tf);
        screen.drawStr((128-screen.getStrWidth(label))/2,22,label);
        screen.setFont(u8g2_font_8x13_tf); screen.drawStr((128-screen.getStrWidth(level))/2,47,level);
        screen.sendBuffer();
      } else draw(text);
    }
    if(redraw) {lastRevision=revision;lastRenderAtMs=nowMs;firstRefresh=false;}
    if (transportFailed) {
      setStatus(State::TransferFailed, transferError);
      Serial.println("OLED clock: transfer failed; OLED stopped, map continues");
      vTaskDelete(nullptr); return;
    }
    // Notice input promptly, but keep idle clock/data redraws at one per second.
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}
void setScreenPreferences(uint64_t word) {
  if(!config::validScreenPreferences(word)) return;
  portENTER_CRITICAL(&screenDataMux);const bool changed=screenPreferencesWord!=word;screenPreferencesWord=word;portEXIT_CRITICAL(&screenDataMux);
  if(changed) requestRefresh();
}
bool quietHoursActive(time_t utc) {
  const uint64_t word=readScreenPreferences();
  if(!(word&1)) return false;
  uint16_t minute=0;const bool known=newYorkMinutes(utc,minute);
  return config::quietActive(word,known,minute);
}
uint8_t quietMapBrightness() {return config::unpackScreenPreferences(readScreenPreferences()).quietMapBrightness;}
void publishScreenData(const ScreenData &data) {
  portENTER_CRITICAL(&screenDataMux);
  screenData = data;
  portEXIT_CRITICAL(&screenDataMux);
  requestRefresh();
}
void showLineTest(semantic::Route route,uint8_t color,uint16_t current,uint16_t total,bool individual) {
  LineTestView view;view.route=route;view.color=color;view.current=current;view.total=total;view.individual=individual;view.active=true;
  portENTER_CRITICAL(&screenDataMux);lineTest=view;portEXIT_CRITICAL(&screenDataMux);
  clearBrightnessFeedback();requestRefresh();
}
void clearLineTest() {
  portENTER_CRITICAL(&screenDataMux);lineTest={};portEXIT_CRITICAL(&screenDataMux);requestRefresh();
}
Screen selectedScreen() {
  portENTER_CRITICAL(&screenDataMux);
  const auto selected = navigation.view().screen;
  portEXIT_CRITICAL(&screenDataMux);
  return selected;
}
void advanceScreen() {
  navigate(Action::NextScreen, millis());
}
void navigate(Action action, uint32_t nowMs) {
  if (__atomic_load_n(&setupMode, __ATOMIC_RELAXED)) return;
  portENTER_CRITICAL(&screenDataMux);
  const bool changed = navigation.input(action, nowMs);
  const auto view = navigation.view();
  portEXIT_CRITICAL(&screenDataMux);
  if(changed) {requestRefresh();Serial.printf("OLED screen: %s (%u/%u), line %s %s\n", screenName(view.screen), unsigned(view.screen)+1, unsigned(kScreenCount),
      semantic::routeName(static_cast<semantic::Route>(view.line)),view.automatic?"Auto":"Paused");
  }
}
void serviceNavigation(uint32_t nowMs, bool active) {
  const bool usable=active && !__atomic_load_n(&setupMode,__ATOMIC_RELAXED);
  const uint16_t timeout=__atomic_load_n(&returnSeconds,__ATOMIC_RELAXED);
  portENTER_CRITICAL(&screenDataMux);
  navigation.setReturnSeconds(timeout,nowMs);
  const bool changed=navigation.update(nowMs,usable,screenData.wifiConnected);
  const auto view=navigation.view();
  portEXIT_CRITICAL(&screenDataMux);
  if(changed) {requestRefresh();Serial.printf("OLED navigation: %s, line %s %s\n",screenName(view.screen),
      semantic::routeName(static_cast<semantic::Route>(view.line)),view.automatic?"Auto":"Paused");
  }
}
void setReturnSeconds(uint16_t seconds) { __atomic_store_n(&returnSeconds,uint32_t(seconds),__ATOMIC_RELAXED); }
void setSetupMode(bool enabled) {
  const uint32_t value = enabled ? 1u : 0u;
  if (__atomic_exchange_n(&setupMode, value, __ATOMIC_RELAXED) != value) {
    requestRefresh();
    Serial.println(enabled ? "OLED clock: setup instructions, join BrooklynMap-Avi then open 192.168.4.1" : "OLED clock: normal clock display resumed");
  }
}
void setVisibleMapLights(uint16_t count) { if(__atomic_exchange_n(&visibleMapLights,uint32_t(count),__ATOMIC_RELAXED)!=count) requestRefresh(); }
void setTrackedTrains(uint32_t count, bool available) {
  const uint32_t value = available ? (0x80000000u | (count & 0x7fffffffu)) : 0;
  const uint32_t previous = __atomic_exchange_n(&trackedTrainsWord, value, __ATOMIC_RELAXED);
  if (previous != value) {
    requestRefresh();
    if (available) Serial.printf("OLED clock: tracked trains %lu\n", static_cast<unsigned long>(count));
    else Serial.println("OLED clock: tracked count unavailable; waiting for current data");
  }
}
void showBrightness(bool oled, uint8_t value, uint8_t maximum) {
  const uint32_t sequence = __atomic_add_fetch(&feedbackSequence, 1u, __ATOMIC_RELAXED);
  __atomic_store_n(&feedbackWord, (sequence << 18) | (1u << 17) | (uint32_t(oled) << 16) |
      (uint32_t(maximum) << 8) | value, __ATOMIC_RELAXED);
  requestRefresh();
}
void clearBrightnessFeedback() { if(__atomic_exchange_n(&feedbackWord, 0u, __ATOMIC_RELAXED)) requestRefresh(); }
void setEnabled(bool enabled) { if(__atomic_exchange_n(&requestedEnabled, uint32_t(enabled), __ATOMIC_RELAXED)!=uint32_t(enabled)) requestRefresh(); }
void setContrast(uint8_t contrast) { if(__atomic_exchange_n(&requestedContrast, uint32_t(contrast), __ATOMIC_RELAXED)!=contrast) requestRefresh(); }
void begin(uint8_t configuredAddress, uint8_t contrast) {
  if (clockTask) return;
  setContrast(contrast);
  requestedAddress = configuredAddress;
  if (configuredAddress < 0x08 || configuredAddress > 0x77) {
    setStatus(State::InvalidAddress);
    Serial.println("OLED clock: invalid configured address; skipped"); return;
  }
  address = 0;
  setStatus(State::Starting);
  // No I2C or display initialization on the application startup/loop task.
  if (xTaskCreatePinnedToCore(run, "oled-clock", 4096, nullptr, 1, &clockTask, 1) != pdPASS) {
    setStatus(State::TaskFailed);
    Serial.println("OLED clock: task allocation failed; map continues without OLED");
  }
}
Status status() {
  const uint32_t value = __atomic_load_n(&statusWord, __ATOMIC_RELAXED);
  return {static_cast<State>(value & 255), static_cast<uint8_t>(value >> 8),
      static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
}
}
#else
namespace brooklyn::oled { void begin(uint8_t, uint8_t) {} void setContrast(uint8_t) {} void setEnabled(bool) {} void setSetupMode(bool) {} void setScreenPreferences(uint64_t) {} bool quietHoursActive(time_t) {return false;} uint8_t quietMapBrightness() {return 255;} void publishScreenData(const ScreenData &) {} void showLineTest(semantic::Route,uint8_t,uint16_t,uint16_t,bool) {} void clearLineTest() {} Screen selectedScreen() { return Screen::Main; } void advanceScreen() {} void navigate(Action,uint32_t) {} void serviceNavigation(uint32_t,bool) {} void setReturnSeconds(uint16_t) {} void showBrightness(bool, uint8_t, uint8_t) {} void clearBrightnessFeedback() {} void setTrackedTrains(uint32_t, bool) {} void setVisibleMapLights(uint16_t) {} Status status() { return {}; } }
#endif
