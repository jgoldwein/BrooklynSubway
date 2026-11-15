// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace brooklyn::weather {
// Approximate central Brooklyn; model data, not a board temperature sensor.
constexpr const char *kUrl="https://api.open-meteo.com/v1/forecast?latitude=40.6782&longitude=-73.9442&current=temperature_2m,weather_code&temperature_unit=fahrenheit&timeformat=unixtime&format=csv";
constexpr uint32_t kPollMs=15u*60u*1000u;
struct Snapshot {
  uint32_t sourceUnix=0;
  int16_t fahrenheitTenths=0;
  uint8_t code=0;
  bool available=false,attempted=false,lastFailed=false;
};
bool parseCsv(const uint8_t *data,size_t length,Snapshot &out);
const char *description(uint8_t code);
// Only schedule after a completed MTA cycle, in its first second of idle time.
bool due(bool attempted,uint32_t lastAttempt,uint32_t now,uint32_t cycleFinished);
}
