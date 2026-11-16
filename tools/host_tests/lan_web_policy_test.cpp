// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "admin_auth.h"
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

size_t countSubstring(const char *text, const char *needle) {
  size_t count = 0;
  const size_t needleLength = strlen(needle);
  for (const char *cursor = text; (cursor = strstr(cursor, needle)) != nullptr;
       cursor += needleLength) {
    ++count;
  }
  return count;
}

struct MemoryStore {
  bool brightnessPresent = false;
  bool routeMaskPresent = false;
  bool dnsOverridePresent = false;
  bool dnsOverrideEnabled = false;
  bool failSave = false;
  uint32_t brightnessValue = 0;
  uint32_t routeMaskValue = 0;
  uint32_t dnsOverrideIpv4 = 0;
  uint32_t saves = 0;
  uint32_t pollSeconds = 0;
  uint32_t oledAddress = 0;
  uint32_t blueHold = 0;
  uint32_t oledContrast = UINT32_MAX;
  uint32_t oledReturn = UINT32_MAX;
  uint64_t screenPrefs = UINT64_MAX;
  uint32_t unknownTiming = UINT32_MAX;
  bool colorsPresent=false;
  brooklyn::semantic::TrainColors colors=brooklyn::semantic::kDefaultTrainColors;
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

bool loadDnsOverride(void *context, bool &enabled, uint32_t &ipv4) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (!store.dnsOverridePresent) return false;
  enabled = store.dnsOverrideEnabled;
  ipv4 = store.dnsOverrideIpv4;
  return true;
}

bool saveDnsOverride(void *context, bool enabled, uint32_t ipv4) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failSave) return false;
  store.dnsOverridePresent = true;
  store.dnsOverrideEnabled = enabled;
  store.dnsOverrideIpv4 = ipv4;
  ++store.saves;
  return true;
}

bool loadPoll(void *context, uint32_t &value) {
  value = static_cast<MemoryStore *>(context)->pollSeconds;
  return value != 0;
}
bool savePoll(void *context, uint32_t value) {
  auto &memory = *static_cast<MemoryStore *>(context);
  if (memory.failSave) return false;
  memory.pollSeconds = value;
  return true;
}
bool loadOled(void *context, uint32_t &value) {
  value = static_cast<MemoryStore *>(context)->oledAddress; return value != 0;
}
bool saveOled(void *context, uint32_t value) {
  auto &memory = *static_cast<MemoryStore *>(context);
  if (memory.failSave) return false;
  memory.oledAddress = value; return true;
}
bool loadBlueHold(void *context, uint32_t &out) {
  out = static_cast<MemoryStore *>(context)->blueHold; return out != 0;
}
bool saveBlueHold(void *context, uint32_t value) {
  auto &memory = *static_cast<MemoryStore *>(context);
  if (memory.failSave) return false;
  memory.blueHold = value; return true;
}
bool loadScreenPrefs(void *context,uint64_t &out) {out=static_cast<MemoryStore *>(context)->screenPrefs;return out!=UINT64_MAX;}
bool saveScreenPrefs(void *context,uint64_t value) {auto &m=*static_cast<MemoryStore *>(context);if(m.failSave) return false;m.screenPrefs=value;return true;}
bool loadReturn(void *context,uint32_t &out) {out=static_cast<MemoryStore *>(context)->oledReturn;return out!=UINT32_MAX;}
bool saveReturn(void *context,uint32_t value) {auto &m=*static_cast<MemoryStore *>(context);if(m.failSave)return false;m.oledReturn=value;return true;}
bool loadContrast(void *context, uint32_t &out) {
  out = static_cast<MemoryStore *>(context)->oledContrast; return out != UINT32_MAX;
}
bool saveContrast(void *context, uint32_t value) {
  auto &memory = *static_cast<MemoryStore *>(context);
  if (memory.failSave) return false;
  memory.oledContrast = value; return true;
}
bool loadUnknown(void *context, uint32_t &out) {
  out = static_cast<MemoryStore *>(context)->unknownTiming; return out != UINT32_MAX;
}
bool saveUnknown(void *context, uint32_t value) {
  auto &memory = *static_cast<MemoryStore *>(context);
  if (memory.failSave) return false;
  memory.unknownTiming = value; return true;
}
bool loadColors(void *context,brooklyn::semantic::TrainColors &value) {
  auto &memory=*static_cast<MemoryStore *>(context); if (!memory.colorsPresent) return false;
  value=memory.colors; return true;
}
bool saveColors(void *context,const brooklyn::semantic::TrainColors &value) {
  auto &memory=*static_cast<MemoryStore *>(context); if (memory.failSave) return false;
  memory.colors=value; memory.colorsPresent=true; return true;
}
brooklyn::config::Store configStore(MemoryStore &memory) {
  return {&memory, loadBrightness, saveBrightness, loadRouteVisibilityMask,
          saveRouteVisibilityMask, loadDnsOverride, saveDnsOverride, loadPoll, savePoll, loadOled, saveOled, loadBlueHold, saveBlueHold, loadContrast, saveContrast, loadUnknown, saveUnknown, loadColors, saveColors, loadReturn, saveReturn, loadScreenPrefs, saveScreenPrefs};
}

void testTrainColors() {
  using namespace brooklyn;
  MemoryStore memory; config::DeviceConfiguration c(configStore(memory)); c.load();
  CHECK(semantic::sameTrainColors(c.trainColors(),semantic::kDefaultTrainColors));
  led::Rgb color{};
  CHECK(config::parseTrainColor("#ffff00",color) && semantic::sameRgb(color,{255,255,0}));
  CHECK(config::parseTrainColor("#123aBC",color) && semantic::sameRgb(color,{0x12,0x3a,0xbc}));
  const char *bad[]={"", "#", "123456", "#12345", "#1234567", "#12xz56", " #123456", "#123456 ", "#123456;bad"};
  for (const auto *text:bad) CHECK(!config::parseTrainColor(text,color));
  CHECK(!config::parseTrainColor(nullptr,color));
  semantic::TrainColors chosen{{10,20,30},{40,50,60},{70,80,90},{100,110,120}};
  CHECK(c.setTrainColors(chosen)); config::DeviceConfiguration reload(configStore(memory)); reload.load();
  CHECK(semantic::sameTrainColors(reload.trainColors(),chosen));
  memory.failSave=true; CHECK(!c.setTrainColors(semantic::kDefaultTrainColors));
  CHECK(semantic::sameTrainColors(c.trainColors(),chosen));
}

