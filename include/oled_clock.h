#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>
namespace brooklyn::oled {
// New York civil time; independent of the UTC SNTP configuration.
bool formatClock(time_t utc, char *out, size_t capacity);
void begin(uint8_t address, uint8_t contrast = 207);
void setContrast(uint8_t contrast);
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
