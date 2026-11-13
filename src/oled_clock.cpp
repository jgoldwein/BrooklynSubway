#include "oled_clock.h"
#if defined(BROOKLYN_TARGET_PCB)
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
namespace brooklyn::oled {
namespace {
// Wire already owns GPIO8/9. Passing those pins to U8g2 makes its GPIO
// initializer call pinMode(OUTPUT), disrupting the active hardware I2C bus.
// Our byte callback reuses Wire, so U8g2 must not initialize any GPIO pins.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen(U8G2_R0, U8X8_PIN_NONE);
TaskHandle_t clockTask = nullptr;
uint8_t address = 0; // Set once from validated saved configuration before the worker starts.
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
        transportFailed = true;
      }
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      {
        const uint8_t error = Wire.endTransmission();
        if (error != 0) {
          Serial.printf("OLED clock: I2C transfer error %u at 0x%02X (2=NACK, 4=other, 5=timeout)\n", error, address);
          transportFailed = true;
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
    Serial.println("OLED clock: SDA/SCL held LOW; OLED skipped, map continues");
    vTaskDelete(nullptr); return;
  }
  Wire.beginTransmission(address);
  const uint8_t error = Wire.endTransmission();
  Serial.printf("OLED clock: configured address 0x%02X -> %u (0=ACK, 2=NACK, 5=timeout)\n", address, error);
  if (error != 0) {
    Serial.println("OLED clock: configured address did not respond; map continues");
    vTaskDelete(nullptr); return;
  }
  Serial.printf("OLED clock: address 0x%02X acknowledged; initializing SSD1306 128x64\n", address);
  screen.setI2CAddress(address * 2);
  screen.setBusClock(100000);
  screen.getU8x8()->byte_cb = boundedI2c;
  screen.begin();
  if (!transportFailed) draw("Syncing time...");
  if (transportFailed) {
    Serial.println("OLED clock: initialization transfer failed; OLED stopped, map continues");
    vTaskDelete(nullptr); return;
  }
  Serial.println("OLED clock: initialization and first frame complete");
  bool synchronized = false;
  for (;;) {
    char text[24]{};
    const bool valid = formatClock(time(nullptr), text, sizeof(text));
    if (valid && !synchronized) Serial.println("OLED clock: New York time synchronized (automatic DST)");
    synchronized = valid;
    draw(text);
    if (transportFailed) {
      Serial.println("OLED clock: transfer failed; OLED stopped, map continues");
      vTaskDelete(nullptr); return;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
}
void begin(uint8_t configuredAddress) {
  if (clockTask) return;
  if (configuredAddress < 0x08 || configuredAddress > 0x77) {
    Serial.println("OLED clock: invalid configured address; skipped"); return;
  }
  address = configuredAddress;
  // No I2C or display initialization on the application startup/loop task.
  if (xTaskCreatePinnedToCore(run, "oled-clock", 4096, nullptr, 1, &clockTask, 1) != pdPASS)
    Serial.println("OLED clock: task allocation failed; map continues without OLED");
}
}
#else
namespace brooklyn::oled { void begin(uint8_t) {} }
#endif