void testBlueHoldConfiguration() {
  using namespace brooklyn::config;
  MemoryStore memory; DeviceConfiguration config(configStore(memory));
  config.load(); CHECK(config.blueHoldSeconds() == 20);
  CHECK(config.unknownTiming() == brooklyn::config::kDefaultUnknownTiming);
  CHECK(brooklyn::semantic::unknownTimingMode(config.unknownTiming())==brooklyn::semantic::UnknownTimingMode::LastReported);
  CHECK(brooklyn::semantic::lastReportedFadeSeconds(config.unknownTiming())==60);
  CHECK(config.setUnknownTiming((120u<<10)|512u|1u));
  DeviceConfiguration modeReload(configStore(memory)); modeReload.load();
  CHECK(modeReload.unknownTiming()==((120u<<10)|512u|1u));
  CHECK(!config.setUnknownTiming((301u<<10)|512u));
  CHECK(config.setUnknownTiming(511)); // Legacy saved orange setting still reads correctly.
  CHECK(brooklyn::semantic::unknownTimingMode(config.unknownTiming())==brooklyn::semantic::UnknownTimingMode::Orange);
  CHECK(brooklyn::semantic::lastReportedFadeSeconds(config.unknownTiming())==60);
  CHECK(config.setUnknownTiming(257));
  DeviceConfiguration uncertain(configStore(memory)); uncertain.load(); CHECK(uncertain.unknownTiming() == 257);
  CHECK(config.setUnknownTiming(0)); uncertain.load(); CHECK(uncertain.unknownTiming() == 0);
  CHECK(config.setUnknownTiming(1)); uncertain.load(); CHECK(uncertain.unknownTiming() == 1);
  memory.failSave = true; CHECK(!config.setUnknownTiming(511) && config.unknownTiming() == 1);
  memory.failSave = false; CHECK(!config.setUnknownTiming(768));
  memory.unknownTiming = 1000; uncertain.load(); CHECK(uncertain.unknownTiming() == brooklyn::config::kDefaultUnknownTiming);
  CHECK(config.oledReturnSeconds()==60);
  CHECK(config.screenPreferences()==kDefaultScreenPreferences);
  ScreenPreferences pref;pref.quietEnabled=true;pref.startupDedication=false;pref.quietMapBrightness=4;
  const uint64_t word=packScreenPreferences(pref);
  CHECK(config.setScreenPreferences(word));DeviceConfiguration prefReloaded(configStore(memory));prefReloaded.load();CHECK(prefReloaded.screenPreferences()==word);
  memory.failSave=true;CHECK(!config.setScreenPreferences(kDefaultScreenPreferences) && config.screenPreferences()==word);memory.failSave=false;
  memory.screenPrefs=UINT64_MAX;prefReloaded.load();CHECK(prefReloaded.screenPreferences()==kDefaultScreenPreferences);
  char screenPage[6144];
  CHECK(brooklyn::web::renderDefaultsPage(screenPage,sizeof(screenPage),"token<&>")>0);
  CHECK(strstr(screenPage,"token&lt;&amp;&gt;")!=nullptr);
  CHECK(strstr(screenPage,"method=\"post\" action=\"/defaults\"")!=nullptr);
  CHECK(strstr(screenPage,"name=\"confirm\" value=\"erase\" required")!=nullptr);
  CHECK(strstr(screenPage,"administrator password") && strstr(screenPage,"BrooklynMap-Avi"));
  CHECK(brooklyn::web::renderDefaultsPage(screenPage,16,"token")==0);
  CHECK(brooklyn::web::renderDefaultsPage(screenPage,sizeof(screenPage),nullptr)==0);
  CHECK(brooklyn::web::renderDefaultsPage(screenPage,sizeof(screenPage),"")==0);
  CHECK(strcmp(brooklyn::web::loginReturnPath("defaults"),"/defaults")==0);
  CHECK(strcmp(brooklyn::web::loginTargetForRequest("/defaults"),"defaults")==0);
  CHECK(brooklyn::web::renderScreenOptionsPage(screenPage,sizeof(screenPage),kDefaultScreenPreferences,false,"token<&>")>0);
  CHECK(strstr(screenPage,"token&lt;&amp;&gt;")!=nullptr);
  CHECK(strstr(screenPage,"name=\"startup\" value=\"1\" checked")!=nullptr);
  CHECK(strstr(screenPage,"name=\"quiet\" value=\"1\" checked")==nullptr);
  CHECK(strstr(screenPage,"name=\"waitWifi\" value=\"1\" checked")!=nullptr);
  CHECK(strstr(screenPage,"22:00") && strstr(screenPage,"07:00"));
  CHECK(brooklyn::web::renderScreenOptionsPage(screenPage,16,kDefaultScreenPreferences,false,"token")==0);
  CHECK(brooklyn::web::renderScreenOptionsPage(screenPage,sizeof(screenPage),UINT32_MAX,false,"token")==0);
  CHECK(strcmp(brooklyn::web::loginReturnPath("screen-options"),"/screen-options")==0);
  CHECK(strcmp(brooklyn::web::loginTargetForRequest("/config/screen-options"),"screen-options")==0);

  CHECK(config.setOledReturnSeconds(0));
  DeviceConfiguration returned(configStore(memory));returned.load();CHECK(returned.oledReturnSeconds()==0);
  CHECK(config.setOledReturnSeconds(300));returned.load();CHECK(returned.oledReturnSeconds()==300);
  memory.failSave=true;CHECK(!config.setOledReturnSeconds(30) && config.oledReturnSeconds()==300);memory.failSave=false;
  CHECK(!config.setOledReturnSeconds(31));memory.oledReturn=31;returned.load();CHECK(returned.oledReturnSeconds()==60);
  uint32_t returnValue=99;
  CHECK(parseOledReturnSeconds("0",returnValue) && returnValue==0);
  CHECK(parseOledReturnSeconds("120",returnValue) && returnValue==120);
  CHECK(!parseOledReturnSeconds("9999999999999999",returnValue));
  CHECK(!parseOledReturnSeconds("30x",returnValue));CHECK(!parseOledReturnSeconds("-1",returnValue));
  CHECK(!parseOledReturnSeconds(nullptr,returnValue));CHECK(!parseOledReturnSeconds("31",returnValue));
  CHECK(config.oledContrast() == 207);
  CHECK(config.setOledContrast(0));
  DeviceConfiguration dimmed(configStore(memory)); dimmed.load(); CHECK(dimmed.oledContrast() == 0);
  CHECK(config.setOledContrast(255));
  dimmed.load(); CHECK(dimmed.oledContrast() == 255);
  memory.failSave = true; CHECK(!config.setOledContrast(64) && config.oledContrast() == 255);
  memory.failSave = false; memory.oledContrast = 300; dimmed.load(); CHECK(dimmed.oledContrast() == 207);
  uint32_t seconds = 0;
  CHECK(parseBlueHoldSeconds("20", seconds) && seconds == 20);
  CHECK(parseBlueHoldSeconds("1", seconds) && seconds == 1);
  CHECK(parseBlueHoldSeconds("300", seconds) && seconds == 300);
  const char *badValues[] = {"", "0", "301", "-1", "+20", "20x", " 20", "9999999999999"};
  for (const char *bad : badValues)
    CHECK(!parseBlueHoldSeconds(bad, seconds));
  CHECK(!parseBlueHoldSeconds(nullptr, seconds));
  CHECK(config.setBlueHoldSeconds(45));
  DeviceConfiguration reload(configStore(memory)); reload.load(); CHECK(reload.blueHoldSeconds() == 45);
  memory.failSave = true; CHECK(!config.setBlueHoldSeconds(60) && config.blueHoldSeconds() == 45);
  CHECK(!config.setBlueHoldSeconds(0)); CHECK(!config.setBlueHoldSeconds(301));
  memory.blueHold = 999; reload.load(); CHECK(reload.blueHoldSeconds() == 20);
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
  CHECK(config.brightness() == 5);
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
  CHECK(corrupt.brightness() == 5);
  memory.failSave = true;
  const uint32_t savesBeforeFailure = memory.saves;
  CHECK(!config.setBrightness(20));
  CHECK(config.brightness() == 255);
  CHECK(memory.brightnessValue == 256);
  CHECK(memory.saves == savesBeforeFailure);
  CHECK(brooklyn::led::power::PRODUCTION_BRINGUP_BRIGHTNESS_CAP == 16);
  CHECK(sizeof(brooklyn::config::DeviceConfiguration) <= sizeof(brooklyn::config::Store) + 48);
}

