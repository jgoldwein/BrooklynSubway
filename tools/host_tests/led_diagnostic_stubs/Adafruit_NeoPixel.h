#pragma once
#include <stdint.h>
constexpr int NEO_GRB = 0;
constexpr int NEO_KHZ800 = 0;
extern bool failAllocation;
void observeStrip(uint8_t pin, const uint32_t *pixels, uint16_t count);
class Adafruit_NeoPixel {
 public:
  Adafruit_NeoPixel(uint16_t count, uint8_t pin, int) : count_(count), pin_(pin) {}
  uint8_t *getPixels() { return failAllocation ? nullptr : reinterpret_cast<uint8_t *>(pixels_); }
  void begin() {}
  void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    pixels_[index] = (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
  }
  void show() { observeStrip(pin_, pixels_, count_); }
 private:
  uint16_t count_;
  uint8_t pin_;
  uint32_t pixels_[42]{};
};
