#pragma once
#include <stdint.h>
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT = 2;
constexpr int OUTPUT = 3;
struct FakeSerial {
  void println(const char *) {}
  template <typename... Args> void printf(const char *, Args...) {}
};
extern FakeSerial Serial;
void digitalWrite(uint8_t pin, int level);
void pinMode(uint8_t pin, int mode);
void delay(uint32_t milliseconds);
