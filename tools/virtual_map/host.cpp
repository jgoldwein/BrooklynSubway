// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "board_pins.h"
#include "led_map.h"
#include "operational_display.h"
#include "rgb_renderer.h"
#include "semantic_api.h"
#include "train_frame.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace semantic = brooklyn::semantic;
namespace display = brooklyn::display;
namespace led = brooklyn::led;
namespace pins = brooklyn::pins;

namespace {

const char *const kStatusNames[4] = {"SYSTEM", "NETWORK", "MTA", "MODE"};
const char *const kWifiNames[] = {
    "Disabled", "Unavailable", "Connecting", "Associated", "TimeSyncing"};

void printJsonString(const char *text) {
  putchar('"');
  for (const unsigned char *p =
           reinterpret_cast<const unsigned char *>(text);
       *p != 0; ++p) {
    if (*p == '"' || *p == '\\') {
      putchar('\\');
      putchar(*p);
    } else if (*p < 0x20) {
      printf("\\u%04x", *p);
    } else {
      putchar(*p);
    }
  }
  putchar('"');
}

const char *roleName(semantic::Role role) {
  return role == semantic::Role::Station ? "STATION" : "SEGMENT";
}

bool emitMetadata() {
  if (!semantic::selfCheck()) return false;
  printf("{\"formatVersion\":1,\"physicalPixelCount\":%u,"
         "\"mapPixelCount\":%u,\"statusPixelCount\":4,\"channels\":[",
         semantic::kPhysicalLedCount, semantic::kMapLedCount);
  for (uint8_t channel = 1; channel <= led::map::CHANNEL_COUNT; ++channel) {
    const uint8_t count =
        led::map::channelLedCount(static_cast<led::PhysicalChannel>(channel));
    if (channel != 1) putchar(',');
    printf("{\"channel\":%u,\"pixelCount\":%u,\"gpio\":%u}", channel, count,
           pins::LED_DATA[channel - 1]);
  }
  printf("],\"routes\":[");
  for (uint8_t value = 1; value <= semantic::kRouteCount; ++value) {
    if (value != 1) putchar(',');
    printf("{\"value\":%u,\"maskBit\":%u,\"name\":", value, value - 1);
    printJsonString(semantic::kRouteNames[value]);
    putchar('}');
  }
  printf("],\"pixels\":[");
  for (uint16_t global = 0; global < semantic::kPhysicalLedCount; ++global) {
    led::map::PhysicalLed physical{};
    if (!led::map::physicalLed(led::GlobalPhysicalIndex{global}, physical)) {
      return false;
    }
    const semantic::MapLed *map = semantic::semanticForLed(global);
    if (global != 0) putchar(',');
    printf("{\"globalIndex\":%u,\"channel\":%u,\"channelIndex\":%u,"
           "\"designator\":\"LED%u\"",
           global, static_cast<uint8_t>(physical.address.channel),
           physical.address.index, physical.designator.value);
    if (physical.role == led::LedRole::Status) {
      const uint8_t statusIndex = physical.address.index;
      if (statusIndex >= 4 || map != nullptr) return false;
      printf(",\"semanticType\":\"STATUS\",\"statusRole\":");
      printJsonString(kStatusNames[statusIndex]);
    } else {
      if (map == nullptr) return false;
      printf(",\"semanticType\":");
      printJsonString(roleName(map->role));
      printf(",\"route\":");
      printJsonString(semantic::routeName(map->route));
      if (map->role == semantic::Role::Station) {
        const semantic::Station *station =
            semantic::findStation(map->stationId);
        if (station == nullptr) return false;
        printf(",\"stationId\":%u,\"stationName\":", map->stationId);
        printJsonString(station->name);
      } else {
        const semantic::Station *a = semantic::findStation(map->endpointA);
        const semantic::Station *b = semantic::findStation(map->endpointB);
        if (a == nullptr || b == nullptr) return false;
        printf(",\"endpointAId\":%u,\"endpointAName\":", map->endpointA);
        printJsonString(a->name);
        printf(",\"endpointBId\":%u,\"endpointBName\":", map->endpointB);
        printJsonString(b->name);
      }
    }
    putchar('}');
  }
  printf("]}\n");
  return true;
}

bool buildInputFrame(semantic::TrainFrame &frame,
                     const uint32_t (&counts)[semantic::kFrameLedCount]) {
  semantic::clearFrame(frame);
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    if (counts[global] > semantic::kOccupancyMax) return false;
    for (uint32_t train = 0; train < counts[global]; ++train) {
      if (!semantic::addValidatedLed(frame, global)) return false;
    }
  }
  return true;
}