void testDnsConfigurationAndValidation() {
  using brooklyn::config::DeviceConfiguration;
  MemoryStore memory;
  DeviceConfiguration config(configStore(memory));
  CHECK(!config.load());
  CHECK(!config.dnsOverrideEnabled());
  CHECK(config.dnsOverrideIpv4() == 0);

  uint32_t address = 0;
  CHECK(brooklyn::config::parseDnsIpv4("1.1.1.1", address));
  CHECK(address == 0x01010101);
  CHECK(brooklyn::config::parseDnsIpv4("192.168.137.25", address));
  CHECK(address == 0xc0a88919);
  CHECK(brooklyn::config::validDnsIpv4(address));
  char formatted[16] = {};
  CHECK(brooklyn::config::formatDnsIpv4(address, formatted,
                                         sizeof(formatted)) == 14);
  CHECK(strcmp(formatted, "192.168.137.25") == 0);
  CHECK(!brooklyn::config::parseDnsIpv4(nullptr, address));
  CHECK(!brooklyn::config::parseDnsIpv4("", address));
  CHECK(!brooklyn::config::parseDnsIpv4("1.1.1", address));
  CHECK(!brooklyn::config::parseDnsIpv4("1.1.1.1.1", address));
  CHECK(!brooklyn::config::parseDnsIpv4("256.1.1.1", address));
  CHECK(!brooklyn::config::parseDnsIpv4("1.1.1.999", address));
  CHECK(!brooklyn::config::parseDnsIpv4("01.1.1.1x", address));
  CHECK(!brooklyn::config::parseDnsIpv4("1.1.1.1 ", address));
  CHECK(!brooklyn::config::parseDnsIpv4("0.1.1.1", address));
  CHECK(!brooklyn::config::parseDnsIpv4("127.0.0.1", address));
  CHECK(!brooklyn::config::parseDnsIpv4("224.0.0.1", address));
  CHECK(!brooklyn::config::parseDnsIpv4("255.255.255.255", address));
  CHECK(!brooklyn::config::validDnsIpv4(0));
  CHECK(!brooklyn::config::validDnsIpv4(0xe0000001));
  CHECK(!brooklyn::config::validDnsIpv4(0xffffffff));

  CHECK(config.setBrightness(77));
  CHECK(config.setRouteVisibilityMask(5));
  CHECK(!config.setDnsOverride(true, 0));
  CHECK(!memory.dnsOverridePresent);
  CHECK(config.setDnsOverride(true, 0x01010101));
  CHECK(config.dnsOverrideEnabled());
  CHECK(config.dnsOverrideIpv4() == 0x01010101);
  DeviceConfiguration reconstructed(configStore(memory));
  CHECK(reconstructed.load());
  CHECK(reconstructed.dnsOverrideEnabled());
  CHECK(reconstructed.dnsOverrideIpv4() == 0x01010101);
  CHECK(reconstructed.brightness() == 77);
  CHECK(reconstructed.routeVisibilityMask() == 5);

  const uint32_t savesBeforeInvalid = memory.saves;
  CHECK(!config.setDnsOverride(true, 0xe0000001));
  CHECK(!config.setDnsOverride(false, 0xffffffff));
  CHECK(memory.saves == savesBeforeInvalid);
  CHECK(config.setDnsOverride(false, 0x01010101));
  CHECK(!config.dnsOverrideEnabled());
  CHECK(config.dnsOverrideIpv4() == 0x01010101);
  DeviceConfiguration disabled(configStore(memory));
  CHECK(disabled.load());
  CHECK(!disabled.dnsOverrideEnabled());
  CHECK(disabled.dnsOverrideIpv4() == 0x01010101);

  memory.dnsOverrideEnabled = true;
  memory.dnsOverrideIpv4 = 0xe0000001;
  DeviceConfiguration corrupt(configStore(memory));
  CHECK(corrupt.load());
  CHECK(!corrupt.dnsOverrideEnabled());
  CHECK(corrupt.dnsOverrideIpv4() == 0);

  memory.dnsOverridePresent = false;
  memory.dnsOverrideEnabled = false;
  memory.dnsOverrideIpv4 = 0;
  memory.brightnessPresent = true;
  memory.brightnessValue = 71;
  memory.routeMaskPresent = true;
  memory.routeMaskValue = 5;
  DeviceConfiguration upgraded(configStore(memory));
  CHECK(upgraded.load());
  CHECK(!upgraded.dnsOverrideEnabled());
  CHECK(upgraded.brightness() == 71);
  CHECK(upgraded.routeVisibilityMask() == 5);
  memory.failSave = true;
  CHECK(!upgraded.setDnsOverride(true, 0x08080808));
  CHECK(!upgraded.dnsOverrideEnabled());
  CHECK(upgraded.brightness() == 71);
  CHECK(upgraded.routeVisibilityMask() == 5);
}

void testDnsSettingsFormParsing() {
  using brooklyn::web::DnsSettingsParam;
  bool enabled = false;
  uint32_t address = 0;
  const DnsSettingsParam enabledForm[] = {
      {"dnsForm", "1"}, {"dnsServer", "1.1.1.1"},
      {"manualDnsEnabled", "1"}};
  CHECK(brooklyn::web::parseDnsSettings(enabledForm, 3, enabled, address));
  CHECK(enabled && address == 0x01010101);

  const DnsSettingsParam disabledForm[] = {
      {"dnsForm", "1"}, {"dnsServer", ""}};
  CHECK(brooklyn::web::parseDnsSettings(disabledForm, 2, enabled, address));
  CHECK(!enabled && address == 0);
  const DnsSettingsParam invalidAddress[] = {
      {"dnsForm", "1"}, {"dnsServer", "224.0.0.1"}};
  CHECK(!brooklyn::web::parseDnsSettings(invalidAddress, 2, enabled, address));
  const DnsSettingsParam missingAddress[] = {
      {"dnsForm", "1"}, {"manualDnsEnabled", "1"}};
  CHECK(!brooklyn::web::parseDnsSettings(missingAddress, 2, enabled, address));
  const DnsSettingsParam duplicate[] = {
      {"dnsForm", "1"}, {"dnsServer", "1.1.1.1"},
      {"dnsServer", "8.8.8.8"}};
  CHECK(!brooklyn::web::parseDnsSettings(duplicate, 3, enabled, address));
  const DnsSettingsParam unknownParameter[] = {
      {"dnsForm", "1"}, {"dnsServer", "1.1.1.1"}, {"other", "1"}};
  CHECK(!brooklyn::web::parseDnsSettings(unknownParameter, 3, enabled,
                                         address));
  const DnsSettingsParam malformedMarker[] = {
      {"dnsForm", "0"}, {"dnsServer", ""}};
  CHECK(!brooklyn::web::parseDnsSettings(malformedMarker, 2, enabled, address));
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
  strncpy(status.dnsPrimary, "0.0.0.0", sizeof(status.dnsPrimary) - 1);
  strncpy(status.dnsSecondary, "192.168.137.1",
          sizeof(status.dnsSecondary) - 1);
  status.manualDnsEnabled = true;
  status.manualDnsApplied = true;
  strncpy(status.manualDnsServer, "1.1.1.1",
          sizeof(status.manualDnsServer) - 1);
  strncpy(status.effectiveDnsServer, "1.1.1.1",
          sizeof(status.effectiveDnsServer) - 1);
  strncpy(status.effectiveDnsSource, "Manual",
          sizeof(status.effectiveDnsSource) - 1);
  strncpy(status.dnsConfigStatus, "applied",
          sizeof(status.dnsConfigStatus) - 1);
  strncpy(status.ip, "192.0.2.17", sizeof(status.ip) - 1);
  status.rssi = -58;
  status.mdnsAvailable = true;
  status.timeSynchronized = true;
  status.displayState = brooklyn::display::State::Fresh;
  status.hasCycle = true;
  status.cycleOutcome = brooklyn::transport::CycleOutcome::Complete;
  status.processedFeedMask = 255;
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
  status.transportCounters.secureConnectFailures = 2;
  status.transportCounters.httpFailures = 3;
  status.transportCounters.timeouts = 4;
  status.transportCounters.bytesDownloaded = 123456;
  status.hasLastFeedFailure = true;
  strncpy(status.lastFeedFailureName, "NQRW",
          sizeof(status.lastFeedFailureName) - 1);
  strncpy(status.lastFeedFailureCategory, "timed-out",
          sizeof(status.lastFeedFailureCategory) - 1);
  status.lastFeedFailureBytes = 143;
  status.lastFeedFailureElapsedMs = 321;
  status.networkDiagnostics.state =
      brooklyn::network::diagnostics::RunState::Complete;
  status.networkDiagnostics.wifiAssociated = true;
  strncpy(status.networkDiagnostics.ip, "192.0.2.17",
          sizeof(status.networkDiagnostics.ip) - 1);
  strncpy(status.networkDiagnostics.dnsPrimary, "192.0.2.1",
          sizeof(status.networkDiagnostics.dnsPrimary) - 1);
  status.networkDiagnostics.ntpDns.state =
      brooklyn::network::diagnostics::CheckState::Succeeded;
  strncpy(status.networkDiagnostics.ntpDns.address, "198.51.100.10",
          sizeof(status.networkDiagnostics.ntpDns.address) - 1);
  status.networkDiagnostics.ntpDns.elapsedMs = 45;
  status.networkDiagnostics.mtaDns.state =
      brooklyn::network::diagnostics::CheckState::Failed;
  status.networkDiagnostics.mtaDns.elapsedMs = 67;
  status.networkDiagnostics.tcp443.state =
      brooklyn::network::diagnostics::CheckState::Skipped;
  status.networkDiagnostics.directDns[0].state =
      brooklyn::network::diagnostics::CheckState::Succeeded;
  strncpy(status.networkDiagnostics.directDns[0].resolver, "1.1.1.1",
          sizeof(status.networkDiagnostics.directDns[0].resolver) - 1);
  strncpy(status.networkDiagnostics.directDns[0].hostname, "pool.ntp.org",
          sizeof(status.networkDiagnostics.directDns[0].hostname) - 1);
  status.networkDiagnostics.directDns[0].responseReceived = true;
  status.networkDiagnostics.directDns[0].hasRcode = true;
  status.networkDiagnostics.directDns[0].rcode = 0;
  status.networkDiagnostics.directDns[0].addressCount = 1;
  strncpy(status.networkDiagnostics.directDns[0].addresses[0], "192.0.2.53",
          sizeof(status.networkDiagnostics.directDns[0].addresses[0]) - 1);
  status.networkDiagnostics.directDns[0].elapsedMs = 18;
  return status;
}

