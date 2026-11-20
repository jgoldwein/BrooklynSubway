// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "led_transport.h"

#if defined(BROOKLYN_TARGET_PCB) && defined(ESP32)
#include "board_pins.h"
#include "led_map.h"
#include "led_waveform.h"
#include <Arduino.h>
#include <driver/rmt.h>
#include <esp_attr.h>
#include <esp_intr_alloc.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

namespace brooklyn::led::transport {
namespace {
constexpr rmt_channel_t kChannel = RMT_CHANNEL_0;
constexpr uint8_t kMemoryBlocks = 4; // all four S3 TX blocks: 192 pulse items
constexpr uint32_t kCounterHz = 40000000;
constexpr uint32_t kTimeoutMs = 100;
// The IRAM interrupt handler must never read PSRAM or encode more data.
// Prepare one whole channel before starting; no allocation during frames.
DRAM_ATTR rmt_item32_t items[waveform::kMaxItems];
bool installed = false;
bool faulted = false;

void detach(uint8_t pin) {
  esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
  gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
  gpio_set_level(static_cast<gpio_num_t>(pin), 0);
}

bool fail(const char *operation, esp_err_t error, unsigned physicalChannel=0) {
  // Do this before stopping/remapping the signal, even if the safety caller
  // will also disarm. A timed-out buffer is never reused by a later frame.
  digitalWrite(pins::LED_ENABLE, LOW);
  faulted = true;
  if (installed) (void)rmt_tx_stop(kChannel);
  holdDataLow();
  Serial.printf("LED transport fault: %s CH%u error=%s; GPIO1 LOW; reboot required\n",
                operation, physicalChannel, esp_err_to_name(error));
  return false;
}
}

void holdDataLow() {
  for (uint8_t pin : pins::LED_DATA) detach(pin);
}

bool initialize() {
  if (faulted) return false;
  if (installed) {holdDataLow(); return true;}
  // Interrupt allocation binds to the calling core. Startup and live output
  // are on core 1; Wi-Fi runs on core 0. Never silently install on core 0.
  if (xPortGetCoreID() != 1) return fail("install core", ESP_ERR_INVALID_STATE);
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(
      static_cast<gpio_num_t>(pins::LED_DATA[0]), kChannel);
  config.clk_div = 2;
  config.mem_block_num = kMemoryBlocks;
  esp_err_t result = rmt_config(&config);
  if (result != ESP_OK) return fail("configure", result);
  result = rmt_driver_install(kChannel, 0, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
  if (result != ESP_OK) return fail("install", result);
  installed = true;
  uint32_t counterHz = 0;
  result = rmt_get_counter_clock(kChannel, &counterHz);
  if (result != ESP_OK || counterHz != kCounterHz)
    return fail("counter clock", result == ESP_OK ? ESP_ERR_INVALID_STATE : result);
  holdDataLow();
  Serial.printf("LED transport: persistent RMT, core 1, IRAM level 3, %u blocks, "
                "%u internal bytes, GRB 800 kHz, reset 300 us; "
                "GPIO4/5/6/7/10/11/12/13; GPIO1 safety interlock\n",
                unsigned(kMemoryBlocks), unsigned(sizeof(items)));
  return true;
}

bool transmit(const DisplayState *frame, uint16_t count) {
  if (!installed || faulted || !frame || count != map::TOTAL_LED_COUNT) return false;
  uint16_t offset = 0;
  for (uint8_t channel = 0; channel < map::CHANNEL_COUNT; ++channel) {
    const size_t used = waveform::encode(frame + offset, pins::LED_COUNT[channel],
                                         items, waveform::kMaxItems);
    if (!used) return fail("encode", ESP_ERR_INVALID_SIZE, channel + 1);
    const uint8_t pin = pins::LED_DATA[channel];
    esp_err_t result = rmt_set_gpio(kChannel, RMT_MODE_TX,
                                   static_cast<gpio_num_t>(pin), false);
    if (result != ESP_OK) return fail("route pin", result, channel + 1);
    result = rmt_write_items(kChannel, items, static_cast<int>(used), false);
    if (result != ESP_OK) return fail("start", result, channel + 1);
    result = rmt_wait_tx_done(kChannel, pdMS_TO_TICKS(kTimeoutMs));
    if (result != ESP_OK) return fail("wait", result, channel + 1);
    detach(pin); // prevent later channel pulses from reaching this chain
    offset += pins::LED_COUNT[channel];
  }
  return offset == count;
}
}
#endif
