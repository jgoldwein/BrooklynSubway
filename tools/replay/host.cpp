// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "gtfs_rt_state.h"
#include "network_config.h"
#include "operational_display.h"
#include "train_pipeline.h"

#include "led_map.h"
#include "semantic_api.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace display = brooklyn::display;
namespace gtfsrt = brooklyn::gtfsrt;
namespace led = brooklyn::led;
namespace network = brooklyn::network;
namespace pipeline = brooklyn::pipeline;
namespace semantic = brooklyn::semantic;

namespace {

constexpr size_t kMaxVehicles = 2048;
constexpr size_t kMaxDecodeTrips = 1024;
constexpr size_t kMaxFeedBytes = 512u * 1024u;
constexpr size_t kMaxFeedLineBytes = kMaxFeedBytes * 2u + 64u;
constexpr char kWifiNames[][16] = {
    "Disabled", "Unavailable", "Connecting", "Associated", "TimeSyncing"};

struct TrainMetadata {
  uint16_t globalIndex;
  char tripId[gtfsrt::kTripIdMax];
  char routeId[gtfsrt::kRouteIdMax];
  char vehicleId[gtfsrt::kVehicleIdMax];
  char trainId[gtfsrt::kTrainIdMax];
  char currentStop[gtfsrt::kStopIdMax];
  char nextStop[gtfsrt::kStopIdMax];
  char previousStop[gtfsrt::kPrevStopMax];
  char previousSource[24];
  char previousFailure[24];
  char category[32];
  char reason[32];
};

uint8_t gFeedBuffer[kMaxFeedBytes];
char gFeedLine[kMaxFeedLineBytes];
gtfsrt::RtTrain gDecoded[kMaxDecodeTrips];
TrainMetadata gCandidateMetadata[kMaxVehicles];
TrainMetadata gDisplayedMetadata[kMaxVehicles];
uint32_t gCandidateOutcomes[kMaxVehicles];
bool gCandidateResolved[kMaxVehicles];
bool gCandidateConflicted[kMaxVehicles];
size_t gCandidateCount;
size_t gDisplayedCount;
uint32_t gMetadataDropped;

void jsonString(const char *value) {
  putchar('"');
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value);
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

const char *previousSourceName(gtfsrt::PreviousSource source) {
  switch (source) {
    case gtfsrt::PreviousSource::Realtime: return "Realtime";
    case gtfsrt::PreviousSource::StaticTopology: return "StaticTopology";
    case gtfsrt::PreviousSource::StaticEquivalent: return "StaticEquivalent";
    case gtfsrt::PreviousSource::ExactTrip: return "ExactTrip";
    case gtfsrt::PreviousSource::RouteFallback: return "RouteFallback";
    case gtfsrt::PreviousSource::None: return "Unavailable";
  }
  return "Unavailable";
}

const char *previousFailureName(gtfsrt::PreviousFailure failure) {
  switch (failure) {
    case gtfsrt::PreviousFailure::None: return "None";
    case gtfsrt::PreviousFailure::NoPrevious: return "NoPrevious";
    case gtfsrt::PreviousFailure::Ambiguous: return "Ambiguous";
    case gtfsrt::PreviousFailure::UnknownDestination: return "UnknownDestination";
  }
  return "Unknown";
}

struct MetadataContext {
  const gtfsrt::RtTrain *merged;
  size_t mergedCount;
  gtfsrt::BuilderConfig config;
};

void collectVehicle(void *opaque, const gtfsrt::RtTrain &vehicle, uint32_t feedTimestamp) {
  if (semantic::routeFromGtfsRouteId(vehicle.routeId) == semantic::Route::None)
    return;
  MetadataContext &context = *static_cast<MetadataContext *>(opaque);
  gtfsrt::Observation observation{};
  gtfsrt::evaluateTrain(vehicle, feedTimestamp, context.config, observation);
  uint16_t globalIndex = UINT16_MAX;
  const bool resolved =
      (observation.category == gtfsrt::Category::ResolvedStation ||
       observation.category == gtfsrt::Category::ResolvedSegment) &&
      semantic::validatedGlobalIndex(observation.result, globalIndex);
  uint32_t reasonKey = 0;
  if (observation.category == gtfsrt::Category::Skipped) {
    if (observation.reason == gtfsrt::Reason::PreviousStopAmbiguous) reasonKey = 1;
    else if (observation.reason == gtfsrt::Reason::Stale) reasonKey = 2;
    else if (observation.reason == gtfsrt::Reason::NotStarted) reasonKey = 3;
  }
  const uint32_t outcome =
      (static_cast<uint32_t>(observation.category) << 24) |
      (reasonKey << 16) | (resolved ? globalIndex : UINT16_MAX);

  const gtfsrt::RtTrain *enriched = nullptr;
  for (size_t i = 0; i < context.mergedCount; ++i) {
    if (strcmp(context.merged[i].tripId, vehicle.tripId) == 0) {
      enriched = &context.merged[i];
      break;
    }
  }
  TrainMetadata candidate{};
  candidate.globalIndex = resolved ? globalIndex : UINT16_MAX;
  snprintf(candidate.tripId, sizeof candidate.tripId, "%s", vehicle.tripId);
  snprintf(candidate.routeId, sizeof candidate.routeId, "%s", vehicle.routeId);
  snprintf(candidate.vehicleId, sizeof candidate.vehicleId, "%s", vehicle.vehicleId);
  snprintf(candidate.trainId, sizeof candidate.trainId, "%s", vehicle.trainId);
  snprintf(candidate.currentStop, sizeof candidate.currentStop, "%s", vehicle.vehicleStopId);
  if (enriched != nullptr) {
    if (enriched->vehicleId[0] != '\0')
      snprintf(candidate.vehicleId, sizeof candidate.vehicleId, "%s", enriched->vehicleId);
    if (enriched->trainId[0] != '\0')
      snprintf(candidate.trainId, sizeof candidate.trainId, "%s", enriched->trainId);
    if (enriched->hasTripUpdate && enriched->nextStopId[0] != '\0')
      snprintf(candidate.nextStop, sizeof candidate.nextStop, "%s", enriched->nextStopId);
  }
  snprintf(candidate.previousStop, sizeof candidate.previousStop, "%s",
           observation.previous.stopId);
  snprintf(candidate.previousSource, sizeof candidate.previousSource, "%s",
           previousSourceName(observation.previous.source));
  snprintf(candidate.previousFailure, sizeof candidate.previousFailure, "%s",
           previousFailureName(observation.previous.failure));
  snprintf(candidate.category, sizeof candidate.category, "%s",
           gtfsrt::categoryName(observation.category));
  snprintf(candidate.reason, sizeof candidate.reason, "%s",
           observation.reason == gtfsrt::Reason::None
               ? "None"
               : gtfsrt::reasonName(observation.reason));

  for (size_t i = 0; i < gCandidateCount; ++i) {
    if (strcmp(gCandidateMetadata[i].tripId, vehicle.tripId) != 0) continue;
    if (gCandidateOutcomes[i] != outcome) {
      gCandidateConflicted[i] = true;
    } else {
      TrainMetadata &existing = gCandidateMetadata[i];
      if (existing.vehicleId[0] == '\0' && candidate.vehicleId[0] != '\0')
        snprintf(existing.vehicleId, sizeof existing.vehicleId, "%s", candidate.vehicleId);
      if (existing.trainId[0] == '\0' && candidate.trainId[0] != '\0')
        snprintf(existing.trainId, sizeof existing.trainId, "%s", candidate.trainId);
      if (existing.nextStop[0] == '\0' && candidate.nextStop[0] != '\0')
        snprintf(existing.nextStop, sizeof existing.nextStop, "%s", candidate.nextStop);
    }
    return;
  }
  if (gCandidateCount >= kMaxVehicles) {
    if (gMetadataDropped != UINT32_MAX) ++gMetadataDropped;
    return;
  }
  gCandidateMetadata[gCandidateCount] = candidate;
  gCandidateOutcomes[gCandidateCount] = outcome;
  gCandidateResolved[gCandidateCount] = resolved;
  gCandidateConflicted[gCandidateCount] = false;
  ++gCandidateCount;
}

void storeDisplayedMetadata(const semantic::TrainFrame &frame) {
  uint8_t accepted[semantic::kFrameLedCount]{};
  gDisplayedCount = 0;
  for (size_t i = 0; i < gCandidateCount; ++i) {
    if (!gCandidateResolved[i] || gCandidateConflicted[i]) continue;
    const uint16_t globalIndex = gCandidateMetadata[i].globalIndex;
    if (globalIndex >= semantic::kFrameLedCount ||
        accepted[globalIndex] >= frame.occupancy[globalIndex])
      continue;
    ++accepted[globalIndex];
    gDisplayedMetadata[gDisplayedCount++] = gCandidateMetadata[i];
  }
}

bool parseHeader(const char *line, uint32_t &brightness, uint32_t &routeMask,
                 uint32_t &healthy, uint32_t &wifi, uint32_t &provisioning) {
  return sscanf(line, "REPLAY 1 %u %u %u %u %u", &brightness, &routeMask,
                &healthy, &wifi, &provisioning) == 5 &&
         brightness <= 255 && routeMask <= semantic::kAllRoutesVisibleMask &&
         healthy <= 1 && wifi <= 4 && provisioning <= 1;
}

const char *feedStatusName(uint8_t status) {
  switch (status) {
    case 0: return "SUCCESS";
    case 1: return "HTTP_FAILURE";
    case 2: return "DNS_CONNECT_FAILURE";
    case 3: return "TIMEOUT";
    case 4: return "OMITTED";
    case 5: return "UNAVAILABLE";
  }
  return "UNAVAILABLE";
}

template <size_t N>
bool readLine(char (&line)[N]) {
  return fgets(line, sizeof line, stdin) != nullptr;
}

int hexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

bool appendTrainRecords(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const TrainMetadata &train = gDisplayedMetadata[i];
    if (i != 0) putchar(',');
    printf("{\"globalIndex\":%u,\"route\":", train.globalIndex);
    jsonString(train.routeId);
    printf(",\"tripId\":");
    jsonString(train.tripId);
    printf(",\"vehicleId\":");
    jsonString(train.vehicleId[0] == '\0' ? "Unavailable" : train.vehicleId);
    printf(",\"trainId\":");
    jsonString(train.trainId[0] == '\0' ? "Unavailable" : train.trainId);
    printf(",\"currentStop\":");
    jsonString(train.currentStop[0] == '\0' ? "Unavailable" : train.currentStop);
    printf(",\"nextStop\":");
    jsonString(train.nextStop[0] == '\0' ? "Unavailable" : train.nextStop);
    printf(",\"previousStop\":");
    jsonString(train.previousStop[0] == '\0' ? "Unavailable" : train.previousStop);
    printf(",\"previousStopSource\":");
    jsonString(train.previousSource);
    putchar('}');
  }
  return true;
}

