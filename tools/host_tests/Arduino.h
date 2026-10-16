// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

class HostSerial {
 public:
  template <typename... Args>
  void printf(const char *, Args...) {}
  void print(const char *) {}
  void println(const char * = "") {}
};

extern HostSerial Serial;

constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;

inline void delay(uint32_t) {}