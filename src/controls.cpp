#include "controls.h"

#include "board_pins.h"

#include <Arduino.h>

#if defined(BROOKLYN_TARGET_PCB)
namespace {

constexpr uint32_t DEBOUNCE_MS = 30;

struct Button {
  const char *name;
  uint8_t pin;
  bool stablePressed;
  bool lastReadingPressed;
  uint32_t lastChangeMs;
  uint32_t pressedAtMs;
};

Button buttons[] = {
    {"TEST", brooklyn::pins::BUTTON_TEST, false, false, 0, 0},
    {"ROUTE", brooklyn::pins::BUTTON_ROUTE, false, false, 0, 0},
    {"DIM", brooklyn::pins::BUTTON_DIM, false, false, 0, 0},
    {"POWER", brooklyn::pins::BUTTON_POWER, false, false, 0, 0},
    {"MODE", brooklyn::pins::BUTTON_MODE, false, false, 0, 0},
    {"BRIGHT", brooklyn::pins::BUTTON_BRIGHT, false, false, 0, 0},
    {"ROTARY_PUSH", brooklyn::pins::ROTARY_PUSH, false, false, 0, 0},
};

bool isPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

}  // namespace
#endif

namespace brooklyn::controls {

void begin() {
#if defined(BROOKLYN_TARGET_PCB)
  const uint32_t now = millis();
  for (Button &button : buttons) {
    const bool pressed = isPressed(button.pin);
    button.stablePressed = pressed;
    button.lastReadingPressed = pressed;
    button.lastChangeMs = now;
    button.pressedAtMs = pressed ? now : 0;
  }
#endif
}

void update() {
#if defined(BROOKLYN_TARGET_PCB)
  const uint32_t now = millis();
  for (Button &button : buttons) {
    const bool pressed = isPressed(button.pin);
    if (pressed != button.lastReadingPressed) {
      button.lastReadingPressed = pressed;
      button.lastChangeMs = now;
    }

    if (pressed != button.stablePressed &&
        static_cast<uint32_t>(now - button.lastChangeMs) >= DEBOUNCE_MS) {
      button.stablePressed = pressed;
      if (pressed) {
        button.pressedAtMs = now;
        Serial.printf("Control PRESS: %s\n", button.name);
      } else {
        const uint32_t heldMs = now - button.pressedAtMs;
        Serial.printf("Control RELEASE: %s (held %lu ms)\n", button.name,
                      static_cast<unsigned long>(heldMs));
      }
    }
  }
#endif
}

}  // namespace brooklyn::controls
