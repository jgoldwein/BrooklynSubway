// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace brooklyn::monitor {
constexpr const char *kNamespace = "bkmap_mon";
constexpr uint32_t kRecordVersion = 1;
// One atomic NVS blob. No production credential is compiled into firmware.
struct Settings {
  uint32_t version = kRecordVersion;
  uint8_t enabled = 1; // Once provisioned, ON unless the administrator chooses OFF.
  char host[97] = "mqtt.goldwein.net";
  uint16_t port = 8884;
  char deviceId[37] = {};
  char password[65] = {};
  char name[33] = "Brooklyn map";
};
bool validUuid(const char *text);
bool valid(const Settings &settings);
void wipe(void *data, size_t size);
struct Param { const char *name; const char *value; };
// Blank password preserves the existing key only for the SAME host/port/identity.
// All fields required, duplicates/unknowns rejected, output untouched on failure.
bool parseSettings(const Param *params, size_t count, const Settings &previous, Settings &out);
struct Store {
  void *context;
  bool (*read)(void *, Settings &, bool &present);
  bool (*write)(void *, const Settings &);
  bool (*erase)(void *);
};
bool load(const Store &, Settings &, bool &configured);
Store nvsStore();

enum class State : uint8_t { NotConfigured, Disabled, WaitingNetwork, WaitingMemory,
  Connecting, Online, Retrying, Paused, SnapshotStale, Failed };
inline const char *stateName(State state) {
  switch(state) {
    case State::NotConfigured:return "Not configured";
    case State::Disabled:return "Off";
    case State::WaitingNetwork:return "Waiting for Wi-Fi and clock";
    case State::WaitingMemory:return "Waiting for memory";
    case State::Connecting:return "Connecting";
    case State::Online:return "Connected to monitoring server";
    case State::Retrying:return "Connection interrupted; retrying";
    case State::Paused:return "Paused for firmware update";
    case State::SnapshotStale:return "Waiting for fresh application status";
    default:return "Monitoring unavailable; map continues";
  }
}
struct Runtime {
  State state = State::NotConfigured;
  bool configured = false;
  bool enabled = false;
  bool hasStatusAck = false;
  uint64_t lastStatusAckMs = 0;
  uint64_t uptimeMs = 0;
  uint32_t wifiDisconnects = 0;
  uint32_t mqttDisconnects = 0;
  uint64_t lastWifiLossMs = 0;
  uint64_t lastMqttLossMs = 0;
};
// Observed transitions only: initial disconnection is not counted as a loss.
struct ConnectionHistory {
  bool wifiOutage=false,mqttOutage=false;
  uint64_t wifiRecoveryMs=0,mqttRecoveryMs=0,wifiOutageMs=0,mqttOutageMs=0;
  bool wifiWasUp = false;
  uint32_t wifiLosses = 0;
  uint32_t mqttLosses = 0;
  uint64_t lastWifiLossMs = 0;
  uint64_t lastMqttLossMs = 0;
  bool observeWifi(bool up, uint64_t now);
  void mqttLost(uint64_t now);
  void mqttRecovered(uint64_t now);
};
constexpr uint32_t kStartHeapBytes = 48000;
constexpr uint32_t kStartLargestBytes = 8192;
constexpr uint32_t kStopHeapBytes = 24000;
constexpr uint32_t kStopLargestBytes = 4096;
constexpr uint32_t kSnapshotMaxAgeMs = 15000;
bool resourcesAllow(bool clientActive, uint32_t freeBytes, uint32_t largestBytes);
struct ResourceGate {
  uint64_t retryAtMs=0;
  bool allow(bool active,uint32_t freeBytes,uint32_t largestBytes,uint64_t now) {
    if(active && !resourcesAllow(true,freeBytes,largestBytes))retryAtMs=now+60000;
    return now>=retryAtMs && resourcesAllow(active,freeBytes,largestBytes);
  }
};
} // namespace brooklyn::monitor
