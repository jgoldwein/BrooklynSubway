// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_types.h"

#include <stdint.h>

namespace brooklyn::led::backend {

bool initializePhysicalLedBackend();
bool armLedOutputs();
void disarmLedOutputs();
bool physicalLedOutputsArmed();
bool physicalLedBackendInitialized();
bool physicalLedOutputPermissionEnabled();
bool transmitFrame(const DisplayState *frame, uint16_t count);

}  // namespace brooklyn::led::backend