void appendObservationRecords() {
  bool first = true;
  for (size_t i = 0; i < gCandidateCount; ++i) {
    const TrainMetadata &observation = gCandidateMetadata[i];
    if (gCandidateResolved[i] ||
        (strcmp(observation.category, "UnknownStop") != 0 &&
         strcmp(observation.category, "OperationalStop") != 0 &&
         strcmp(observation.category, "Invalid") != 0 &&
         strcmp(observation.reason, "PreviousStopAmbiguous") != 0))
      continue;
    if (!first) putchar(',');
    first = false;
    printf("{\"route\":");
    jsonString(observation.routeId);
    printf(",\"tripId\":");
    jsonString(observation.tripId);
    printf(",\"currentStop\":");
    jsonString(observation.currentStop[0] == '\0' ? "Unavailable"
                                                  : observation.currentStop);
    printf(",\"nextStop\":");
    jsonString(observation.nextStop[0] == '\0' ? "Unavailable"
                                               : observation.nextStop);
    printf(",\"category\":");
    jsonString(observation.category);
    printf(",\"reason\":");
    jsonString(observation.reason);
    printf(",\"previousStop\":");
    jsonString(observation.previousStop[0] == '\0' ? "Unavailable"
                                                   : observation.previousStop);
    printf(",\"previousStopSource\":");
    jsonString(observation.previousSource);
    printf(",\"previousStopFailure\":");
    jsonString(observation.previousFailure);
    putchar('}');
  }
}

