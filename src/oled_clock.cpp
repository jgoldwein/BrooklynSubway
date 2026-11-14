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
uint32_t requestedEnabled = 1;
uint32_t feedbackWord = 0, feedbackSequence = 0;
uint32_t requestedContrast = 207; // Accessed atomically; only the OLED worker touches I2C.
uint32_t statusWord = 0; // Single atomic packed snapshot; no bus access by readers.
void setStatus(State state, uint8_t error = 0) {
  __atomic_store_n(&statusWord, static_cast<uint32_t>(state) | (uint32_t(requestedAddress) << 8) |
      (uint32_t(address) << 16) | (uint32_t(error) << 24), __ATOMIC_RELAXED);
}
void draw(const char *clock) {
  screen.clearBuffer();
  screen.setFont(u8g2_font_6x12_tf);
  screen.drawStr((128 - screen.getStrWidth("Brooklyn Subway")) / 2, 22, "Brooklyn Subway");
  screen.setFont(u8g2_font_8x13_tf);
  screen.drawStr((128 - screen.getStrWidth(clock)) / 2, 47, clock);
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
    const uint8_t desired = __atomic_load_n(&requestedContrast, __ATOMIC_RELAXED);
    if (desired != appliedContrast) { screen.setContrast(desired); appliedContrast = desired; }
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
  uint8_t feedbackFrames = 0;
  for (;;) {
    char text[24]{};
    const bool valid = formatClock(time(nullptr), text, sizeof(text));
    if (valid && !synchronized) Serial.println("OLED clock: New York time synchronized (automatic DST)");
    synchronized = valid;
    const uint8_t desired = __atomic_load_n(&requestedContrast, __ATOMIC_RELAXED);
    if (desired != appliedContrast && !transportFailed) {
      screen.setContrast(desired); appliedContrast = desired;
      Serial.printf("OLED clock: contrast %u/255\n", desired);
    }
    const uint32_t feedback = __atomic_load_n(&feedbackWord, __ATOMIC_RELAXED);
    if (feedback != lastFeedback) {
      lastFeedback = feedback; feedbackFrames = feedback ? 3 : 0;
    }
    const bool desiredEnabled = __atomic_load_n(&requestedEnabled, __ATOMIC_RELAXED) != 0 || feedbackFrames != 0;
    if (!transportFailed && desiredEnabled != awake) {
      screen.setPowerSave(desiredEnabled ? 0 : 1);
      awake = desiredEnabled;
      Serial.println(awake ? "OLED clock: POWER display ON" : "OLED clock: POWER display OFF");
    }
    if (!transportFailed && awake) {
      if (feedbackFrames) {
        char level[24];
        snprintf(level, sizeof(level), "%u / %u", unsigned(feedback & 255), unsigned((feedback >> 8) & 255));
        const char *label = (feedback & (1u << 16)) ? "OLED brightness" : "LED brightness";
        screen.clearBuffer(); screen.setFont(u8g2_font_6x12_tf);
        screen.drawStr((128-screen.getStrWidth(label))/2,22,label);
        screen.setFont(u8g2_font_8x13_tf); screen.drawStr((128-screen.getStrWidth(level))/2,47,level);
        screen.sendBuffer(); --feedbackFrames;
      } else draw(text);
    }
    if (transportFailed) {
      setStatus(State::TransferFailed, transferError);
      Serial.println("OLED clock: transfer failed; OLED stopped, map continues");
      vTaskDelete(nullptr); return;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
}
void showBrightness(bool oled, uint8_t value, uint8_t maximum) {
  const uint32_t sequence = __atomic_add_fetch(&feedbackSequence, 1u, __ATOMIC_RELAXED);
  __atomic_store_n(&feedbackWord, (sequence << 18) | (1u << 17) | (uint32_t(oled) << 16) |
      (uint32_t(maximum) << 8) | value, __ATOMIC_RELAXED);
}
void clearBrightnessFeedback() { __atomic_store_n(&feedbackWord, 0u, __ATOMIC_RELAXED); }
void setEnabled(bool enabled) { __atomic_store_n(&requestedEnabled, uint32_t(enabled), __ATOMIC_RELAXED); }
void setContrast(uint8_t contrast) { __atomic_store_n(&requestedContrast, uint32_t(contrast), __ATOMIC_RELAXED); }
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
namespace brooklyn::oled { void begin(uint8_t, uint8_t) {} void setContrast(uint8_t) {} void setEnabled(bool) {} void showBrightness(bool, uint8_t, uint8_t) {} void clearBrightnessFeedback() {} Status status() { return {}; } }
#endif
