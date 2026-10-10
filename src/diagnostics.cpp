// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "diagnostics.h"

#include "board_pins.h"
#include "led_backend.h"
#include "led_map.h"
#include "led_power.h"

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
  Serial.printf("Physical LED backend: %s\n",
                brooklyn::led::backend::physicalLedBackendInitialized()
                    ? "initialized"
                    : "not initialized");
  Serial.println("Physical LED outputs: DISARMED");
  Serial.printf("Physical LED output permission: %s\n",
                brooklyn::led::backend::physicalLedOutputPermissionEnabled()
                    ? "ENABLED BY BUILD"
                    : "DISABLED BY BUILD");
  Serial.printf("LED channels: %u\n", brooklyn::led::map::CHANNEL_COUNT);
  Serial.printf("LED count: %u\n", brooklyn::led::map::TOTAL_LED_COUNT);
  Serial.printf("Power budget: %u mA at nominal %u mV / %u mA supply\n",
                brooklyn::led::power::FIRMWARE_LED_CURRENT_BUDGET_MA,
                brooklyn::led::power::NOMINAL_SUPPLY_VOLTAGE_MV,
                brooklyn::led::power::NOMINAL_SUPPLY_CURRENT_LIMIT_MA);
  Serial.printf("Bring-up brightness cap: %u/255\n",
                brooklyn::led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP);
  Serial.printf("GPIO%u LED_ENABLE: LOW / DISARMED\n",
                brooklyn::pins::LED_ENABLE);
  Serial.println("LED outputs: DISABLED");
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
