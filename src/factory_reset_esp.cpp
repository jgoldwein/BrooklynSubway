#include "factory_reset.h"
#include "network_config.h"
#if BROOKLYN_NETWORK_ENABLED
#include <Preferences.h>

namespace brooklyn::defaults {
namespace {
constexpr const char *kResetNamespace="bkmap_reset";
bool read(void *,bool &pending) {
  Preferences prefs;
  if (!prefs.begin(kResetNamespace,false)) return false;
  pending=prefs.getBool("pending",false);prefs.end();return true;
}
bool write(void *,bool pending) {
  Preferences prefs;
  if (!prefs.begin(kResetNamespace,false)) return false;
  const bool ok=prefs.putBool("pending",pending)==sizeof(bool);
  prefs.end();return ok;
}
bool clear(void *,const char *name) {
  Preferences prefs;
  if (!prefs.begin(name,false)) return false;
  const bool ok=prefs.clear();prefs.end();return ok;
}
}
bool request() {return write(nullptr,true);}
Result completePending() {return completePending({nullptr,read,write,clear});}
}
#endif
