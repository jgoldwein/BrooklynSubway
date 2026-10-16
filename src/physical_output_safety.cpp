// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "physical_output_safety.h"

namespace brooklyn::led::safety {
namespace {

constexpr DisplayState kBlackFrame[map::TOTAL_LED_COUNT]{};

}  // namespace

bool isBlackFrame(const DisplayState *frame, uint16_t count) {
  if (frame == nullptr || count != map::TOTAL_LED_COUNT) {
    return false;
  }
  for (uint16_t index = 0; index < count; ++index) {
    const Rgb color = frame[index].color;
    if (color.red != 0 || color.green != 0 || color.blue != 0) {
      return false;
    }
  }
  return true;
}

bool Controller::validOperations(const Operations &operations) const {
  return operations.setEnable != nullptr &&
         operations.initializeBackend != nullptr &&
         operations.setDataLow != nullptr &&
         operations.prepareBlack != nullptr &&
         operations.transmit != nullptr;
}

void Controller::failSafe(const Operations &operations, bool backendReady) {
  const bool needsDataLow = backendReady || state_ != State::Disabled;
  const bool enableLow =
      operations.setEnable != nullptr &&
      operations.setEnable(operations.context, false);
  const bool dataLow = !needsDataLow ||
                       (operations.setDataLow != nullptr &&
                        operations.setDataLow(operations.context));
  state_ = enableLow && dataLow
               ? (backendReady ? State::BackendReady : State::Disabled)
               : State::Fault;
}

bool Controller::initialize(const Operations &operations,
                            bool compileTimePermission) {
  state_ = State::Disabled;
  if (!compileTimePermission || !validOperations(operations)) {
    return false;
  }
  if (!operations.setEnable(operations.context, false)) {
    failSafe(operations, false);
    return false;
  }
  if (!operations.initializeBackend(operations.context)) {
    failSafe(operations, false);
    return false;
  }
  state_ = State::BackendReady;
  return true;
}

bool Controller::arm(const Operations &operations,
                     bool compileTimePermission) {
  if (!compileTimePermission || state_ != State::BackendReady ||
      !validOperations(operations)) {
    return false;
  }
  if (!operations.setDataLow(operations.context)) {
    failSafe(operations, true);
    return false;
  }

  state_ = State::ClearingBlack;
  DisplayState limitedBlack[map::TOTAL_LED_COUNT]{};
  if (!operations.prepareBlack(operations.context, kBlackFrame, limitedBlack,
                               map::TOTAL_LED_COUNT) ||
      !isBlackFrame(limitedBlack, map::TOTAL_LED_COUNT)) {
    failSafe(operations, true);
    return false;
  }
  if (!operations.setEnable(operations.context, true)) {
    failSafe(operations, true);
    return false;
  }
  if (!operations.transmit(operations.context, limitedBlack,
                           map::TOTAL_LED_COUNT)) {
    failSafe(operations, true);
    return false;
  }

  state_ = State::Armed;
  return true;
}

bool Controller::transmit(const Operations &operations,
                          bool compileTimePermission,
                          const DisplayState *frame, uint16_t count) {
  if (!compileTimePermission || state_ != State::Armed ||
      !validOperations(operations) || frame == nullptr ||
      count != map::TOTAL_LED_COUNT) {
    return false;
  }
  if (!operations.transmit(operations.context, frame, count)) {
    failSafe(operations, true);
    return false;
  }
  return true;
}

void Controller::disarm(const Operations &operations) {
  const bool backendWasReady = state_ != State::Disabled;
  failSafe(operations, false);
  if (backendWasReady && state_ == State::Disabled) {
    state_ = State::BackendReady;
  }
}

}  // namespace brooklyn::led::safety
