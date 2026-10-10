// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <Arduino.h>

#if defined(BROOKLYN_TARGET_PCB) && defined(BROOKLYN_TARGET_XIAO)
#error "Define exactly one Brooklyn hardware target"
#elif !defined(BROOKLYN_TARGET_PCB) && !defined(BROOKLYN_TARGET_XIAO)
#error "Define exactly one Brooklyn hardware target"
#endif

namespace brooklyn::pins {

#if defined(BROOKLYN_TARGET_PCB)
// LED subsystem — production PCB
constexpr uint8_t LED_ENABLE = 1;  // HIGH enables U14/U15 through Q1
constexpr uint8_t LED_DATA[8] = {4, 5, 6, 7, 10, 11, 12, 13};
constexpr uint16_t LED_COUNT[8] = {37, 38, 37, 37, 42, 37, 38, 40};
constexpr uint16_t LED_TOTAL = 306;

// Rotary encoder / knob
constexpr uint8_t ROTARY_A = 15;
constexpr uint8_t ROTARY_B = 16;
constexpr uint8_t ROTARY_PUSH = 17;

// Front buttons, all active LOW
constexpr uint8_t BUTTON_TEST = 47;
constexpr uint8_t BUTTON_ROUTE = 38;
constexpr uint8_t BUTTON_DIM = 39;
constexpr uint8_t BUTTON_POWER = 40;
constexpr uint8_t BUTTON_MODE = 41;
constexpr uint8_t BUTTON_BRIGHT = 42;

// Other peripherals
constexpr uint8_t POWER_STAT = 18;
constexpr uint8_t I2C_SDA = 8;
constexpr uint8_t I2C_SCL = 9;
constexpr uint8_t USER_LED = 14;  // HIGH = on
constexpr uint8_t BUZZER = 21;
constexpr uint8_t BOOT = 0;
constexpr uint8_t UART_TX = 43;   // module TXD0
constexpr uint8_t UART_RX = 44;   // module RXD0

// Native USB is fixed by ESP32-S3 silicon/module routing.
constexpr uint8_t USB_DM = 19;
constexpr uint8_t USB_DP = 20;

// Channel 8 indexes 0..3 are front-panel status indicators.
constexpr uint16_t STATUS_SYSTEM = 0;
constexpr uint16_t STATUS_NETWORK = 1;
constexpr uint16_t STATUS_MTA_FEED = 2;
constexpr uint16_t STATUS_MODE = 3;
#endif

}  // namespace brooklyn::pins
