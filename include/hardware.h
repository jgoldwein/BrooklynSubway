// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace brooklyn::hardware {

// Initializes only Phase 1 hardware. Returns false if I2C initialization fails.
bool beginSafe();
// Two brief U23 pulses after local startup checks; never indicates feed health.
// Leaves the Avi-location output LOW for its normal runtime use.
void indicateStartup(bool checksPassed);

}  // namespace brooklyn::hardware
