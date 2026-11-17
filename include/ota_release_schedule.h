// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace brooklyn { namespace ota {
inline uint32_t releaseCheckDelayMs(uint32_t configured,uint8_t failures) {
  if(!failures)return configured;
  const uint32_t retry=failures==1?60000u:failures==2?120000u:failures==3?300000u:900000u;
  return configured<retry?configured:retry;
}
inline bool releaseCheckDue(bool attempted,bool forced,uint32_t now,uint32_t last,
                           uint32_t configured,uint8_t failures) {
  return !attempted || forced || static_cast<uint32_t>(now-last)>=releaseCheckDelayMs(configured,failures);
}
}}
