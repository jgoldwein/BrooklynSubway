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
// Fixed CH1/LED42 red=4 test, two seconds, black-first/black-last.
// Synchronous: never overlaps blocking feed retrieval or OTA work.
bool runFirstLedDiagnostic();
void setDiagnosticSystemReady(bool ready);

}  // namespace brooklyn::led::backend
