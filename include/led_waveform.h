// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "led_types.h"
#include <stddef.h>
#include <stdint.h>

namespace brooklyn::led::waveform {
constexpr uint16_t kMaxChannelPixels = 42;
constexpr size_t kMaxItems = kMaxChannelPixels * 24 + 1;
// 40 MHz counter: retain the previous driver's 400/850 ns and 800/450 ns
// timings. One RMT item contains HIGH then LOW, MSB first, in GRB order.
constexpr uint32_t kZero = 16u | (1u << 15) | (34u << 16);
constexpr uint32_t kOne = 32u | (1u << 15) | (18u << 16);
constexpr uint32_t kReset = 6000u | (6000u << 16); // 300 us, both levels LOW

template <typename Item>
size_t encode(const DisplayState *pixels, uint16_t count, Item *out,
              size_t capacity) {
  if (!pixels || !out || !count || count > kMaxChannelPixels ||
      capacity < size_t(count) * 24 + 1) return 0;
  size_t used = 0;
  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t grb[] = {pixels[i].color.green, pixels[i].color.red,
                           pixels[i].color.blue};
    for (uint8_t byte : grb)
      for (uint8_t bit = 0x80; bit; bit >>= 1)
        out[used++].val = (byte & bit) ? kOne : kZero;
  }
  out[used++].val = kReset;
  return used;
}
}