void testFullProductionPageCapacity() {
  auto status = sampleStatus(); status.adminConfigured = status.adminSessionActive = true;
  status.physicalLiveAvailable = status.physicalLedOutputEnabled = true;
  auto &report = status.blueReport; report.timestamp = 1794615150; report.comparedTrips = 128;
  report.steadyLeds = 9; report.uncertainLeds = 86;
  for (unsigned i=1; i<=brooklyn::semantic::kRouteCount; ++i)
    report.routes[i] = {15,20,4,7,6,2,3,2};
  for (unsigned i=0; i<brooklyn::semantic::kMapLedCount && report.sampleCount<brooklyn::pipeline::kBlueSamples; ++i) {
    const auto &led = brooklyn::semantic::kMapLeds[i];
    if (led.role != brooklyn::semantic::Role::Station) continue;
    auto &sample=report.samples[report.sampleCount++]; sample.led=led.globalOrder;
    sample.trains=3; sample.age=255;
    sample.flags=brooklyn::pipeline::NextDifferent | brooklyn::pipeline::NoPrediction | brooklyn::pipeline::StopChanged;
  }
  static char expanded[brooklyn::web::kStatusPageCapacity];
  static char oldCapacity[16384]; const char *csrf="0123456789abcdef0123456789abcdef";
  CHECK(brooklyn::web::renderHomePage(oldCapacity,sizeof(oldCapacity),status,5,brooklyn::semantic::kAllRoutesVisibleMask,csrf)==0);
  const size_t size=brooklyn::web::renderHomePage(expanded,sizeof(expanded),status,5,brooklyn::semantic::kAllRoutesVisibleMask,csrf);
  CHECK(size>0); CHECK(size>16384); CHECK(strstr(expanded,"Reported STOPPED_AT candidates")!=nullptr);
  CHECK(strstr(expanded,"Save train display settings")!=nullptr);
  printf("Full production status page: %zu B\n",size);
  memset(status.ssid,'&',sizeof(status.ssid)-1);
  memset(status.deviceName,'&',sizeof(status.deviceName)-1);
  memset(status.target,'&',sizeof(status.target)-1);
  memset(status.availableVersion,'&',sizeof(status.availableVersion)-1);
  const size_t escaped=brooklyn::web::renderHomePage(expanded,sizeof(expanded),status,255,brooklyn::semantic::kAllRoutesVisibleMask,csrf);
  CHECK(escaped>size && escaped<sizeof(expanded));
  CHECK(strstr(expanded,"Save train display settings")!=nullptr);
  printf("Full escaped production status page: %zu B\n",escaped);
}

