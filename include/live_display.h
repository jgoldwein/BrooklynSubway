#pragma once
#include "live_display_policy.h"

namespace brooklyn::display::live {
bool begin();
bool available();
void publish(const Snapshot &snapshot);
void disable();
bool enabled();
OutputStats stats();
void restoreAfterSuccessfulTest(bool start);
bool consumeTestRequest();
void beginDiagnostic();
void endDiagnostic();
struct DiagnosticGuard {
  DiagnosticGuard() { beginDiagnostic(); }
  ~DiagnosticGuard() { endDiagnostic(); }
};
bool consumeRecoveryRequest();
bool consumeRebootRequest();
}
