// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "tls_memory.h"
#include "network_config.h"
#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_NETWORK_ENABLED
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#include <stdint.h>
namespace brooklyn::tls {
namespace {
constexpr size_t kExternalThreshold=4096;
uint32_t externalAllocations=0,internalFallbacks=0,allocationFailures=0;
void *allocate(size_t count,size_t size) {
  if(count && size>SIZE_MAX/count){__atomic_add_fetch(&allocationFailures,1u,__ATOMIC_RELAXED);return nullptr;}
  const bool large=count*size>=kExternalThreshold;
  if(large){
    void *p=heap_caps_calloc(count,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(p){__atomic_add_fetch(&externalAllocations,1u,__ATOMIC_RELAXED);return p;}
    __atomic_add_fetch(&internalFallbacks,1u,__ATOMIC_RELAXED);
  }
  void *p=heap_caps_calloc(count,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(!p && count && size)__atomic_add_fetch(&allocationFailures,1u,__ATOMIC_RELAXED);
  return p;
}
void release(void *p){heap_caps_free(p);}
}
bool configureMemory() {
  // MBEDTLS_PLATFORM_MEMORY is enabled in the pinned ESP32-S3 SDK. Set once,
  // before network startup, rather than changing allocators during connections.
  static bool configured=false;if(configured)return true;
  configured=mbedtls_platform_set_calloc_free(allocate,release)==0;
  Serial.println(configured?"TLS memory: blocks >=4096 B prefer PSRAM; smaller blocks internal; certificate verification unchanged":"TLS memory: allocator setup FAILED; SDK allocator retained");
  return configured;
}
MemoryStats memoryStats(){MemoryStats result;result.externalAllocations=__atomic_load_n(&externalAllocations,__ATOMIC_RELAXED);result.internalFallbacks=__atomic_load_n(&internalFallbacks,__ATOMIC_RELAXED);result.allocationFailures=__atomic_load_n(&allocationFailures,__ATOMIC_RELAXED);return result;}
}
#else
namespace brooklyn::tls {bool configureMemory(){return true;}MemoryStats memoryStats(){return {};}}
#endif
