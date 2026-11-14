// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "led_types.h"

namespace brooklyn::hardware {

// Initializes only Phase 1 hardware. Returns false if I2C initialization fails.
bool beginSafe();
// Two brief U23 pulses and a short passive-buzzer chime after local checks.
// Holding DIM at boot mutes the chime. Never indicates feed health.
// Leaves the Avi-location output LOW for its normal runtime use.
void indicateStartup(bool checksPassed);
// Manual diagnostic cue: one to eight beeps; POWER aborts; leaves GPIO21 LOW.
bool signalDiagnosticChannel(led::PhysicalChannel channel);
// U23/GPIO14: dim PWM feed-health indicator, independent of map POWER state.
// First successful ON per boot emits a 100 ms GPIO21 beep unless DIM-muted.
void setFeedIndicator(bool freshComplete);
void setFeedIndicatorAllowed(bool allowed);

}  // namespace brooklyn::hardware
