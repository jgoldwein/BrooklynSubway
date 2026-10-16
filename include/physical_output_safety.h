// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "led_map.h"

namespace brooklyn::led::safety {

enum class State : uint8_t {
  Disabled,
  BackendReady,
  ClearingBlack,
  Armed,
  Fault,
};

struct Operations {
  void *context;
  bool (*setEnable)(void *context, bool enabled);
  bool (*initializeBackend)(void *context);
  bool (*setDataLow)(void *context);
  bool (*prepareBlack)(void *context, const DisplayState *black,
                       DisplayState *limited, uint16_t count);
  bool (*transmit)(void *context, const DisplayState *frame, uint16_t count);
};

bool isBlackFrame(const DisplayState *frame, uint16_t count);

class Controller {
 public:
  bool initialize(const Operations &operations, bool compileTimePermission);
  bool arm(const Operations &operations, bool compileTimePermission);
  bool transmit(const Operations &operations, bool compileTimePermission,
                const DisplayState *frame, uint16_t count);
  void disarm(const Operations &operations);

  State state() const { return state_; }

 private:
  bool validOperations(const Operations &operations) const;
  void failSafe(const Operations &operations, bool backendReady);

  State state_ = State::Disabled;
};

}  // namespace brooklyn::led::safety
