// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "monitor_config.h"
namespace brooklyn::monitor {
enum class SeedResult : uint8_t { NotIncluded, WrongDevice, AlreadyHandled,
  PreservedExisting, Applied, Failed };
struct SeedStore {
  void *context;
  bool (*handled)(void *,const char *seedId,bool &);
  bool (*mark)(void *,const char *seedId);
};
SeedResult seedOnce(const Settings &seed,uint64_t expectedUnit,uint64_t actualUnit,
                    const Store &,const SeedStore &);
SeedResult applyPrivateEnrollment();
inline const char *seedResultName(SeedResult r) {
  switch(r) {
    case SeedResult::WrongDevice:return "not applied: different unit";
    case SeedResult::AlreadyHandled:return "already handled; local choices retained";
    case SeedResult::PreservedExisting:return "existing monitoring settings retained";
    case SeedResult::Applied:return "individual monitoring settings installed";
    case SeedResult::Failed:return "not applied: storage/configuration error";
    default:return "no private enrollment included";
  }
}
}
