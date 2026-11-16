// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace brooklyn::tls {
struct MemoryStats {uint32_t externalAllocations=0,internalFallbacks=0,allocationFailures=0;};
// Install once before any TLS users. Existing internal/PSRAM pointers share
// heap_caps_free; certificate validation, TLS settings and key policy are unchanged.
bool configureMemory();
MemoryStats memoryStats();
}
