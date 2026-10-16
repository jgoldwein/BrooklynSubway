// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "rgb_renderer.h"

namespace brooklyn::display::output {

bool publish(const semantic::RgbFrame *frame);

}  // namespace brooklyn::display::output