bool emitCycle(uint32_t cycleIndex, uint32_t replayUnix, uint32_t elapsedMs,
               const uint8_t (&statuses)[8], pipeline::TrainPipeline &trainPipeline,
               display::Controller &controller, uint32_t brightness,
               uint32_t routeMask, bool healthy, display::WifiIndicator wifi,
               bool provisioning) {
  const pipeline::Counters &counts = trainPipeline.counters();
  uint8_t available = 0;
  for (uint8_t status : statuses) {
    if (status == 0) ++available;
  }
  const display::CycleQuality quality =
      available == network::kFeedCount
          ? display::CycleQuality::Complete
          : available == 0 ? display::CycleQuality::NoFeeds
                           : display::CycleQuality::Partial;

  const uint32_t previousRevision = controller.frameRevision();
  controller.update(elapsedMs);
  if (controller.frameRevision() != previousRevision && !controller.hasFrame())
    gDisplayedCount = 0;
  controller.completeCycle(trainPipeline.frame(), quality, elapsedMs);
  if (controller.frameRevision() != previousRevision) {
    if (controller.hasFrame()) {
      storeDisplayedMetadata(controller.frame());
    } else {
      gDisplayedCount = 0;
    }
  }

  semantic::RgbFrame rendered{};
  semantic::RenderConfig renderConfig{};
  renderConfig.brightness = static_cast<uint8_t>(brightness);
  renderConfig.routeVisibilityMask = routeMask;
  const semantic::RenderResult renderResult =
      semantic::renderTrainFrame(controller.frame(), rendered, renderConfig);
  display::StatusIndicators indicators{};
  indicators.systemHealthy = healthy;
  indicators.wifi = wifi;
  indicators.provisioning = provisioning;
  indicators.feedAttempted = controller.feedAttempted();
  indicators.displayState = controller.state();
  indicators.brightness = static_cast<uint8_t>(brightness);
  if (!display::applyStatusOverlay(rendered, indicators)) return false;

  printf("{\"cycleIndex\":%u,\"replayUnix\":%u,\"frameTimestamp\":%u,"
         "\"displayState\":",
         cycleIndex, replayUnix, elapsedMs);
  jsonString(display::stateName(controller.state()));
  printf(",\"brightness\":%u,\"routeVisibilityMask\":%u,"
         "\"systemHealthy\":%s,\"wifiState\":",
         brightness, routeMask, healthy ? "true" : "false");
  jsonString(kWifiNames[static_cast<uint8_t>(wifi)]);
  printf(",\"provisioning\":%s,\"feedAttempted\":%s,\"feeds\":{"
         "\"available\":%u,\"expected\":%u,\"statuses\":[",
         provisioning ? "true" : "false",
         controller.feedAttempted() ? "true" : "false", available,
         static_cast<unsigned>(network::kFeedCount));
  for (size_t i = 0; i < network::kFeedCount; ++i) {
    if (i != 0) putchar(',');
    printf("{\"feedId\":");
    jsonString(network::kFeeds[i].name);
    printf(",\"status\":");
    jsonString(feedStatusName(statuses[i]));
    putchar('}');
  }
  printf("]},\"pipeline\":{\"feedsProcessed\":%u,\"feedDecodeFailures\":%u,"
         "\"entitiesSeen\":%u,\"vehiclesSeen\":%u,\"supportedVehicles\":%u,"
         "\"malformedEntities\":%u,\"tripUpdatesIgnored\":%u,"
         "\"resolvedStation\":%u,\"resolvedSegment\":%u,\"unrepresented\":%u,"
         "\"insufficientPosition\":%u,\"unsupportedRoute\":%u,\"unknownStop\":%u,"
         "\"invalid\":%u,\"previousStopAmbiguous\":%u,\"operationalStop\":%u,"
         "\"stale\":%u,\"notStarted\":%u,\"skipped\":%u,"
         "\"duplicateIdentical\":%u,\"duplicateConflict\":%u,"
         "\"conflictedTrips\":%u,\"tripCapacityOverflow\":%u,"
         "\"resolvedTrains\":%u,\"occupiedLeds\":%u,\"collisionLeds\":%u,"
         "\"maxOccupancy\":%u,\"rendererRejected\":%u,"
         "\"metadataDropped\":%u,\"renderedLeds\":%u,"
         "\"renderedStationLeds\":%u,\"renderedSegmentLeds\":%u},"
         "\"occupancy\":[",
         counts.feedsProcessed, counts.feedDecodeFailures, counts.entitiesSeen,
         counts.vehiclesSeen, counts.supportedVehicles, counts.malformedEntities,
         counts.tripUpdatesIgnored, counts.resolvedStation, counts.resolvedSegment,
         counts.unrepresented, counts.insufficientPosition, counts.unsupportedRoute,
         counts.unknownStop, counts.invalid, counts.previousStopAmbiguous,
         counts.operationalStop, counts.stale, counts.notStarted, counts.skipped,
         counts.duplicateIdentical, counts.duplicateConflict, counts.conflictedTrips,
         counts.tripCapacityOverflow, controller.frame().resolvedTrains,
         controller.frame().occupiedLeds, controller.frame().collisionLeds,
         controller.frame().maxOccupancy, renderResult.rejectedLeds,
         gMetadataDropped, renderResult.renderedLeds, renderResult.stationLeds,
         renderResult.segmentLeds);

  const semantic::TrainFrame &displayed = controller.frame();
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    if (i != 0) putchar(',');
    printf("%u", displayed.occupancy[i]);
  }
  printf("],\"routeVisible\":[");
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    const semantic::MapLed *map = semantic::semanticForLed(i);
    if (i != 0) putchar(',');
    printf("%s", map != nullptr &&
                         semantic::routeVisible(routeMask, map->route)
                     ? "true"
                     : "false");
  }
  printf("],\"filteredByRoute\":[");
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    const semantic::MapLed *map = semantic::semanticForLed(i);
    if (i != 0) putchar(',');
    printf("%s", displayed.occupancy[i] != 0 && map != nullptr &&
                         !semantic::routeVisible(routeMask, map->route)
                     ? "true"
                     : "false");
  }
  printf("],\"pixels\":[");
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    if (i != 0) putchar(',');
    const semantic::Rgb &pixel = rendered.pixels[i];
    printf("{\"r\":%u,\"g\":%u,\"b\":%u}", pixel.red, pixel.green,
           pixel.blue);
  }
  printf("],\"trains\":[");
  appendTrainRecords(gDisplayedCount);
  printf("],\"observations\":[");
  appendObservationRecords();
  printf("],\"trainFrame\":{\"occupancy\":[");
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    if (i != 0) putchar(',');
    printf("%u", displayed.occupancy[i]);
  }
  printf("],\"trains\":[");
  appendTrainRecords(gDisplayedCount);
  printf("],\"resolvedTrains\":%u,\"occupiedLeds\":%u,\"collisionLeds\":%u},"
         "\"collisions\":[",
         displayed.resolvedTrains, displayed.occupiedLeds,
         displayed.collisionLeds);

  bool firstCollision = true;
  for (uint16_t i = 0; i < semantic::kFrameLedCount; ++i) {
    if (displayed.occupancy[i] < 2) continue;
    led::map::PhysicalLed physical{};
    if (!led::map::physicalLed(led::GlobalPhysicalIndex{i}, physical)) return false;
    const semantic::MapLed *map = semantic::semanticForLed(i);
    if (map == nullptr) continue;
    if (!firstCollision) putchar(',');
    firstCollision = false;
    printf("{\"globalIndex\":%u,\"designator\":\"LED%u\",\"route\":",
           i, physical.designator.value);
    jsonString(semantic::routeName(map->route));
    if (map->role == semantic::Role::Station) {
      const semantic::Station *station = semantic::findStation(map->stationId);
      printf(",\"semanticType\":\"STATION\",\"stationId\":%u,\"stationName\":",
             map->stationId);
      jsonString(station == nullptr ? "Unavailable" : station->name);
    } else {
      const semantic::Station *a = semantic::findStation(map->endpointA);
      const semantic::Station *b = semantic::findStation(map->endpointB);
      printf(",\"semanticType\":\"SEGMENT\",\"endpointAId\":%u,"
             "\"endpointAName\":",
             map->endpointA);
      jsonString(a == nullptr ? "Unavailable" : a->name);
      printf(",\"endpointBId\":%u,\"endpointBName\":", map->endpointB);
      jsonString(b == nullptr ? "Unavailable" : b->name);
    }
    printf(",\"trainCount\":%u,\"rgb\":{\"r\":%u,\"g\":%u,\"b\":%u},"
           "\"trains\":[",
           displayed.occupancy[i], rendered.pixels[i].red,
           rendered.pixels[i].green, rendered.pixels[i].blue);
    bool firstTrain = true;
    for (size_t j = 0; j < gDisplayedCount; ++j) {
      if (gDisplayedMetadata[j].globalIndex != i) continue;
      if (!firstTrain) putchar(',');
      firstTrain = false;
      printf("{\"tripId\":");
      jsonString(gDisplayedMetadata[j].tripId);
      printf(",\"vehicleId\":");
      jsonString(gDisplayedMetadata[j].vehicleId[0] == '\0'
                     ? "Unavailable"
                     : gDisplayedMetadata[j].vehicleId);
      putchar('}');
    }
    printf("]}");
  }
  printf("]}\n");
  fflush(stdout);
  return true;
}

