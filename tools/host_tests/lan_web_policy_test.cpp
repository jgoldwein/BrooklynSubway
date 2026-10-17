// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "led_power.h"
#include "normal_config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

int checks = 0;
int failures = 0;

#define CHECK(condition)                                                   \
  do {                                                                      \
    ++checks;                                                               \
    if (!(condition)) {                                                     \
      ++failures;                                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);           \
    }                                                                       \
  } while (0)

struct MemoryStore {
  bool present = false;
  bool failSave = false;
  uint32_t value = 0;
  uint32_t saves = 0;
};

bool loadBrightness(void *context, uint32_t &out) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (!store.present) return false;
  out = store.value;
  return true;
}

bool saveBrightness(void *context, uint8_t brightness) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failSave) return false;
  store.value = brightness;
  store.present = true;
  ++store.saves;
  return true;
}

void testBrightnessParsing() {
  uint8_t value = 0;
  CHECK(brooklyn::web::parseBrightness("0", value) && value == 0);
  CHECK(brooklyn::web::parseBrightness("1", value) && value == 1);
  CHECK(brooklyn::web::parseBrightness("64", value) && value == 64);
  CHECK(brooklyn::web::parseBrightness("255", value) && value == 255);
  CHECK(!brooklyn::web::parseBrightness(nullptr, value));
  CHECK(!brooklyn::web::parseBrightness("", value));
  CHECK(!brooklyn::web::parseBrightness("-1", value));
  CHECK(!brooklyn::web::parseBrightness("+1", value));
  CHECK(!brooklyn::web::parseBrightness(" 1", value));
  CHECK(!brooklyn::web::parseBrightness("1 ", value));
  CHECK(!brooklyn::web::parseBrightness("256", value));
  CHECK(!brooklyn::web::parseBrightness("999999999999", value));
  CHECK(!brooklyn::web::parseBrightness("1x", value));
}

void testPersistentConfiguration() {
  MemoryStore memory;
  brooklyn::config::Store store{&memory, loadBrightness, saveBrightness};
  brooklyn::config::DeviceConfiguration config(store);
  CHECK(!config.load());
  CHECK(config.brightness() == 64);

  const uint32_t savesBefore = memory.saves;
  const uint8_t expected[] = {0, 1, 64, 255};
  for (uint8_t value : expected) {
    CHECK(config.setBrightness(value));
    brooklyn::config::DeviceConfiguration reconstructed(store);
    CHECK(reconstructed.load());
    CHECK(reconstructed.brightness() == value);
  }
  CHECK(memory.saves == savesBefore + sizeof(expected));

  memory.value = 256;
  brooklyn::config::DeviceConfiguration corrupt(store);
  CHECK(!corrupt.load());
  CHECK(corrupt.brightness() == 64);
  memory.failSave = true;
  const uint32_t savesBeforeFailure = memory.saves;
  CHECK(!config.setBrightness(20));
  CHECK(config.brightness() == 255);
  CHECK(memory.value == 256);
  CHECK(memory.saves == savesBeforeFailure);
  CHECK(brooklyn::led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP == 16);
  CHECK(sizeof(brooklyn::config::DeviceConfiguration) <= sizeof(void *) * 4);
}

brooklyn::web::StatusSnapshot sampleStatus() {
  brooklyn::web::StatusSnapshot status{};
  strncpy(status.deviceName, "BrooklynMap", sizeof(status.deviceName) - 1);
  strncpy(status.target, "XIAO ESP32-S3 bench target",
          sizeof(status.target) - 1);
  strncpy(status.resetReason, "power-on", sizeof(status.resetReason) - 1);
  status.networkConnected = true;
  strncpy(status.ssid, "TestNetwork", sizeof(status.ssid) - 1);
  strncpy(status.ip, "192.0.2.17", sizeof(status.ip) - 1);
  status.rssi = -58;
  status.mdnsAvailable = true;
  status.timeSynchronized = true;
  status.displayState = brooklyn::display::State::Fresh;
  status.hasCycle = true;
  status.cycleOutcome = brooklyn::transport::CycleOutcome::Complete;
  status.feedsSucceeded = 8;
  status.feedsTotal = 8;
  status.hasDataAge = true;
  status.dataAgeSeconds = 12;
  status.hasCompleteAge = true;
  status.completeAgeSeconds = 12;
  status.resolvedTrains = 57;
  status.occupiedLeds = 53;
  status.collisionCount = 4;
  status.freeInternalHeap = 42000;
  status.minimumInternalHeap = 39000;
  status.psramPresent = true;
  status.freePsram = 3000000;
  status.minimumFreePsram = 2800000;
  status.psramFeedBufferPresent = true;
  return status;
}