bool emitFrame(uint32_t timestamp) {
  uint32_t brightness = 0;
  uint32_t routeMask = 0;
  uint32_t requestedState = 0;
  uint32_t healthy = 0;
  uint32_t wifi = 0;
  uint32_t provisioning = 0;
  if (scanf("%u %u %u %u %u %u", &brightness, &routeMask, &requestedState,
            &healthy, &wifi, &provisioning) != 6 ||
      brightness > 255 || routeMask > semantic::kAllRoutesVisibleMask ||
      requestedState > 5 || healthy > 1 || wifi > 4 || provisioning > 1) {
    return false;
  }

  uint32_t requestedOccupancy[semantic::kFrameLedCount]{};
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    if (scanf("%u", &requestedOccupancy[global]) != 1) return false;
  }

  semantic::TrainFrame candidate{};
  if (!buildInputFrame(candidate, requestedOccupancy)) return false;

  display::Controller controller;
  switch (requestedState) {
    case 0:  // WaitingForData
      break;
    case 1:  // Fresh
      controller.completeCycle(candidate, display::CycleQuality::Complete, 1000);
      break;
    case 2:  // DegradedHold
      controller.completeCycle(candidate, display::CycleQuality::Complete, 1000);
      controller.completeCycle(candidate, display::CycleQuality::Partial, 2000);
      break;
    case 3:  // DegradedPartial
      controller.completeCycle(candidate, display::CycleQuality::Partial, 1000);
      break;
    case 4:  // StaleHold
      controller.completeCycle(candidate, display::CycleQuality::Complete, 1000);
      controller.update(display::kPartialGraceMs + 1001);
      break;
    case 5:  // NoData
      controller.completeCycle(candidate, display::CycleQuality::Complete, 1000);
      controller.update(display::kHardStaleLimitMs + 1001);
      break;
  }

  semantic::RenderConfig config{};
  config.brightness = static_cast<uint8_t>(brightness);
  config.routeVisibilityMask = routeMask;
  semantic::RgbFrame output{};
  const semantic::TrainFrame &displayed = controller.frame();
  const semantic::RenderResult rendered =
      semantic::renderTrainFrame(displayed, output, config);

  display::StatusIndicators indicators{};
  indicators.systemHealthy = healthy != 0;
  indicators.wifi = static_cast<display::WifiIndicator>(wifi);
  indicators.provisioning = provisioning != 0;
  indicators.feedAttempted = controller.feedAttempted();
  indicators.displayState = controller.state();
  indicators.brightness = static_cast<uint8_t>(brightness);
  if (!display::applyStatusOverlay(output, indicators)) return false;

  printf("{\"formatVersion\":1,\"frameTimestamp\":%u,\"brightness\":%u,"
         "\"routeVisibilityMask\":%u,\"displayState\":",
         timestamp, brightness, routeMask);
  printJsonString(display::stateName(controller.state()));
  printf(",\"systemHealthy\":%s,\"wifiState\":",
         healthy ? "true" : "false");
  printJsonString(kWifiNames[wifi]);
  printf(",\"provisioning\":%s,\"feedAttempted\":%s,"
         "\"renderedLeds\":%u,\"renderedStationLeds\":%u,"
         "\"renderedSegmentLeds\":%u,\"rejectedLeds\":%u,"
         "\"resolvedTrains\":%u,\"occupiedLeds\":%u,"
         "\"collisionLeds\":%u,\"occupancy\":[",
         provisioning ? "true" : "false",
         controller.feedAttempted() ? "true" : "false",
         rendered.renderedLeds, rendered.stationLeds, rendered.segmentLeds,
         rendered.rejectedLeds, displayed.resolvedTrains,
         displayed.occupiedLeds, displayed.collisionLeds);
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    if (global != 0) putchar(',');
    printf("%u", displayed.occupancy[global]);
  }
  printf("],\"routeVisible\":[");
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    const semantic::MapLed *map = semantic::semanticForLed(global);
    const bool visible =
        map != nullptr && semantic::routeVisible(routeMask, map->route);
    if (global != 0) putchar(',');
    printf("%s", visible ? "true" : "false");
  }
  printf("],\"filteredByRoute\":[");
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    const semantic::MapLed *map = semantic::semanticForLed(global);
    const bool filtered = displayed.occupancy[global] != 0 && map != nullptr &&
                          !semantic::routeVisible(routeMask, map->route);
    if (global != 0) putchar(',');
    printf("%s", filtered ? "true" : "false");
  }
  printf("],\"pixels\":[");
  for (uint16_t global = 0; global < semantic::kFrameLedCount; ++global) {
    if (global != 0) putchar(',');
    const semantic::Rgb &pixel = output.pixels[global];
    printf("{\"r\":%u,\"g\":%u,\"b\":%u}", pixel.red, pixel.green,
           pixel.blue);
  }
  printf("]}\n");
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "metadata") == 0) {
    return emitMetadata() ? 0 : 2;
  }
  if (argc == 3 && strcmp(argv[1], "render") == 0) {
    char *end = nullptr;
    const unsigned long timestamp = strtoul(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || timestamp > 0xFFFFFFFFul) return 2;
    return emitFrame(static_cast<uint32_t>(timestamp)) ? 0 : 2;
  }
  fprintf(stderr, "usage: virtual_map_host metadata | render <timestamp>\n");
  return 2;
}
