#pragma once
#include "semantic_api.h"
#include <stdint.h>
namespace brooklyn::pipeline {
enum BlueFlag : uint8_t { NoPrediction=1, NextDifferent=2, FutureArrival=4, UnknownAge=8, StopChanged=16, RecentMatchingArrival=32 };
struct BlueEvidence { uint32_t stopKey; char nextStop[4]; uint8_t flags; uint8_t age; };
struct BlueRoute { uint16_t trains, leds, old, different, future, missing, shared, changed; };
struct BlueSample { uint16_t led; uint8_t age, flags, trains; char nextStop[5]; };
constexpr uint8_t kBlueSamples = 8;
struct BlueReport {
  uint32_t timestamp;
  uint16_t comparedTrips, historyDropped;
  uint16_t steadyLeds, uncertainLeds;
  BlueRoute routes[semantic::kRouteCount + 1];
  BlueSample samples[kBlueSamples];
  uint8_t sampleCount;
};
}
