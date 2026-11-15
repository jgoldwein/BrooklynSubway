#pragma once
#include "live_display_policy.h"
#include "brightness_button_policy.h"

namespace brooklyn::display::live {
bool begin();
bool available();
void publish(const Snapshot &snapshot);
void disable(IndicationEnd reason=IndicationEnd::OutputDisabled);
void readIndications(IndicationReport &report, uint32_t after=0, bool latest=true);
bool enabled();
OutputStats stats();
void restoreAfterSuccessfulTest(bool start);
bool consumeTestRequest();
bool consumeBrightnessRequest(controls::BrightnessRequest &request);
bool showBrightnessFeedback(const controls::BrightnessRequest &request, uint8_t value);
void beginDiagnostic();
void endDiagnostic();
struct DiagnosticGuard {
  DiagnosticGuard() { beginDiagnostic(); }
  ~DiagnosticGuard() { endDiagnostic(); }
};
bool consumeRecoveryRequest();
bool consumeRebootRequest();
}
