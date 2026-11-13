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
// CH1 only: 37 pixels, red=4, 500 ms each; POWER aborts between pixels.
bool runChannelOneDiagnostic();
// One selected manufactured channel, including CH8 status pixels.
bool runChannelDiagnostic(PhysicalChannel channel);
bool runAllChannelDiagnostics();
enum class RouteDiagnosticSpeed : uint8_t { Normal, Fast };
bool runAllRouteDiagnostics(RouteDiagnosticSpeed speed = RouteDiagnosticSpeed::Normal);
void setDiagnosticSystemReady(bool ready);
// Dedicated live-service entry points. Normal arm/transmit APIs remain locked.
bool armLiveOutput();
bool transmitLiveOutput(const DisplayState *frame, uint16_t count);
void stopLiveOutput();

}  // namespace brooklyn::led::backend
