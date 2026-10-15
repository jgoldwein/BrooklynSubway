// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace brooklyn::controls {

void begin();
void update();
// True once per 10-second continuous ROTARY_PUSH hold (production PCB only;
// always false on the XIAO). Requests Wi-Fi setup mode; never erases anything.
bool consumeWifiRecoveryRequest();

}  // namespace brooklyn::controls
