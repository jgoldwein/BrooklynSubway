#include "oled_clock.h"
#if defined(BROOKLYN_TARGET_PCB)
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
namespace brooklyn::oled {
namespace {
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C screen(U8G2_R0, U8X8_PIN_NONE,
    pins::I2C_SCL, pins::I2C_SDA);
TaskHandle_t clockTask = nullptr;
uint8_t address = 0;
void draw(const char *clock) {
  screen.clearBuffer();
  screen.setFont(u8g2_font_6x12_tf);
  screen.drawStr((128 - screen.getStrWidth("Brooklyn Subway")) / 2, 22, "Brooklyn Subway");
  screen.setFont(u8g2_font_8x13_tf);
  screen.drawStr((128 - screen.getStrWidth(clock)) / 2, 47, clock);
  screen.sendBuffer();
}
void run(void *) {
  bool synchronized = false;
  for (;;) {
    char text[24]{};
    const bool valid = formatClock(time(nullptr), text, sizeof(text));
    if (valid && !synchronized) Serial.println("OLED clock: New York time synchronized (automatic DST)");
    synchronized = valid;
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) {
      Serial.println("OLED clock: display disconnected; stopping OLED task");
      vTaskDelete(nullptr);
      return;
    }
    draw(text);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
}
void begin() {
  if (clockTask) return;
  Wire.setTimeOut(50);
  for (uint8_t candidate : {uint8_t(0x3c), uint8_t(0x3d)}) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) { address = candidate; break; }
  }
  if (!address) { Serial.println("OLED clock: SSD1309 absent at 0x3C/0x3D; skipped"); return; }
  screen.setI2CAddress(address * 2);
  screen.setBusClock(100000);
  screen.begin();
  draw("Syncing time...");
  Serial.printf("OLED clock: SSD1309 128x64 at 0x%02X, SDA GPIO%u SCL GPIO%u\n",
      address, pins::I2C_SDA, pins::I2C_SCL);
  if (xTaskCreatePinnedToCore(run, "oled-clock", 4096, nullptr, 1, &clockTask, 1) != pdPASS)
    Serial.println("OLED clock: task allocation failed; startup text retained");
}
}
#else
namespace brooklyn::oled { void begin() {} }
#endif
