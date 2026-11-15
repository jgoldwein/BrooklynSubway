#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "oled_screens.h"
#include "screen_preferences.h"
namespace brooklyn::oled {
// New York civil time; independent of the UTC SNTP configuration.
bool formatClock(time_t utc, char *out, size_t capacity);
bool newYorkMinutes(time_t utc,uint16_t &minute);
void setScreenPreferences(uint64_t word);
bool quietHoursActive(time_t utc);
uint8_t quietMapBrightness();
void begin(uint8_t address, uint8_t contrast = 207);
void setContrast(uint8_t contrast);
void setEnabled(bool enabled);
// Setup instructions replace the clock only while the provisioning AP is active.
void setSetupMode(bool enabled);
void publishScreenData(const ScreenData &data);
Screen selectedScreen();
void advanceScreen();
void navigate(Action action, uint32_t nowMs);
void serviceNavigation(uint32_t nowMs, bool active);
void setReturnSeconds(uint16_t seconds);
void showBrightness(bool oled, uint8_t value, uint8_t maximum);
void clearBrightnessFeedback();
void setTrackedTrains(uint32_t count, bool available);
void setVisibleMapLights(uint16_t count);
enum class State : uint8_t { NotStarted, Starting, BusLow, NoAck, InitFailed, Ready, TransferFailed, TaskFailed, InvalidAddress };
struct Status { State state; uint8_t requestedAddress, activeAddress, error; };
Status status();
inline const char *stateName(State s) {
  switch (s) {
    case State::Starting: return "Starting";
    case State::BusLow: return "SDA/SCL held LOW";
    case State::NoAck: return "No OLED address acknowledged";
    case State::InitFailed: return "Initialization transfer failed";
    case State::Ready: return "Frame writes OK";
    case State::TransferFailed: return "Runtime transfer failed";
    case State::TaskFailed: return "Task allocation failed";
    case State::InvalidAddress: return "Invalid configured address";
    default: return "Not started";
  }
}
}
