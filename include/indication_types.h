// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace brooklyn::display {
enum class Indication : uint8_t { Off, Approaching, Stopped, Leaving, Uncertain };
enum class IndicationEnd : uint8_t {
  NoIndication, Approaching, Leaving, Uncertain, StopWindowExpired,
  SourceExpired, RouteHidden, UncertainHidden, BrightnessOrColorZero,
  CurrentLimited, OutputDisabled, PowerButton, Recovery, NoUsableData,
  OutputFailure, HardwareTest
};
inline const char *indicationName(Indication kind) {
  switch(kind) {
    case Indication::Approaching:return "Approaching";
    case Indication::Stopped:return "Stopped";
    case Indication::Leaving:return "Leaving";
    case Indication::Uncertain:return "Uncertain";
    default:return "Off";
  }
}
inline const char *indicationEndName(IndicationEnd reason) {
  switch(reason) {
    case IndicationEnd::Approaching:return "Approaching indication replaced stop";
    case IndicationEnd::Leaving:return "Leaving indication replaced stop";
    case IndicationEnd::Uncertain:return "Displayed report became uncertain";
    case IndicationEnd::StopWindowExpired:return "Stopped-color time limit expired";
    case IndicationEnd::SourceExpired:return "Original source freshness limit expired";
    case IndicationEnd::RouteHidden:return "Line hidden";
    case IndicationEnd::UncertainHidden:return "Uncertain indication hidden";
    case IndicationEnd::BrightnessOrColorZero:return "Brightness or configured color is zero";
    case IndicationEnd::CurrentLimited:return "Current limit made this pixel dark";
    case IndicationEnd::OutputDisabled:return "Live output disabled";
    case IndicationEnd::PowerButton:return "POWER button turned map lights off";
    case IndicationEnd::Recovery:return "Wi-Fi recovery requested";
    case IndicationEnd::NoUsableData:return "No usable display frame";
    case IndicationEnd::OutputFailure:return "Output stopped after a rendering or driver failure";
    case IndicationEnd::HardwareTest:return "Hardware test paused live output";
    default:return "No indication at this position in displayed frame";
  }
}
struct IndicationCounts { uint16_t approaching, stopped, leaving, uncertain; };
constexpr uint8_t kIndicationHistoryCapacity=64;
constexpr uint8_t kIndicationHistoryPage=12;
constexpr uint8_t kActiveIndicationPage=12;
struct IndicationEvent {
  uint32_t sequence, startedAtMs, endedAtMs, endedUnix;
  uint16_t led;
  IndicationEnd reason;
  Indication next;
};
struct ActiveIndication { uint32_t startedAtMs, durationMs; uint16_t led; };
struct IndicationReport {
  IndicationCounts counts{};
  uint32_t observedAtMs=0, observedUnix=0, newestSequence=0, oldestSequence=0, nextAfter=0;
  bool gap=false, more=false, available=false, outputEnabled=false;
  uint32_t nowMs=0;
  uint16_t activeTotal=0;
  uint8_t activeCount=0, eventCount=0;
  IndicationEvent events[kIndicationHistoryPage]{};
  ActiveIndication active[kActiveIndicationPage]{};
};
}
