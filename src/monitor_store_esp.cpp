// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "monitor_config.h"
#include "network_config.h"
#if BROOKLYN_NETWORK_ENABLED
#include <Preferences.h>
namespace brooklyn::monitor {
namespace {
bool read(void *,Settings &s,bool &present) {
  Preferences p;present=false;
  if(!p.begin(kNamespace,false))return false;
  const size_t size=p.getBytesLength("record");present=size!=0;
  const bool ok=!present || (size==sizeof(s) && p.getBytes("record",&s,sizeof(s))==sizeof(s));
  p.end();return ok;
}
bool write(void *,const Settings &s) {
  if(!valid(s))return false;
  Preferences p;if(!p.begin(kNamespace,false))return false;
  const bool ok=p.putBytes("record",&s,sizeof(s))==sizeof(s);p.end();return ok;
}
bool erase(void *) {
  Preferences p;if(!p.begin(kNamespace,false))return false;
  const bool ok=p.clear();p.end();return ok;
}
}
Store nvsStore() {return {nullptr,read,write,erase};}
}
#else
namespace brooklyn::monitor {Store nvsStore(){return {};}}
#endif
