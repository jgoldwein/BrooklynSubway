// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "led_power.h"
#include "normal_config.h"
#include "semantic_api.h"

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
  bool brightnessPresent = false;
  bool routeMaskPresent = false;
  bool failSave = false;
  uint32_t brightnessValue = 0;
  uint32_t routeMaskValue = 0;
  uint32_t saves = 0;
};

bool loadBrightness(void *context, uint32_t &out) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (!store.brightnessPresent) return false;
  out = store.brightnessValue;
  return true;
}

bool saveBrightness(void *context, uint8_t brightness) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failSave) return false;
  store.brightnessValue = brightness;
  store.brightnessPresent = true;
  ++store.saves;
  return true;
}

bool loadRouteVisibilityMask(void *context, uint32_t &out) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (!store.routeMaskPresent) return false;
  out = store.routeMaskValue;
  return true;
}

bool saveRouteVisibilityMask(void *context, uint32_t value) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failSave) return false;
  store.routeMaskValue = value;
  store.routeMaskPresent = true;
  ++store.saves;
  return true;
}

brooklyn::config::Store configStore(MemoryStore &memory) {
  return {&memory, loadBrightness, saveBrightness, loadRouteVisibilityMask,
          saveRouteVisibilityMask};
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
  brooklyn::config::Store store = configStore(memory);
  brooklyn::config::DeviceConfiguration config(store);
  CHECK(!config.load());
  CHECK(config.brightness() == 64);
  CHECK(config.routeVisibilityMask() ==
        brooklyn::semantic::kAllRoutesVisibleMask);

  const uint32_t savesBefore = memory.saves;
  const uint8_t expected[] = {0, 1, 64, 255};
  for (uint8_t value : expected) {
    CHECK(config.setBrightness(value));
    brooklyn::config::DeviceConfiguration reconstructed(store);
    CHECK(reconstructed.load());
    CHECK(reconstructed.brightness() == value);
  }
  CHECK(memory.saves == savesBefore + sizeof(expected));

  memory.brightnessValue = 256;
  memory.brightnessPresent = true;
  brooklyn::config::DeviceConfiguration corrupt(store);
  CHECK(!corrupt.load());
  CHECK(corrupt.brightness() == 64);
  memory.failSave = true;
  const uint32_t savesBeforeFailure = memory.saves;
  CHECK(!config.setBrightness(20));
  CHECK(config.brightness() == 255);
  CHECK(memory.brightnessValue == 256);
  CHECK(memory.saves == savesBeforeFailure);
  CHECK(brooklyn::led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP == 16);
  CHECK(sizeof(brooklyn::config::DeviceConfiguration) <= sizeof(void *) * 6);
}