bool runReplay() {
  char line[128];
  uint32_t brightness = 0, routeMask = 0, healthy = 0, wifi = 0, provisioning = 0;
  if (!readLine(line) ||
      !parseHeader(line, brightness, routeMask, healthy, wifi, provisioning)) {
    fprintf(stderr, "replay bridge: invalid replay header\n");
    return false;
  }

  display::Controller controller;
  for (;;) {
    if (!readLine(line)) {
      fprintf(stderr, "replay bridge: unexpected end of input\n");
      return false;
    }
    if (strcmp(line, "END\n") == 0 || strcmp(line, "END\r\n") == 0) return true;
    uint32_t cycleIndex = 0, replayUnix = 0, elapsedMs = 0, feedCount = 0;
    if (sscanf(line, "C %u %u %u %u", &cycleIndex, &replayUnix, &elapsedMs,
               &feedCount) != 4 ||
        feedCount != network::kFeedCount)
    {
      fprintf(stderr, "replay bridge: invalid cycle header\n");
      return false;
    }

    semantic::RenderConfig config{};
    config.brightness = static_cast<uint8_t>(brightness);
    config.routeVisibilityMask = routeMask;
    pipeline::TrainPipeline trainPipeline;
    trainPipeline.beginCycle(replayUnix, config);
    gCandidateCount = 0;
    gMetadataDropped = 0;
    uint8_t statuses[8]{};

    for (size_t i = 0; i < network::kFeedCount; ++i) {
      uint32_t status = 0, length = 0;
      int payloadOffset = 0;
      if (!readLine(gFeedLine) ||
          sscanf(gFeedLine, "F %u %u%n", &status, &length, &payloadOffset) != 2 ||
          status >= 6 || length > kMaxFeedBytes) {
        fprintf(stderr, "replay bridge: invalid feed record\n");
        return false;
      }
      statuses[i] = static_cast<uint8_t>(status);
      const size_t lineLength = strlen(gFeedLine);
      const size_t encodedLength = static_cast<size_t>(length) * 2u;
      const size_t dataOffset = static_cast<size_t>(payloadOffset) + 1u;
      if (payloadOffset <= 0 || gFeedLine[payloadOffset] != ' ' ||
          lineLength != dataOffset + encodedLength + 1u ||
          gFeedLine[lineLength - 1u] != '\n' ||
          (status != 0 && length != 0)) {
        fprintf(stderr, "replay bridge: invalid feed payload length\n");
        return false;
      }
      for (size_t byte = 0; byte < length; ++byte) {
        const int high = hexValue(gFeedLine[dataOffset + byte * 2u]);
        const int low = hexValue(gFeedLine[dataOffset + byte * 2u + 1u]);
        if (high < 0 || low < 0) {
          fprintf(stderr, "replay bridge: invalid hexadecimal payload\n");
          return false;
        }
        gFeedBuffer[byte] = static_cast<uint8_t>((high << 4) | low);
      }
      if (length == 0) continue;

      trainPipeline.processFeed(gFeedBuffer, length);
      size_t decodedCount = 0;
      gtfsrt::FeedStats decodedStats{};
      gtfsrt::decodeFeed(gFeedBuffer, length, gDecoded, kMaxDecodeTrips,
                         decodedCount, decodedStats);
      gtfsrt::BuilderConfig builder{};
      builder.now = replayUnix;
      builder.maxVehicleAgeSec = gtfsrt::kDefaultMaxVehicleAgeSec;
      builder.previousStop = gtfsrt::topologyPreviousStop;
      MetadataContext context{gDecoded, decodedCount, builder};
      gtfsrt::FeedStats visitorStats{};
      gtfsrt::visitVehicles(gFeedBuffer, length, collectVehicle, &context,
                            visitorStats);
    }
    trainPipeline.finishCycle();
    if (!emitCycle(cycleIndex, replayUnix, elapsedMs, statuses, trainPipeline,
                   controller, brightness, routeMask, healthy != 0,
                   static_cast<display::WifiIndicator>(wifi),
                   provisioning != 0)) {
      fprintf(stderr, "replay bridge: unable to emit cycle\n");
      return false;
    }
  }
}

bool emitFeeds() {
  printf("{\"formatVersion\":1,\"feeds\":[");
  for (size_t i = 0; i < network::kFeedCount; ++i) {
    if (i != 0) putchar(',');
    printf("{\"id\":");
    jsonString(network::kFeeds[i].name);
    printf(",\"url\":");
    jsonString(network::kFeeds[i].url);
    putchar('}');
  }
  printf("]}\n");
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "feeds") == 0) return emitFeeds() ? 0 : 2;
  if (argc == 2 && strcmp(argv[1], "replay") == 0) return runReplay() ? 0 : 2;
  fprintf(stderr, "usage: brooklyn_replay_host feeds | replay\n");
  return 2;
}
