// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "monitor_seed.h"
namespace brooklyn::monitor {
SeedResult seedOnce(const Settings &seed,uint64_t expected,uint64_t actual,
                    const Store &store,const SeedStore &markers) {
  if(!expected || expected!=actual)return SeedResult::WrongDevice;
  if(!valid(seed) || !markers.handled || !markers.mark)return SeedResult::Failed;
  bool handled=false;
  if(!markers.handled(markers.context,seed.deviceId,handled))return SeedResult::Failed;
  if(handled)return SeedResult::AlreadyHandled;
  Settings previous;bool configured=false;
  const bool loaded=load(store,previous,configured);wipe(&previous,sizeof(previous));
  if(!loaded)return SeedResult::Failed;
  if(!configured && (!store.write || !store.write(store.context,seed)))return SeedResult::Failed;
  // Settings are committed first. After interruption, valid settings are preserved
  // and the marker write is retried. No partial key sequence is stored.
  if(!markers.mark(markers.context,seed.deviceId))return SeedResult::Failed;
  return configured?SeedResult::PreservedExisting:SeedResult::Applied;
}
}
