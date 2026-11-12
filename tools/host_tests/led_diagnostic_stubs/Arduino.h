#pragma once
#include <stdint.h>
constexpr int LOW = 0;
constexpr int HIGH = 1;
struct FakeSerial {
  void println(const char *) {}
  template <typename... Args> void printf(const char *, Args...) {}
};
extern FakeSerial Serial;
void digitalWrite(uint8_t pin, int level);
void delay(uint32_t milliseconds);
