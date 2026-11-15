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
// U23/GPIO14: dim PWM feed-health indicator, gated by the selected POWER mode.
// First successful ON per boot emits a 100 ms GPIO21 beep unless DIM-muted.
void setFeedIndicator(bool freshComplete);
void setFeedIndicatorAllowed(bool allowed,uint8_t brightnessCap=8);
void signalBrightness(uint8_t value, uint8_t maximum);
// One 120 ms confirmation after the setup AP successfully starts; quiet-aware.
void signalSetupReady();
// Bounded, nonblocking knob feedback; serviced by the live input task.
void requestKnobClick();
void serviceKnobClick(uint32_t nowMs, bool allowed);
void setQuiet(bool quiet,uint8_t mapBrightness=255);
// Manual easter-egg tone; zero stops. Shared timer/quiet policy, no delay.
void setEasterEggTone(uint16_t frequency);
void enableStatusReadyCues();
void observeStatusGreen(uint8_t bits);

}  // namespace brooklyn::hardware
