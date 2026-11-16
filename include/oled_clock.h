#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "oled_screens.h"
#include "screen_preferences.h"
namespace brooklyn::oled {
// New York civil time; independent of the UTC SNTP configuration.
bool formatClock(time_t utc, char *out, size_t capacity);
bool formatDate(time_t utc,char *out,size_t capacity);
bool newYorkMinutes(time_t utc,uint16_t &minute);
uint16_t newYorkAnniversaryYear(time_t utc);
void setScreenPreferences(uint64_t word);
bool quietHoursActive(time_t utc);
uint8_t quietMapBrightness();
void begin(uint8_t address, uint8_t contrast = 207);
void setContrast(uint8_t contrast);
void setEnabled(bool enabled);
// Setup instructions replace the clock while provisioning. A boot that starts
// in setup shows the enabled startup dedication for three seconds first.
void setSetupMode(bool enabled);
// Worker-owned OLED overlay; publishers never access I2C.
void showUpgrade(UpgradePhase phase,const char *version=nullptr,uint8_t percent=0);
void publishScreenData(const ScreenData &data);
void showLineTest(semantic::Route route,uint8_t color,uint16_t current,uint16_t total,bool individual);
void clearLineTest();
void showEasterEgg(const ScreenText &text);
void clearEasterEgg();
void setRouteFocus(bool active,semantic::Route route);
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
