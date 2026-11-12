#pragma once
#include <stdint.h>
struct FakeWire {
  bool begin(uint8_t, uint8_t) { return true; }
};
extern FakeWire Wire;
