#pragma once
#include <stdint.h>
constexpr int LOW=0;
struct FakeSerial {
  template<typename... Args> void printf(const char *, Args...) {}
};
extern FakeSerial Serial;
void digitalWrite(uint8_t pin, int level);
int xPortGetCoreID();
constexpr uint32_t pdMS_TO_TICKS(uint32_t ms) {return ms;}
