// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ota_package.h"

namespace brooklyn::ota {

// Public firmware-verification keys only. Slots whose bytes are all zero are
// unprovisioned and never trusted. A private key must never appear in this
// repository, in firmware, or in NVS.
constexpr size_t kTrustedKeyCount = 2;
extern const TrustedKey kTrustedKeys[kTrustedKeyCount];

}  // namespace brooklyn::ota
