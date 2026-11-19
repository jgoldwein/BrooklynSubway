// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace brooklyn { namespace oled {
// 11x11 monochrome warning triangle; XBM rows, low bit is the left pixel.
constexpr uint8_t kAlertTriangle[]={0x20,0,0x50,0,0x50,0,0xa8,0,0xa8,0,0x24,1,0x04,1,0x22,2,0x02,2,0x01,4,0xff,7};
constexpr const char *kAlertPrompt="!-see pg 8";
}}
