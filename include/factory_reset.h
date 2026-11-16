#pragma once

namespace brooklyn::defaults {
enum class Result { NotRequested, Complete, Failed };
struct Store {
  void *context;
  bool (*readPending)(void *, bool &);
  bool (*writePending)(void *, bool);
  bool (*clearNamespace)(void *, const char *);
};
// A committed marker survives power loss. Clear it only after every owned
// settings namespace is empty. Firmware/OTA partitions and diagnostics stay intact.
inline Result completePending(const Store &store) {
  bool pending=false;
  if (!store.readPending || !store.readPending(store.context,pending)) return Result::Failed;
  if (!pending) return Result::NotRequested;
  if (!store.clearNamespace || !store.writePending) return Result::Failed;
  const char *const names[]={"bkmap_wifi", "bkmap_cfg", "otacfg"};
  for (const char *name : names) {
    if (!store.clearNamespace(store.context,name)) return Result::Failed;
  }
  return store.writePending(store.context,false) ? Result::Complete : Result::Failed;
}
bool request();
Result completePending();
}