void testPageEscapingAndRequiredContent() {
  char page[brooklyn::web::kStatusPageCapacity];
  brooklyn::web::StatusSnapshot status = sampleStatus();
  status.adminConfigured = true;
  status.adminSessionActive = true;
  const char csrfToken[] = "0123456789abcdef0123456789abcdef";
  const size_t authenticatedPageSize = brooklyn::web::renderHomePage(
      page, sizeof(page), status, 64,
      brooklyn::semantic::kAllRoutesVisibleMask, csrfToken);
  CHECK(authenticatedPageSize > 0);
  CHECK(authenticatedPageSize < sizeof(page));
  CHECK(strstr(page,"data-status=\"firmwareVersion\"")!=nullptr);
  CHECK(strstr(page,"data-status=\"firmwareVersion\"") < strstr(page,"id=\"advanced\""));
  const char *views[]={"overview","lights","screen","wifi","advanced"};
  for (const char *view:views) {
    char navigation[64],anchor[64];
    snprintf(navigation,sizeof(navigation),"data-nav=\"%s\"",view);
    snprintf(anchor,sizeof(anchor),"id=\"%s\"",view);
    CHECK(countSubstring(page,navigation)==1); CHECK(strstr(page,anchor)!=nullptr);
  }
  CHECK(strstr(page,"aria-label=\"Map settings\"")!=nullptr);
  CHECK(strstr(page,">Advanced</a>")!=nullptr && strstr(page,">More</a>")==nullptr);
  CHECK(strstr(page,"<main>")!=nullptr && strstr(page,"</main>")!=nullptr);
  CHECK(strstr(page,"<output id=\"screen-level\"")!=nullptr);

  {
    static char expanded[brooklyn::web::kStatusPageCapacity];
    auto longStatus = status;
    memset(longStatus.ssid, '&', sizeof(longStatus.ssid)-1);
    memset(longStatus.deviceName, '&', sizeof(longStatus.deviceName)-1);
    memset(longStatus.target, '&', sizeof(longStatus.target)-1);
    memset(longStatus.availableVersion, '&', sizeof(longStatus.availableVersion)-1);
    CHECK(brooklyn::web::renderHomePage(expanded, sizeof(expanded), longStatus, 64,
        brooklyn::semantic::kAllRoutesVisibleMask, csrfToken) > 0);
  }
  printf("Authenticated status page: %lu B\n",
         static_cast<unsigned long>(authenticatedPageSize));
  CHECK(strstr(page, "TestNetwork") != nullptr);
  CHECK(strstr(page, "href=\"http://192.0.2.17/\"") != nullptr);
  CHECK(strstr(page, "http://brooklynmap.local/") != nullptr);
  CHECK(strstr(page, "id=\"map-level\">64</output>") != nullptr);
  CHECK(strstr(page, "<dt>Resolved trains</dt><dd data-status=\"resolvedTrains\">57</dd>") != nullptr);
  CHECK(strstr(page, "<dt>Occupied LEDs</dt><dd data-status=\"occupiedLeds\">53</dd>") != nullptr);
  CHECK(strstr(page, "<dt>Collision count</dt><dd data-status=\"collisionCount\">4</dd>") != nullptr);
  CHECK(strstr(page, "Fresh") != nullptr);
  CHECK(strstr(page, "DISABLED") != nullptr);
  CHECK(strstr(page, "power-on") != nullptr);
  CHECK(strstr(page, "Change Wi-Fi") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/wifi/setup\"") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/reboot\"") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/brightness\"") != nullptr);
  CHECK(strstr(page, "https://") == nullptr);
  CHECK(strstr(page, "cdn") == nullptr);
  CHECK(strstr(page, "src=\"/ui.js?v=038\"") != nullptr);
  CHECK(strstr(page, "href=\"/ui.css?v=038\"") != nullptr);
  CHECK(strstr(page, "value=\"password sentinel\"") == nullptr);
  CHECK(strstr(page, "password sentinel") == nullptr);
  CHECK(strstr(page, "Choose subway lines") != nullptr);
  CHECK(strstr(page, "18 of 18 lines shown") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/routes\"") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/dns\"") != nullptr);
  CHECK(strstr(page, "Use manual DNS") != nullptr);
  CHECK(strstr(page, "DHCP DNS 1") != nullptr);
  CHECK(strstr(page, "Effective DNS source") != nullptr);
  CHECK(strstr(page, "value=\"1.1.1.1\"") != nullptr);
  CHECK(strstr(page, "resolver; DHCP, Wi-Fi credentials, feeds, and TLS policy") !=
        nullptr);
  CHECK(strstr(page, "Select all") != nullptr);
  CHECK(strstr(page, "Clear all") != nullptr);
  CHECK(strstr(page, "Save subway lines") != nullptr);
  CHECK(strstr(page, "Network diagnostics") != nullptr);
  CHECK(strstr(page, "Run Network Diagnostics") != nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/network-diagnostics/run\"") !=
        nullptr);
  CHECK(countSubstring(page, csrfToken) == 14);
  CHECK(strstr(page,"There is no fixed 20-second cutoff"));
  CHECK(!strstr(page,"action=\"/config/blue-hold\""));
  // Exercise the actual generated forms against the runtime route policy.
  // A rendered POST form must pass authorizeRequest's endpoint-policy gate.
  const char *cursor = page;
  const char *prefix = "<form method=\"post\" action=\"";
  while ((cursor = strstr(cursor, prefix)) != nullptr) {
    cursor += strlen(prefix);
    const char *end = strchr(cursor, '"');
    CHECK(end != nullptr);
    if (!end) break;
    char path[96]{};
    const size_t length = static_cast<size_t>(end - cursor);
    CHECK(length < sizeof(path));
    if (length >= sizeof(path)) break;
    memcpy(path, cursor, length);
    CHECK(brooklyn::admin::isMutationEndpoint("POST", path));
    CHECK(brooklyn::admin::isProtectedEndpoint("POST", path));
    cursor = end + 1;
  }

  CHECK(strstr(page, "action=\"/config/oled\"") != nullptr);
  CHECK(strstr(page, "Saved I2C address: 0x38") != nullptr);
  CHECK(strstr(page, "Reboot after changing") != nullptr);
  CHECK(strstr(page, "MTA TCP 443") != nullptr);
  CHECK(strstr(page, "Diagnostic run</dt><dd>complete</dd>") != nullptr);
  CHECK(strstr(page, "succeeded (198.51.100.10)") != nullptr);
  CHECK(strstr(page, "1.1.1.1 -> pool.ntp.org: succeeded, response, "
                     "RCODE 0, 192.0.2.53, 18 ms") != nullptr);
  CHECK(strstr(page, "<dt>Secure-connect failures</dt><dd>2</dd>") != nullptr);
  CHECK(strstr(page, "<dt>HTTP failures</dt><dd>3</dd>") != nullptr);
  CHECK(strstr(page, "<dt>Transport timeouts</dt><dd>4</dd>") != nullptr);
  CHECK(strstr(page, "<dt>Complete-body bytes</dt><dd>123456</dd>") != nullptr);
  CHECK(strstr(page, "NQRW: timed-out, 143 bytes, 321 ms") != nullptr);
  CHECK(strstr(page, "Complete-body bytes") != nullptr);
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
                                      hiddenRoutes, csrfToken) > 0);
  CHECK(strstr(page, "&lt;test&amp;&quot;wifi&#39;&gt;") != nullptr);
  CHECK(strstr(page, "&lt;script&gt;alert(1)&lt;/script&gt;") != nullptr);
  CHECK(strstr(page, "<script>alert(1)</script>") == nullptr);
  CHECK(strstr(page, "boot&#x0a;ready") != nullptr);
  CHECK(strstr(page, "id=\"map-level\">0</output>") != nullptr);
  CHECK(strstr(page, "17 of 18 lines shown") != nullptr);
  CHECK(strstr(page, "name=\"route\" value=\"A\" checked") == nullptr);
  CHECK(strstr(page, "name=\"route\" value=\"2\" checked") != nullptr);
  CHECK(strstr(page, "http://brooklynmap.local/") != nullptr);

  status.mdnsAvailable = false;
  CHECK(brooklyn::web::renderHomePage(page, sizeof(page), status, 255, 0,
                                      csrfToken) > 0);
  CHECK(strstr(page, "brooklynmap.local") == nullptr);
  CHECK(strstr(page, "id=\"map-level\">255</output>") != nullptr);
  CHECK(strstr(page, "0 of 18 lines shown") != nullptr);

  status.adminSessionActive = false;
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 255, 0) > 0);
  CHECK(strstr(page, "method=\"post\" action=\"/config/brightness\"") ==
        nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/reboot\"") == nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/logout\"") == nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/admin/password\"") == nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/wifi/setup\"") == nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/dns\"") == nullptr);
  CHECK(strstr(page, "method=\"post\" action=\"/config/routes\"") == nullptr);
  CHECK(strstr(page,
               "method=\"post\" action=\"/network-diagnostics/run\"") ==
        nullptr);

  status.adminSessionActive = true;
  strncpy(status.ip, "javascript:alert(1)", sizeof(status.ip) - 1);
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask, csrfToken) > 0);
  CHECK(strstr(page, "javascript:") == nullptr);
  CHECK(strstr(page, "Unavailable</dd>") != nullptr);
  status.hasDataAge = false;
  status.hasCompleteAge = false;
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask, csrfToken) > 0);
  CHECK(strstr(page, "<dt>Last usable update age</dt><dd data-status=\"dataAgeSeconds\">Not available</dd>") !=
        nullptr);
  CHECK(brooklyn::web::renderHomePage(
            page, 8, status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask, csrfToken) == 0);
}

