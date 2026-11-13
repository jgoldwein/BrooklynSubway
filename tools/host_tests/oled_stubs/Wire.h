#pragma once
#include <stdint.h>
#include <stddef.h>
struct FakeWire {
 void setTimeOut(uint16_t); void setClock(uint32_t); void beginTransmission(uint8_t);
 uint8_t endTransmission(); size_t write(uint8_t *, size_t);
};
extern FakeWire Wire;
