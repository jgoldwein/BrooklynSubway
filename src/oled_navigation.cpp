// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "oled_screens.h"
namespace brooklyn::oled {
void Navigation::setReturnSeconds(uint16_t seconds, uint32_t nowMs) {
  if (returnSeconds_ != seconds) { returnSeconds_ = seconds; lastInputAtMs_ = nowMs; }
}
bool Navigation::input(Action action, uint32_t nowMs) {
  if ((action == Action::Select || action == Action::ToggleAuto) && view_.screen != Screen::Lines) return false;
  lastInputAtMs_ = autoAtMs_ = nowMs;
  if (action == Action::NextScreen ||
      ((action==Action::TurnNext || action==Action::TurnPrevious) && !view_.lineBrowsing)) {
    view_.screen=action==Action::TurnPrevious ? static_cast<Screen>((static_cast<uint8_t>(view_.screen)+4u)%5u) : nextScreen(view_.screen);
    view_.automatic=false;view_.lineBrowsing=false;
  }
  else if (action == Action::OpenLines) { view_.screen=Screen::Lines;view_.lineBrowsing=true;view_.automatic=false; }
  else if (action == Action::Select) {view_.lineBrowsing=!view_.lineBrowsing;view_.automatic=false;}
  else if (action == Action::ToggleAuto) {view_.automatic=!view_.automatic;view_.lineBrowsing=true;}
  else {
    view_.automatic=false;
    if (action == Action::TurnNext) view_.line=view_.line==semantic::kRouteCount ? 1 : view_.line+1;
    else view_.line=view_.line==1 ? semantic::kRouteCount : view_.line-1;
  }
  view_.changedAtMs=nowMs;
  return true;
}
bool Navigation::update(uint32_t nowMs, bool active) {
  if (!active) { lastInputAtMs_=autoAtMs_=nowMs; return false; }
  if (view_.screen != Screen::Main && returnSeconds_ && static_cast<uint32_t>(nowMs-lastInputAtMs_) >= uint32_t(returnSeconds_)*1000) {
    view_.screen=Screen::Main;view_.automatic=false;view_.lineBrowsing=false;view_.changedAtMs=nowMs;return true;
  }
  if (view_.screen==Screen::Lines && view_.automatic && static_cast<uint32_t>(nowMs-autoAtMs_)>=5000) {
    autoAtMs_=nowMs;view_.line=view_.line==semantic::kRouteCount ? 1 : view_.line+1;view_.changedAtMs=nowMs;return true;
  }
  return false;
}
}