void testInlineLoginOnStatusPage() {
  static char page[brooklyn::web::kStatusPageCapacity];
  brooklyn::web::StatusSnapshot status{};
  status.adminConfigured = true;
  status.adminSessionActive = false;
  const char token[] = "abcdef0123456789abcdef0123456789";
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask, token) > 0);
  CHECK(strstr(page, "action=\"/login\"") != nullptr);
  CHECK(strstr(page, "action=\"/config/oled\"") == nullptr);
  CHECK(strstr(page, token) != nullptr);
  CHECK(strstr(page, "type=\"password\"") != nullptr);
  CHECK(strstr(page, "http-equiv=\"refresh\"") == nullptr);
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask, nullptr) > 0);
  CHECK(strstr(page, "<a href=\"/login\">Sign in</a>") != nullptr);
  CHECK(strstr(page, "http-equiv=\"refresh\"") == nullptr);
  CHECK(strstr(page, "fetch('/api/status'") != nullptr);
  CHECK(strstr(page, "setTimeout(refresh,5000)") != nullptr);
  CHECK(strstr(page, "e.textContent=") != nullptr);
  status.adminConfigured = false;
  CHECK(brooklyn::web::renderHomePage(
            page, sizeof(page), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask, token) > 0);
  CHECK(strstr(page, "action=\"/login\"") == nullptr);
}

void testLoginPageCsrfRendering() {
  char page[4096] = {};
  const char csrf[] = "abcdef0123456789abcdef0123456789";
  CHECK(brooklyn::web::renderLoginPage(page, sizeof(page), csrf) > 0);
  CHECK(strstr(page, "method=\"post\" action=\"/login\"") != nullptr);
  CHECK(strstr(page, "name=\"csrf\"") != nullptr);
  CHECK(strstr(page, csrf) != nullptr);
  CHECK(strstr(page, "type=\"password\"") != nullptr);
  CHECK(strstr(page, "value=\"password") == nullptr);
  CHECK(brooklyn::web::renderLoginPage(page, sizeof(page), nullptr) == 0);
  CHECK(brooklyn::web::renderLoginPage(page,sizeof(page),csrf,"update")>0);
  CHECK(strstr(page,"name=\"next\" value=\"update\"")!=nullptr);
  CHECK(strcmp(brooklyn::web::loginReturnPath("update"),"/update")==0);
  CHECK(strcmp(brooklyn::web::loginReturnPath("screen"),"/#screen")==0);
  CHECK(strcmp(brooklyn::web::loginReturnPath("hardware-test"),"/hardware-test")==0);
  CHECK(strcmp(brooklyn::web::loginReturnPath("wifi-setup"),"/wifi/setup")==0);
  CHECK(brooklyn::web::loginReturnPath("https://example.com")==nullptr);
  CHECK(brooklyn::web::loginReturnPath("//example.com")==nullptr);
  CHECK(brooklyn::web::loginReturnPath("/reboot")==nullptr);
  CHECK(brooklyn::web::loginReturnPath("update\r\nLocation: evil")==nullptr);
  CHECK(brooklyn::web::renderLoginPage(page,sizeof(page),csrf,"<script>")==0);
  const char *paths[]={"/update","/update/install","/update/reboot","/update/progress","/hardware-test/run","/wifi/setup/confirm","/config/oled-dimmer","/config/colors","/config/dns","/admin/password","/reboot"};
  const char *destinations[]={"/update","/update","/update","/update","/hardware-test","/wifi/setup","/#screen","/#lights","/#wifi","/#advanced","/#advanced"};
  for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i)
    CHECK(strcmp(brooklyn::web::loginReturnPath(brooklyn::web::loginTargetForRequest(paths[i])),destinations[i])==0);

}

void testRuntimeObservation() {
  using namespace brooklyn::observation;
  RetainedBoot retained{};
  auto first=startBoot(retained,true,false,1,2);
  CHECK(first.boots==1 && !first.previousAvailable && retainedValid(retained));
  checkpoint(retained,12345,60000,30000);
  auto warm=startBoot(retained,false,true,3,4);
  CHECK(warm.boots==2 && warm.unexpected==1 && warm.previousCheckpointAvailable);
  CHECK(warm.previousHigh==1 && warm.previousLow==2 && warm.previousUptimeMs==12345);
  CHECK(warm.previousFreeHeap==60000 && warm.previousMinHeap==30000);
  checkpoint(retained,20000,55000,28000);
  auto software=startBoot(retained,false,false,5,6);CHECK(software.boots==3 && software.unexpected==1);
  retained.freeHeap^=1;CHECK(!retainedValid(retained));
  auto corrupt=startBoot(retained,false,true,7,8);CHECK(!corrupt.previousAvailable && corrupt.boots==1 && corrupt.unexpected==1);
  auto cold=startBoot(retained,true,false,9,10);CHECK(cold.boots==1 && cold.unexpected==0 && !cold.previousAvailable);
  retained.boots=retained.unexpected=UINT32_MAX;retained.checksum=bootChecksum(retained);
  auto saturated=startBoot(retained,false,true,11,12);CHECK(saturated.boots==UINT32_MAX && saturated.unexpected==UINT32_MAX);
  RuntimeReport report{};report.boot=warm;report.resetReason="panic";report.deviceId=0x123456789abcdef0ull;
  ObservedFailure failure{};failure.failure.name="NQRW";failure.failure.status=brooklyn::transport::DownloadStatus::SecureConnectFailure;
  failure.details.captured=true;failure.details.httpCode=-1;failure.details.tlsError=-32512;
  failure.details.heapBefore=50000;failure.details.largestBefore=24000;
  failure.details.heapBeforeClose=45000;failure.details.largestBeforeClose=19000;
  for(unsigned i=0;i<12;++i)report.record(failure);
  CHECK(report.count==8 && report.newestSequence==12);
  char json[6144];CHECK(brooklyn::web::renderRuntimeJson(json,sizeof(json),report)>0);
  CHECK(strstr(json,"\"deviceId\":\"123456789abcdef0\"") && strstr(json,"\"oldestFailureSequence\":5"));
  CHECK(strstr(json,"\"tlsError\":-32512") && strstr(json,"\"largestBefore\":24000"));
  CHECK(brooklyn::web::renderRuntimeJson(json,30,report)==0);
  failure.details.tlsError=0;report.record(failure);CHECK(brooklyn::web::renderRuntimeJson(json,sizeof(json),report)>0);
  CHECK(strstr(json,"\"tlsError\":null"));
  failure.failure.name="1-7,S";failure.failure.bytesReceived=UINT32_MAX;failure.failure.elapsedMs=UINT32_MAX;
  failure.details.httpCode=INT32_MIN;failure.details.tlsError=INT32_MIN;
  failure.details.startedAtMs=failure.details.unixTime=failure.details.heapBefore=failure.details.largestBefore=UINT32_MAX;
  failure.details.heapBeforeClose=failure.details.largestBeforeClose=failure.details.minimumHeap=UINT32_MAX;
  for(unsigned i=0;i<8;++i)report.record(failure);
  CHECK(brooklyn::web::renderRuntimeJson(json,sizeof(json),report)>0);

}

