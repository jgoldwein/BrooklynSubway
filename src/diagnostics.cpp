#include "diagnostics.h"

#include "board_pins.h"

#include <Arduino.h>
#include <esp_system.h>

namespace {

uint32_t lastHeartbeatMs = 0;

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "power-on";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt-watchdog";
    case ESP_RST_TASK_WDT:
      return "task-watchdog";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep-sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "other";
  }
}

}  // namespace

namespace brooklyn::diagnostics {

void begin() {
  const esp_reset_reason_t resetReason = esp_reset_reason();
  const bool hasPsram = psramFound();

  Serial.println();
  Serial.println("Brooklyn Active Subway Map Firmware");
#if defined(BROOKLYN_TARGET_PCB)
  Serial.println("Build target: BROOKLYN PCB");
#else
  Serial.println("Build target: XIAO BENCH");
#endif
  Serial.printf("ESP32 chip: %s\n", ESP.getChipModel());
  Serial.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()));
  Serial.printf("PSRAM: %s, %lu bytes\n", hasPsram ? "present" : "not present",
                static_cast<unsigned long>(hasPsram ? ESP.getPsramSize() : 0));
  Serial.printf("Free heap: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFreeHeap()));
  Serial.printf("Reset reason: %s (%d)\n", resetReasonName(resetReason),
                static_cast<int>(resetReason));
#if defined(BROOKLYN_TARGET_PCB)
  Serial.printf("LED outputs: DISABLED (GPIO %u LOW)\n",
                brooklyn::pins::LED_ENABLE);
#else
  Serial.println("LED outputs: DISABLED (production hardware not initialized)");
#endif

  lastHeartbeatMs = millis();
}

void update() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastHeartbeatMs) >= 5000) {
    lastHeartbeatMs = now;
    Serial.printf("Heartbeat: uptime %lu s; LED outputs DISABLED\n",
                  static_cast<unsigned long>(now / 1000));
  }
}

}  // namespace brooklyn::diagnostics