void testRouteVisibilityPersistenceAndMigration() {
  MemoryStore memory;
  brooklyn::config::Store store = configStore(memory);
  memory.brightnessPresent = true;
  memory.brightnessValue = 73;

  brooklyn::config::DeviceConfiguration upgraded(store);
  CHECK(upgraded.load());
  CHECK(upgraded.brightness() == 73);
  CHECK(upgraded.routeVisibilityMask() ==
        brooklyn::semantic::kAllRoutesVisibleMask);
  CHECK(!memory.routeMaskPresent);

  const uint32_t savedBrightness = memory.brightnessValue;
  const uint32_t chosenMask =
      brooklyn::semantic::kAllRoutesVisibleMask &
      ~brooklyn::semantic::routeVisibilityBit(
          brooklyn::semantic::Route::RouteA);
  const uint32_t savesBeforeRoute = memory.saves;
  CHECK(upgraded.setRouteVisibilityMask(chosenMask));
  CHECK(memory.saves == savesBeforeRoute + 1);
  CHECK(memory.brightnessValue == savedBrightness);

  brooklyn::config::DeviceConfiguration reconstructed(store);
  CHECK(reconstructed.load());
  CHECK(reconstructed.brightness() == 73);
  CHECK(reconstructed.routeVisibilityMask() == chosenMask);
  CHECK(reconstructed.setBrightness(0));
  CHECK(reconstructed.routeVisibilityMask() == chosenMask);
  CHECK(reconstructed.setRouteVisibilityMask(0));
  CHECK(reconstructed.brightness() == 0);
  CHECK(reconstructed.routeVisibilityMask() == 0);

  memory.routeMaskValue =
      brooklyn::semantic::kAllRoutesVisibleMask | uint32_t{0x80000000};
  brooklyn::config::DeviceConfiguration highBits(store);
  CHECK(highBits.load());
  CHECK(highBits.routeVisibilityMask() ==
        brooklyn::semantic::kAllRoutesVisibleMask);

  memory.routeMaskValue = UINT32_MAX;
  brooklyn::config::DeviceConfiguration corruptMask(store);
  CHECK(corruptMask.load());
  CHECK(corruptMask.routeVisibilityMask() ==
        brooklyn::semantic::kAllRoutesVisibleMask);
  CHECK(corruptMask.setRouteVisibilityMask(UINT32_MAX));
  CHECK(corruptMask.routeVisibilityMask() ==
        brooklyn::semantic::kAllRoutesVisibleMask);
  memory.failSave = true;
  const uint32_t routeMaskBeforeFailure = corruptMask.routeVisibilityMask();
  CHECK(!corruptMask.setRouteVisibilityMask(0));
  CHECK(corruptMask.routeVisibilityMask() == routeMaskBeforeFailure);
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
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
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
  CHECK(strstr(page, "<script src=") == nullptr);
  CHECK(strstr(page, "name=\"password\"") == nullptr);
  CHECK(strstr(page, "password sentinel") == nullptr);
  CHECK(strstr(page, "Routes to display") != nullptr);
  CHECK(strstr(page, "18 of 18 routes enabled") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/routes\"") != nullptr);
  CHECK(strstr(page, "Select all") != nullptr);
  CHECK(strstr(page, "Clear all") != nullptr);
  CHECK(strstr(page, "Save routes") != nullptr);
  CHECK(strstr(page, "name=\"LED_ENABLE\"") == nullptr);
  CHECK(strstr(page, "name=\"physicalBrightness\"") == nullptr);
  CHECK(strstr(page, "name=\"channel\"") == nullptr);
  for (uint8_t route = 1; route <= brooklyn::semantic::kRouteCount; ++route) {
    char checkbox[64];
    snprintf(checkbox, sizeof(checkbox),
             "name=\"route\" value=\"%s\" checked",
             brooklyn::semantic::kRouteNames[route]);
    CHECK(strstr(page, checkbox) != nullptr);
  }

  strncpy(status.ssid, "<test&\"wifi'>", sizeof(status.ssid) - 1);
  strncpy(status.deviceName, "<script>alert(1)</script>",
          sizeof(status.deviceName) - 1);
  strncpy(status.resetReason, "boot\nready", sizeof(status.resetReason) - 1);
  const uint32_t hiddenRoutes =
      brooklyn::semantic::kAllRoutesVisibleMask &
      ~brooklyn::semantic::routeVisibilityBit(
          brooklyn::semantic::Route::RouteA);
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 0,
                                      hiddenRoutes) > 0);
  CHECK(strstr(page, "&lt;test&amp;&quot;wifi&#39;&gt;") != nullptr);
  CHECK(strstr(page, "&lt;script&gt;alert(1)&lt;/script&gt;") != nullptr);
  CHECK(strstr(page, "<script>alert(1)</script>") == nullptr);
  CHECK(strstr(page, "boot&#x0a;ready") != nullptr);
  CHECK(strstr(page, "Brightness: 0 / 255") != nullptr);
  CHECK(strstr(page, "17 of 18 routes enabled") != nullptr);
  CHECK(strstr(page, "name=\"route\" value=\"A\" checked") == nullptr);
  CHECK(strstr(page, "name=\"route\" value=\"2\" checked") != nullptr);
  CHECK(strstr(page, "http://brooklynmap.local/") != nullptr);

  status.mdnsAvailable = false;
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 255, 0) > 0);
  CHECK(strstr(page, "brooklynmap.local") == nullptr);
  CHECK(strstr(page, "Brightness: 255 / 255") != nullptr);
  CHECK(strstr(page, "0 of 18 routes enabled") != nullptr);

  strncpy(status.ip, "javascript:alert(1)", sizeof(status.ip) - 1);
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
  CHECK(strstr(page, "javascript:") == nullptr);
  CHECK(strstr(page, "Unavailable</dd>") != nullptr);
  status.hasDataAge = false;
  status.hasCompleteAge = false;
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
  CHECK(strstr(page, "<dt>Last usable update age</dt><dd>Not available</dd>") !=
        nullptr);
  CHECK(brooklyn::web::renderHomePage(
            page, 8, status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) == 0);
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
  CHECK(brooklyn::web::renderStatusJson(
            json, sizeof(json), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
  CHECK(strstr(json, "\"physicalLedOutputEnabled\":false") != nullptr);
  CHECK(strstr(json, "\"resetReason\":\"boot\\rready\"") != nullptr);
  CHECK(strstr(json, "\"brightness\":64") != nullptr);
  CHECK(strstr(json, "\"routeVisibilityMask\":262143,\"routeCount\":18") !=
        nullptr);
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
  CHECK(brooklyn::web::renderStatusJson(json, sizeof(json), status, 64, 0) >
        0);
  CHECK(strstr(json, "\"dataAgeSeconds\":null") != nullptr);
  CHECK(strstr(json, "\"completeAgeSeconds\":null") != nullptr);
  CHECK(brooklyn::web::renderStatusJson(
            json, 8, status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) == 0);
}

void testRouteSelectionParsing() {
  using brooklyn::web::RouteSelectionParam;
  RouteSelectionParam all[brooklyn::semantic::kRouteCount + 1] = {
      {"routeForm", "1"}};
  for (uint8_t route = 1; route <= brooklyn::semantic::kRouteCount; ++route) {
    all[route] = {"route", brooklyn::semantic::kRouteNames[route]};
  }
  uint32_t mask = 0;
  CHECK(brooklyn::web::parseRouteSelection(
      all, brooklyn::semantic::kRouteCount + 1, mask));
  CHECK(mask == brooklyn::semantic::kAllRoutesVisibleMask);

  const RouteSelectionParam clearAll[] = {{"routeForm", "1"}};
  CHECK(brooklyn::web::parseRouteSelection(clearAll, 1, mask));
  CHECK(mask == 0);

  const RouteSelectionParam partial[] = {
      {"routeForm", "1"}, {"route", "2"}, {"route", "A"}};
  CHECK(brooklyn::web::parseRouteSelection(partial, 3, mask));
  CHECK(mask == (brooklyn::semantic::routeVisibilityBit(
                     brooklyn::semantic::Route::Route2) |
                 brooklyn::semantic::routeVisibilityBit(
                     brooklyn::semantic::Route::RouteA)));

  const RouteSelectionParam unknownRoute[] = {
      {"routeForm", "1"}, {"route", "X"}};
  CHECK(!brooklyn::web::parseRouteSelection(unknownRoute, 2, mask));
  const RouteSelectionParam unknownParameter[] = {
      {"routeForm", "1"}, {"brightness", "0"}};
  CHECK(!brooklyn::web::parseRouteSelection(unknownParameter, 2, mask));
  const RouteSelectionParam duplicate[] = {
      {"routeForm", "1"}, {"route", "2"}, {"route", "2"}};
  CHECK(!brooklyn::web::parseRouteSelection(duplicate, 3, mask));
  const RouteSelectionParam malformedMarker[] = {{"routeForm", "0"}};
  CHECK(!brooklyn::web::parseRouteSelection(malformedMarker, 1, mask));
  const RouteSelectionParam malformedMask[] = {
      {"routeForm", "1"}, {"routeMask", "not-a-mask"}};
  CHECK(!brooklyn::web::parseRouteSelection(malformedMask, 2, mask));
  const RouteSelectionParam missingMarker[] = {{"route", "2"}};
  CHECK(!brooklyn::web::parseRouteSelection(missingMarker, 1, mask));
  CHECK(!brooklyn::web::parseRouteSelection(nullptr, 0, mask));
  CHECK(!brooklyn::web::parseRouteSelection(all,
                                            brooklyn::semantic::kRouteCount + 2,
                                            mask));
}

void testRouteVisibilityApiMasks() {
  char json[2048];
  const brooklyn::web::StatusSnapshot status = sampleStatus();
  const uint32_t masks[] = {
      brooklyn::semantic::kAllRoutesVisibleMask,
      brooklyn::semantic::kAllRoutesVisibleMask &
          ~brooklyn::semantic::routeVisibilityBit(
              brooklyn::semantic::Route::RouteA),
      brooklyn::semantic::kAllRoutesVisibleMask &
          ~brooklyn::semantic::routeVisibilityBit(
              brooklyn::semantic::Route::RouteA) &
          ~brooklyn::semantic::routeVisibilityBit(
              brooklyn::semantic::Route::RouteL),
      0,
  };
  for (uint32_t mask : masks) {
    CHECK(brooklyn::web::renderStatusJson(json, sizeof(json), status, 64, mask) >
          0);
    char expected[64];
    snprintf(expected, sizeof(expected), "\"routeVisibilityMask\":%lu",
             static_cast<unsigned long>(mask));
    CHECK(strstr(json, expected) != nullptr);
    CHECK(strstr(json, "\"routeCount\":18") != nullptr);
    CHECK(json[0] == '{' && json[strlen(json) - 1] == '}');
    printf("JSON_TEST:%s\n", json);
  }
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
  testRouteVisibilityPersistenceAndMigration();
  testPageEscapingAndRequiredContent();
  testJsonStatusIsEscapedAndSecretFree();
  testRouteVisibilityApiMasks();
  testRouteSelectionParsing();
  testServerOwnershipTransitions();
  char page[6144];
  char json[2048];
  const brooklyn::web::StatusSnapshot status = sampleStatus();
  printf("Sample generated response sizes: HTML %lu B, status JSON %lu B\n",
         static_cast<unsigned long>(brooklyn::web::renderHomePage(
             page, sizeof(page), status, 64,
             brooklyn::semantic::kAllRoutesVisibleMask)),
         static_cast<unsigned long>(brooklyn::web::renderStatusJson(
             json, sizeof(json), status, 64,
             brooklyn::semantic::kAllRoutesVisibleMask)));
  printf("LAN web policy tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
