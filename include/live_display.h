#pragma once
#include "live_display_policy.h"

namespace brooklyn::display::live {
bool begin();
bool available();
void publish(const Snapshot &snapshot);
bool enable();
void disable();
bool enabled();
OutputStats stats();
bool consumeRecoveryRequest();
}
