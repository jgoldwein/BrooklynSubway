// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "led_types.h"
#include <stdint.h>

namespace brooklyn::led::transport {
// Serialized by the existing live/diagnostic guard. GPIO1 is owned by the
// safety controller; a transport fault may additionally force it LOW.
bool initialize();
void holdDataLow();
bool transmit(const DisplayState *frame, uint16_t count);
}
