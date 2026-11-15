// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "runtime_observation.h"

namespace brooklyn::diagnostics {

void begin();
void update();
const char *resetReasonName();
const observation::BootInfo &bootInfo();

}  // namespace brooklyn::diagnostics