void testPageEscapingAndRequiredContent() {
  char page[6144];
  brooklyn::web::StatusSnapshot status = sampleStatus();
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 64) > 0);
  CHECK(strstr(page, "TestNetwork") != nullptr);
  CHECK(strstr(page, "href=\"http://192.0.2.17/\"") != nullptr);
  CHECK(strstr(page, "http://brooklynmap.local/") != nullptr);
  CHECK(strstr(page, "Brightness: 64 / 255") != nullptr);
  CHECK(strstr(page, "<dt>Resolved trains</dt><dd>57</dd>") != nullptr);
  CHECK(strstr(page, "<dt>Occupied LEDs</dt><dd>53</dd>") != nullptr);
  CHECK(strstr(page, "<dt>Collision count</dt><dd>4</dd>") != nullptr);
  CHECK(strstr(page, "Fresh") != nullptr);
  CHECK(strstr(page, "DISABLED") != nullptr);
  CHECK(strstr(page, "power-on") != nullptr);
  CHECK(strstr(page, "Change Wi-Fi") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/wifi/setup\"") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/reboot\"") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/brightness\"") != nullptr);
  CHECK(strstr(page, "https://") == nullptr);
  CHECK(strstr(page, "cdn") == nullptr);
  CHECK(strstr(page, "<script") == nullptr);
  CHECK(strstr(page, "name=\"password\"") == nullptr);
  CHECK(strstr(page, "password sentinel") == nullptr);

  strncpy(status.ssid, "<test&\"wifi'>", sizeof(status.ssid) - 1);
  strncpy(status.deviceName, "<script>alert(1)</script>",
          sizeof(status.deviceName) - 1);
  strncpy(status.resetReason, "boot\nready", sizeof(status.resetReason) - 1);
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 0) > 0);
  CHECK(strstr(page, "&lt;test&amp;&quot;wifi&#39;&gt;") != nullptr);
  CHECK(strstr(page, "&lt;script&gt;alert(1)&lt;/script&gt;") != nullptr);
  CHECK(strstr(page, "<script>alert(1)</script>") == nullptr);
  CHECK(strstr(page, "boot&#x0a;ready") != nullptr);
  CHECK(strstr(page, "Brightness: 0 / 255") != nullptr);
  CHECK(strstr(page, "http://brooklynmap.local/") != nullptr);

  status.mdnsAvailable = false;
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 255) > 0);
  CHECK(strstr(page, "brooklynmap.local") == nullptr);
  CHECK(strstr(page, "Brightness: 255 / 255") != nullptr);

  strncpy(status.ip, "javascript:alert(1)", sizeof(status.ip) - 1);
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 64) > 0);
  CHECK(strstr(page, "javascript:") == nullptr);
  CHECK(strstr(page, "Unavailable</dd>") != nullptr);
  status.hasDataAge = false;
  status.hasCompleteAge = false;
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 64) > 0);
  CHECK(strstr(page, "<dt>Last usable update age</dt><dd>Not available</dd>") !=
        nullptr);
  CHECK(brooklyn::web::renderHomePage(page, 8, status, 64) == 0);
}

void testJsonStatusIsEscapedAndSecretFree() {
  char json[2048];
  brooklyn::web::StatusSnapshot status = sampleStatus();
  strncpy(status.ssid, "test\"\\wifi\n\t", sizeof(status.ssid) - 1);
  strncpy(status.deviceName, "device\"\\\x01",
          sizeof(status.deviceName) - 1);
  strncpy(status.target, "<x>&", sizeof(status.target) - 1);
  strncpy(status.resetReason, "boot\rready",
          sizeof(status.resetReason) - 1);
  CHECK(brooklyn::web::renderStatusJson(json, sizeof(json), status, 64) > 0);
  CHECK(strstr(json, "\"physicalLedOutputEnabled\":false") != nullptr);
  CHECK(strstr(json, "\"resetReason\":\"boot\\rready\"") != nullptr);
  CHECK(strstr(json, "\"brightness\":64") != nullptr);
  CHECK(strstr(json, "\"feedsSucceeded\":8,\"feedsTotal\":8") != nullptr);
  CHECK(strstr(json, "\"resolvedTrains\":57,\"occupiedLeds\":53,"
                     "\"collisionCount\":4") != nullptr);
  CHECK(strstr(json, "\"displayState\":\"Fresh\"") != nullptr);
  CHECK(strstr(json, "\"ssid\":\"test\\\"\\\\wifi\\n\\t\"") != nullptr);
  CHECK(strstr(json, "\"deviceName\":\"device\\\"\\\\\\u0001\"") != nullptr);
  CHECK(strstr(json, "\"target\":\"<x>&\"") != nullptr);
  CHECK(strstr(json, "\"password\"") == nullptr);
  CHECK(strstr(json, "password sentinel") == nullptr);
  status.hasDataAge = false;
  status.hasCompleteAge = false;
  CHECK(brooklyn::web::renderStatusJson(json, sizeof(json), status, 64) > 0);
  CHECK(strstr(json, "\"dataAgeSeconds\":null") != nullptr);
  CHECK(strstr(json, "\"completeAgeSeconds\":null") != nullptr);
  CHECK(brooklyn::web::renderStatusJson(json, 8, status, 64) == 0);
}

void testServerOwnershipTransitions() {
  brooklyn::web::ServerOwnership owner;
  CHECK(owner.mode() == brooklyn::web::ServerMode::Stopped);
  CHECK(owner.enterNormalLan());
  CHECK(owner.mode() == brooklyn::web::ServerMode::NormalLan);
  CHECK(!owner.enterProvisioning());
  CHECK(owner.mode() == brooklyn::web::ServerMode::NormalLan);
  owner.stop();
  CHECK(owner.enterProvisioning());
  CHECK(owner.mode() == brooklyn::web::ServerMode::Provisioning);
  CHECK(!owner.enterNormalLan());
  owner.stop();
  CHECK(owner.enterNormalLan());
  CHECK(owner.mode() == brooklyn::web::ServerMode::NormalLan);
}

}  // namespace

int main() {
  testBrightnessParsing();
  testPersistentConfiguration();
  testPageEscapingAndRequiredContent();
  testJsonStatusIsEscapedAndSecretFree();
  testServerOwnershipTransitions();
  char page[6144];
  char json[2048];
  const brooklyn::web::StatusSnapshot status = sampleStatus();
  printf("Sample generated response sizes: HTML %lu B, status JSON %lu B\n",
         static_cast<unsigned long>(brooklyn::web::renderHomePage(
             page, sizeof(page), status, 64)),
         static_cast<unsigned long>(brooklyn::web::renderStatusJson(
             json, sizeof(json), status, 64)));
  printf("LAN web policy tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
