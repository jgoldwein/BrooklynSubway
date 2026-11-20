// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "monitor_seed.h"
#include "network_config.h"
#if defined(BROOKLYN_PRIVATE_MONITOR_ENROLLMENT)
#if !defined(BROOKLYN_TARGET_PCB) || !BROOKLYN_NETWORK_ENABLED
#error "Private monitoring enrollment requires the production network target"
#endif
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "private_monitor_enrollment.h" // Generated ONLY inside an ignored private build tree.
namespace brooklyn::monitor {
namespace {
constexpr const char *kMarkerNamespace="bkmap_seed";
bool handled(void *,const char *seedId,bool &done) {
  Preferences p;if(!p.begin(kMarkerNamespace,false))return false;
  if(!p.isKey("enroll")) {p.end();done=false;return true;}
  char previous[37]{};const size_t len=p.getString("enroll",previous,sizeof(previous));
  p.end();const bool ok=len==37 && validUuid(previous);done=ok && !strcmp(previous,seedId);return ok;
}
bool mark(void *,const char *seedId) {
  Preferences p;if(!p.begin(kMarkerNamespace,false))return false;
  const bool ok=p.putString("enroll",seedId)==36;p.end();return ok;
}
}
SeedResult applyPrivateEnrollment() {
  auto settings=private_seed::settings();
  const auto result=seedOnce(settings,private_seed::kExpectedUnit,ESP.getEfuseMac(),nvsStore(),{nullptr,handled,mark});
  wipe(&settings,sizeof(settings));
  // Mandatory binary marker: public release tooling refuses any image containing it.
  Serial.printf("BROOKLYN_PRIVATE_MONITOR_ENROLLMENT_V1: %s\n",seedResultName(result));
  return result;
}
}
#else
namespace brooklyn::monitor {SeedResult applyPrivateEnrollment(){return SeedResult::NotIncluded;}}
#endif
