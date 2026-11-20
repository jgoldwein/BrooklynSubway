// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace brooklyn::telemetry {
// Separate from the frozen heartbeat. No credentials, personal text or raw URLs.
enum class RestartIntent : uint8_t { None, Web, PowerButton, FirmwareUpdate, Defaults };
struct RestartRecord { uint32_t magic, intent, inverse; };
inline void rememberRestart(RestartRecord &r,RestartIntent intent) {
  r.intent=static_cast<uint32_t>(intent);r.inverse=~r.intent;r.magic=0x424b5235;
}
inline RestartIntent takeRestart(RestartRecord &r,bool softwareReset) {
  const bool valid=r.magic==0x424b5235 && r.inverse==~r.intent && r.intent>0 && r.intent<=4;
  const auto result=valid && softwareReset?static_cast<RestartIntent>(r.intent):RestartIntent::None;
  r={};return result;
}
struct Diagnostics {
  bool autoUpdates=false, checkAttempted=false, checkSucceeded=false, updateAvailable=false;
  uint32_t checkIntervalSeconds=0, lastCheckAgeSeconds=0, nextCheckSeconds=0;
  char availableVersion[33]{};
  uint8_t installPhase=0; // idle, downloading, staged, failed
  bool ledsOn=false, oledOn=false, quiet=false, maintenance=false, systemHealthy=true;
  uint8_t powerMode=0, brightness=0, oledBrightness=0, quietMapCap=255;
  uint8_t oledState=0;
  uint32_t routeMask=0;
  bool hasInput=false, hasButton=false;
  uint64_t lastInputMs=0,lastButtonMs=0;
  RestartIntent restart=RestartIntent::None;
  bool imageConfirmed=false;
  uint32_t settingsRevision=0;
  uint64_t settingsAtMs=0;
  char settingsGroup[24]{}; // code-defined label only
  bool monitoringEnabled=true;
  bool hasError=false;
  uint64_t errorAtMs=0;
  uint8_t errorFeed=0,errorCategory=0;
  int32_t errorHttp=0,errorTls=0;
  bool wifiOutage=false,mqttOutage=false;
  uint64_t wifiLossMs=0,mqttLossMs=0,wifiRecoveryMs=0,mqttRecoveryMs=0;
  uint64_t wifiOutageMs=0,mqttOutageMs=0;
};
const char *ledOffReason(const Diagnostics &d);
const char *restartIntentName(RestartIntent intent);
size_t buildDiagnosticsJson(char *out,size_t capacity,const char *deviceId,const char *bootId,
                           uint64_t sequence,uint64_t uptime,uint32_t unixTime,const Diagnostics &d);
}