void testIndicationApi() {
  uint32_t cursor=17;
  CHECK(brooklyn::web::parseIndicationCursor("0",cursor)&&cursor==0);
  CHECK(brooklyn::web::parseIndicationCursor("4294967295",cursor)&&cursor==UINT32_MAX);
  const char *badInputs[]={"","-1"," 1","1x","4294967296","1&after=2"};
  for(const char *bad:badInputs)CHECK(!brooklyn::web::parseIndicationCursor(bad,cursor));
  brooklyn::display::IndicationReport r{};r.available=r.outputEnabled=true;r.nowMs=50000;
  r.counts={5,12,3,20};r.activeTotal=20;r.activeCount=brooklyn::display::kActiveIndicationPage;
  r.eventCount=brooklyn::display::kIndicationHistoryPage;r.gap=r.more=true;
  for(unsigned i=0;i<r.activeCount;++i)r.active[i]={1000,49000,static_cast<uint16_t>(i)};
  for(unsigned i=0;i<r.eventCount;++i)r.events[i]={i+1,1000,21000,100000,static_cast<uint16_t>(i),brooklyn::display::IndicationEnd::StopWindowExpired,brooklyn::display::Indication::Uncertain};
  char json[6144];CHECK(brooklyn::web::renderIndicationJson(json,sizeof(json),r)>0);
  CHECK(strstr(json,"\"schemaVersion\":1")&&strstr(json,"\"historyGap\":true"));
  CHECK(strstr(json,"\"stoppedPolicy\":\"fresh-evidence\""));
  CHECK(strstr(json,"\"durationMs\":20000")&&strstr(json,"Stopped-color time limit expired"));
  CHECK(strstr(json,"\"stopped\":12")&&strstr(json,"\"activeTotal\":20"));
  CHECK(brooklyn::web::renderIndicationJson(json,40,r)==0);
  // Every mapped station must fit even with full active/history pages and max-width integers.
  for(uint16_t led=0;led<306;++led) {
    if(!brooklyn::semantic::semanticForLed(led))continue;
    for(auto &a:r.active)a={UINT32_MAX,UINT32_MAX,led};
    for(auto &e:r.events)e={UINT32_MAX,UINT32_MAX,UINT32_MAX-1,UINT32_MAX,led,brooklyn::display::IndicationEnd::OutputFailure,brooklyn::display::Indication::Uncertain};
    CHECK(brooklyn::web::renderIndicationJson(json,sizeof(json),r)>0);
  }
}

void testTrainMonitor() {
  auto status=sampleStatus();status.blueReport.timestamp=1000;status.unixTime=1015;
  status.visibleMapLeds=12;status.resolvedTrains=80;
  status.blueReport.oldestFeedTimestamp=995;status.blueReport.newestFeedTimestamp=1000;
  status.blueReport.monitoring[static_cast<unsigned>(brooklyn::semantic::Route::RouteF)]={8,3,2,4};
  char json[6144],html[24576];
  CHECK(brooklyn::web::renderTrainMonitorJson(json,sizeof(json),status)>0);
  CHECK(strstr(json,"\"visibleMapLeds\":12") && strstr(json,"\"mappedTrains\":80"));
  CHECK(strstr(json,"\"stoppedPolicy\":\"fresh-evidence\"") && strstr(json,"\"stoppedTimeLimitEnabled\":false"));
  CHECK(strstr(json,"\"line\":\"F\"") && strstr(json,"\"health\":\""));
  CHECK(strstr(json,"\"latestPollMappedTrains\":8"));
  CHECK(strstr(json,"\"sourceAgeSeconds\":20") && strstr(json,"\"reportAgeSeconds\":15"));
  CHECK(strstr(json,"\"mapped\":8,\"moving\":3,\"estimated\":2,\"uncertain\":4"));
  unsigned lines=0;for(const char *p=json;(p=strstr(p,"\"line\":"));p+=7)++lines;CHECK(lines==18);
  CHECK(brooklyn::web::renderTrainMonitorPage(html,sizeof(html),status)>0);
  CHECK(strstr(html,"Train monitor") && strstr(html,"Download report"));
  CHECK(brooklyn::web::renderTrainMonitorPage(html,40,status)==0);
  CHECK(brooklyn::web::renderTrainMonitorJson(json,40,status)==0);
  status.hasCycle=true;status.feedsSucceeded=7;status.feedsTotal=8;
  CHECK(brooklyn::web::renderTrainMonitorJson(json,sizeof(json),status)>0);
  CHECK(strstr(json,"Some feeds are missing"));
  status.fetchingFeeds=true;
  CHECK(brooklyn::web::renderTrainMonitorJson(json,sizeof(json),status)>0);
  CHECK(strstr(json,"\"fetchingFeeds\":true") && strstr(json,"Fetching train data"));
  CHECK(brooklyn::web::renderStatusJson(json,sizeof(json),status,5,brooklyn::semantic::kAllRoutesVisibleMask)>0);
  CHECK(strstr(json,"\"fetchingFeeds\":true"));
  CHECK(brooklyn::web::renderHomePage(html,sizeof(html),status,5,brooklyn::semantic::kAllRoutesVisibleMask)>0);
  CHECK(strstr(html,"Fetching train data"));
  status.fetchingFeeds=false;

  status.processedFeedMask=254;
  CHECK(brooklyn::web::renderTrainMonitorJson(json,sizeof(json),status)>0);
  CHECK(strstr(json,"\"feedState\":\"Missing\""));
  status.processedFeedMask=2;
  CHECK(brooklyn::web::renderTrainMonitorJson(json,sizeof(json),status)>0);
  CHECK(strstr(json,"\"feedState\":\"Partial\""));
  status.hasLastFeedFailure=true;strcpy(status.lastFeedFailureCategory,"<script>");
  CHECK(brooklyn::web::renderTrainMonitorPage(html,sizeof(html),status)>0);
  CHECK(strstr(html,"&lt;script&gt;") && !strstr(html,"<dd id=\"failure\"><script>"));
}

