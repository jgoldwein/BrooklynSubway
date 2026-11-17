// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "ota_release_schedule.h"
#include <assert.h>
#include <stdio.h>
using namespace brooklyn::ota;
int main() {
  constexpr uint32_t sixHours=21600000;
  assert(releaseCheckDue(false,false,5,0,sixHours,0));
  assert(!releaseCheckDue(true,false,59999,0,sixHours,1));
  assert(releaseCheckDue(true,false,60000,0,sixHours,1));
  assert(releaseCheckDelayMs(sixHours,2)==120000);
  assert(releaseCheckDelayMs(sixHours,3)==300000);
  assert(releaseCheckDelayMs(sixHours,4)==900000);
  assert(releaseCheckDelayMs(sixHours,255)==900000);
  assert(releaseCheckDelayMs(300000,255)==300000);
  assert(releaseCheckDelayMs(sixHours,0)==sixHours);
  assert(!releaseCheckDue(true,false,60000,0,sixHours,0));
  assert(releaseCheckDue(true,true,5,0,sixHours,0));
  assert(!releaseCheckDue(true,false,29998,UINT32_MAX-30000,sixHours,1));
  assert(releaseCheckDue(true,false,29999,UINT32_MAX-30000,sixHours,1));
  puts("OTA schedule: boot check, bounded failure backoff, configured cap, successful reset, manual force and rollover PASS");
}
