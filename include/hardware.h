// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace brooklyn::hardware {

// Initializes only Phase 1 hardware. Returns false if I2C initialization fails.
bool beginSafe();

}  // namespace brooklyn::hardware