void testJsonStatusIsEscapedAndSecretFree() {
  char json[6144];
  brooklyn::web::StatusSnapshot status = sampleStatus();
  strncpy(status.firmwareVersion, "0.2.8", sizeof(status.firmwareVersion) - 1);
  status.otaBootConfirmed = true;
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
  status.physicalLedOutputEnabled = true;
  status.physicalFramesWritten = 42;
  status.physicalMapChanges = 7;
  status.physicalBlinkingLeds = 3;
  CHECK(brooklyn::web::renderStatusJson(json, sizeof(json), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
  CHECK(strstr(json, "\"physicalLedOutputEnabled\":true") != nullptr);
  CHECK(strstr(json, "\"physicalFramesWritten\":42") != nullptr);
  CHECK(strstr(json, "\"physicalMapChanges\":7") != nullptr);
  CHECK(strstr(json, "\"physicalBlinkingLeds\":3") != nullptr);
  status.physicalLedOutputEnabled = false;
  CHECK(brooklyn::web::renderStatusJson(json, sizeof(json), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
  CHECK(strstr(json, "\"approachingTrains\":0") != nullptr);
  CHECK(strstr(json, "\"firmware\":\"Phase 4W\"") != nullptr);
  CHECK(strstr(json, "\"firmwareVersion\":\"0.2.8\"") != nullptr);
  CHECK(strstr(json, "\"otaBootConfirmed\":true") != nullptr);
  CHECK(strstr(json, "\"adminConfigured\":false") != nullptr);
  CHECK(strstr(json, "\"adminSessionActive\":false") != nullptr);
  CHECK(strstr(json, "\"dhcpDns1\":\"0.0.0.0\"") != nullptr);
  CHECK(strstr(json, "\"dhcpDns2\":\"192.168.137.1\"") != nullptr);
  CHECK(strstr(json, "\"manualDnsEnabled\":true") != nullptr);
  CHECK(strstr(json, "\"manualDnsApplied\":true") != nullptr);
  CHECK(strstr(json, "\"manualDnsServer\":\"1.1.1.1\"") != nullptr);
  CHECK(strstr(json, "\"effectiveDnsSource\":\"Manual\"") != nullptr);
  CHECK(strstr(json, "\"effectiveDnsServer\":\"1.1.1.1\"") != nullptr);
  CHECK(strstr(json, "\"resetReason\":\"boot\\rready\"") != nullptr);
  CHECK(strstr(json, "\"brightness\":64") != nullptr);
  CHECK(strstr(json, "\"routeVisibilityMask\":262143,\"routeCount\":18") !=
        nullptr);
  CHECK(strstr(json, "\"feedsSucceeded\":8,\"feedsTotal\":8") != nullptr);
  CHECK(strstr(json, "\"resolvedTrains\":57,\"occupiedLeds\":53,"
                     "\"collisionCount\":4") != nullptr);
  CHECK(strstr(json, "\"displayState\":\"Fresh\"") != nullptr);
  CHECK(strstr(json, "\"secureConnectFailures\":2,\"httpFailures\":3,"
                     "\"transportTimeouts\":4,\"bytesDownloaded\":123456") !=
        nullptr);
  CHECK(strstr(json, "\"lastFeedFailure\":{\"feed\":\"NQRW\","
                     "\"category\":\"timed-out\",\"bytes\":143,"
                     "\"elapsedMs\":321}") != nullptr);
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
  status.adminConfigured = true;
  status.adminSessionActive = true;
  CHECK(brooklyn::web::renderStatusJson(
            json, sizeof(json), status, 64,
            brooklyn::semantic::kAllRoutesVisibleMask) > 0);
  CHECK(strstr(json, "\"adminConfigured\":true") != nullptr);
  CHECK(strstr(json, "\"adminSessionActive\":true") != nullptr);
  CHECK(strstr(json, "0123456789abcdef") == nullptr);
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
  char json[6144];
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
  {
    MemoryStore memory;
    brooklyn::config::DeviceConfiguration c(configStore(memory));
    c.load(); CHECK(c.pollSeconds() == 15);
    CHECK(c.setPollSeconds(30));
    brooklyn::config::DeviceConfiguration restored(configStore(memory));
    restored.load(); CHECK(restored.pollSeconds() == 30);
    CHECK(!c.setPollSeconds(14) && !c.setPollSeconds(121));
    memory.failSave = true; CHECK(!c.setPollSeconds(15) && c.pollSeconds() == 30);
    uint32_t seconds = 0;
    CHECK(brooklyn::config::parsePollSeconds("15", seconds) && seconds == 15);
    CHECK(brooklyn::config::parsePollSeconds("120", seconds) && seconds == 120);
    CHECK(!brooklyn::config::parsePollSeconds("15x", seconds));
    CHECK(!brooklyn::config::parsePollSeconds("-15", seconds));
    CHECK(!brooklyn::config::parsePollSeconds("9999999999999999999", seconds));
    CHECK(!brooklyn::config::parsePollSeconds(nullptr, seconds));
  }
  testTrainColors();
  testBlueHoldConfiguration();
  testBrightnessParsing();
  {
    uint8_t a = 0;
    CHECK(brooklyn::config::parseOledAddress("0x38", a) && a == 0x38);
    CHECK(brooklyn::config::parseOledAddress("0X3c", a) && a == 0x3c);
    const char *bad[] = {"", "0", "0x", "0x3", "0x380", "38", "0x07", "0x78", "0xff", "0xgg", " 0x38"};
    for (const char *v : bad) CHECK(!brooklyn::config::parseOledAddress(v, a));
    MemoryStore m; brooklyn::config::DeviceConfiguration c(configStore(m));
    c.load(); CHECK(c.oledAddress() == 0x38);
    CHECK(c.setOledAddress(0x3c));
    brooklyn::config::DeviceConfiguration restored(configStore(m)); restored.load(); CHECK(restored.oledAddress() == 0x3c);
    m.failSave = true; CHECK(!c.setOledAddress(0x3d) && c.oledAddress() == 0x3c);
    CHECK(!c.setOledAddress(0x07) && !c.setOledAddress(0x78));
    m.oledAddress = 0x138; restored.load(); CHECK(restored.oledAddress() == 0x38);
  }
  {
    auto status = sampleStatus();
    status.blueReport.timestamp = 1794584704;
    for (uint8_t i = 1; i <= brooklyn::semantic::kRouteCount; ++i) {
      auto &r = status.blueReport.routes[i]; r.trains=10; r.leds=9; r.old=3; r.different=2; r.future=1; r.missing=4; r.shared=1;
    }
    status.blueReport.sampleCount = brooklyn::pipeline::kBlueSamples;
    for (uint8_t i = 0; i < status.blueReport.sampleCount; ++i) {
      auto &s = status.blueReport.samples[i]; s.led=brooklyn::semantic::kMapLeds[i].globalOrder;
      s.age=100; s.flags=brooklyn::pipeline::NextDifferent; s.trains=2; strcpy(s.nextStop,"232N");
    }
    static char json[6144], page[brooklyn::web::kStatusPageCapacity];
    CHECK(brooklyn::web::renderStatusJson(json,sizeof(json),status,5,brooklyn::semantic::kAllRoutesVisibleMask)>0);
    CHECK(strstr(json,"\"blueSummary\":") != nullptr);
    CHECK(strstr(json,"NextDiff") != nullptr && strstr(json,"232N (different)") != nullptr);
    CHECK(brooklyn::web::renderHomePage(page,sizeof(page),status,5,brooklyn::semantic::kAllRoutesVisibleMask)>0);
    CHECK(strstr(page,"Train report details") != nullptr && strstr(page,"data-status=\"blueSummary\"") != nullptr);
  }
  {
    auto s = sampleStatus();
    s.oledStatus.state = brooklyn::oled::State::Ready;
    s.oledStatus.requestedAddress = 0x38; s.oledStatus.activeAddress = 0x3c;
    static char page[brooklyn::web::kStatusPageCapacity], json[6144];
    CHECK(brooklyn::web::renderHomePage(page,sizeof(page),s,5,brooklyn::semantic::kAllRoutesVisibleMask)>0);
    CHECK(strstr(page,"Frame writes OK; startup address 0x38; active address 0x3C")!=nullptr);
    CHECK(brooklyn::web::renderStatusJson(json,sizeof(json),s,5,brooklyn::semantic::kAllRoutesVisibleMask)>0);
    CHECK(strstr(json,"\"oledActiveAddress\":60")!=nullptr);
  }
  {brooklyn::web::StatusSnapshot busy;CHECK(!brooklyn::web::otaTransportBusy(busy));
   busy.networkBusy=true;CHECK(brooklyn::web::otaTransportBusy(busy));
   busy.networkBusy=false;busy.fetchingFeeds=true;CHECK(brooklyn::web::otaTransportBusy(busy));}
  {brooklyn::web::StatusSnapshot focus;focus.routeVisibilityMask=0;focus.routeFocusActive=true;focus.focusedRoute=brooklyn::semantic::Route::RouteF;
   char result[6144];CHECK(brooklyn::web::effectiveRouteMask(focus)==256);
   CHECK(brooklyn::web::renderStatusJson(result,sizeof(result),focus,5,0)>0);
   CHECK(strstr(result,"\"routeVisibilityMask\":0") && strstr(result,"\"routeFocusActive\":true") && strstr(result,"\"routeFocusRoute\":\"F\"") && strstr(result,"\"effectiveRouteMask\":256"));
   focus.routeFocusActive=false;CHECK(brooklyn::web::effectiveRouteMask(focus)==0);}
  testPersistentConfiguration();
  testDnsConfigurationAndValidation();
  testDnsSettingsFormParsing();
  testRouteVisibilityPersistenceAndMigration();
  testFullProductionPageCapacity();
  testPageEscapingAndRequiredContent();
  testInlineLoginOnStatusPage();
  testLoginPageCsrfRendering();
  testTrainMonitor();
  testIndicationApi();
  testRuntimeObservation();  testJsonStatusIsEscapedAndSecretFree();
  testRouteVisibilityApiMasks();
  testRouteSelectionParsing();
  testServerOwnershipTransitions();
  static char page[brooklyn::web::kStatusPageCapacity];
  char json[6144];
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
