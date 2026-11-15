// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace brooklyn::transport {
// One producer task, one loop consumer. Release/acquire transfers ownership of
// the pipeline, feed buffer, result and counters; no shared live pipeline reads.
class PollHandoff {
 public:
  bool start() { uint8_t expected=0; return __atomic_compare_exchange_n(&state_, &expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); }
  void finish() { __atomic_store_n(&state_, uint8_t{2}, __ATOMIC_RELEASE); }
  bool take() { uint8_t expected=2; return __atomic_compare_exchange_n(&state_, &expected, 0, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); }
  bool active() const { return __atomic_load_n(&state_, __ATOMIC_ACQUIRE)!=0; }
 private:
  uint8_t state_=0;
};
}
