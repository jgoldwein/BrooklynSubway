// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "led_types.h"
namespace brooklyn::semantic {
struct TrainColors { led::Rgb approaching, stopped, leaving, uncertain; };
static_assert(sizeof(TrainColors)==12,"Four packed RGB colors");
constexpr TrainColors kDefaultTrainColors{{255,255,0},{255,0,0},{0,255,0},{0,0,255}};
inline bool sameRgb(led::Rgb a,led::Rgb b) { return a.red==b.red && a.green==b.green && a.blue==b.blue; }
inline bool sameTrainColors(const TrainColors &a,const TrainColors &b) {
 return sameRgb(a.approaching,b.approaching) && sameRgb(a.stopped,b.stopped) && sameRgb(a.leaving,b.leaving) && sameRgb(a.uncertain,b.uncertain);
}
}
