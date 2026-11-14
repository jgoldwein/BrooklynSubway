// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
// Read-only host audit using the same decoder and state builder as firmware.
#include "gtfs_rt_state.h"
#include "semantic_api.h"
#include "train_frame.h"
#include "train_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace brooklyn;
static gtfsrt::RtTrain trains[2048];
static pipeline::TrainPipeline pipelineAudit;
static uint8_t bytes[1500000];
void js(const char *s) { putchar('"'); for(;s&&*s;++s) { unsigned char c=*s; if(c=='"'||c=='\\')printf("\\%c",c);else if(c<32)printf("\\u%04x",c);else putchar(c); } putchar('"'); }
void mapped(const gtfsrt::Observation &o) {
 uint16_t index=0;
 if(semantic::validatedGlobalIndex(o.result,index)) {
  const auto *m=semantic::semanticForLed(index);
  printf("{\"index\":%u,\"designator\":%u,\"approaching\":%s}",index,m?m->designator:0,o.approachingStation?"true":"false");
 } else printf("null");
}
int main(int argc,char **argv) {
 if(argc<3)return 2;
 const uint32_t now=static_cast<uint32_t>(strtoul(argv[1],nullptr,10));
 pipelineAudit.beginCycle(now);
 printf("{\"now\":%u,\"feeds\":[",now);
 for(int file=2;file<argc;++file) {
  FILE *f=fopen(argv[file],"rb");if(!f)return 3;size_t len=fread(bytes,1,sizeof(bytes),f);fclose(f);
  pipelineAudit.processFeed(bytes,len);
  size_t count=0;gtfsrt::FeedStats stats{};const auto status=gtfsrt::decodeFeed(bytes,len,trains,2048,count,stats);
  if(file>2)putchar(',');printf("{\"file\":");js(argv[file]);printf(",\"timestamp\":%u,\"decodeStatus\":%u,\"routeConflicts\":%u,\"trips\":[",stats.headerTimestamp,unsigned(status),stats.routeConflicts);
  bool first=true;
  for(size_t i=0;i<count;++i) {
   auto &t=trains[i];if(!t.hasVehicle)continue;
   if(!first)putchar(',');first=false;
   gtfsrt::BuilderConfig cfg{};cfg.now=now;cfg.maxVehicleAgeSec=gtfsrt::kDefaultMaxVehicleAgeSec;cfg.previousStop=gtfsrt::topologyPreviousStop;
   gtfsrt::Observation before{},after{};gtfsrt::evaluateTrain(t,stats.headerTimestamp,cfg,before);
   printf("{\"trip\":");js(t.tripId);printf(",\"route\":");js(t.routeId);printf(",\"canonicalRoute\":");js(semantic::routeName(semantic::routeFromGtfsRouteId(t.routeId)));
   printf(",\"vehicleStop\":");js(t.vehicleStopId);printf(",\"nextStop\":");js(t.nextStopId);
   printf(",\"vehicleStatus\":%u,\"vehicleTimestamp\":%u,\"predictionTimestamp\":%u,\"arrival\":%u,\"beforeCategory\":",unsigned(t.status),t.vehicleTimestamp,t.tripUpdateTimestamp?t.tripUpdateTimestamp:stats.headerTimestamp,t.nextStopArrival);
   js(gtfsrt::categoryName(before.category));printf(",\"before\":");mapped(before);
   const bool inferred=t.hasTripUpdate&&gtfsrt::applyVerifiedTripDeparture(t,t.nextStopId,t.nextStopArrival,t.tripUpdateTimestamp?t.tripUpdateTimestamp:stats.headerTimestamp,stats.headerTimestamp,now);
   gtfsrt::evaluateTrain(t,stats.headerTimestamp,cfg,after);
   printf(",\"inferred\":%s,\"afterStop\":",inferred?"true":"false");js(t.vehicleStopId);
   printf(",\"afterCategory\":");js(gtfsrt::categoryName(after.category));printf(",\"afterReason\":");js(gtfsrt::reasonName(after.reason));printf(",\"after\":");mapped(after);putchar('}');
  }
  printf("]}");
 }
 pipelineAudit.finishCycle();
 const auto &counts=pipelineAudit.counters();
 printf("],\"pipeline\":{\"resolvedTrains\":%u,\"estimatedDepartures\":%u,\"approachingStation\":%u,\"decodeFailures\":%u,\"predictionOverflow\":%u,\"rendererRejected\":%u},\"map\":[",counts.resolvedTrains,counts.verifiedTripDepartures,counts.approachingStation,counts.feedDecodeFailures,counts.predictionCapacityOverflow,counts.rendererRejected);
 bool firstMap=true;
 for(uint16_t led=0;led<semantic::kFrameLedCount;++led) {
  if(!pipelineAudit.frame().occupancy[led])continue;
  const auto *mapped=semantic::semanticForLed(led);if(!mapped)continue;
  if(!firstMap)putchar(',');firstMap=false;
  printf("{\"route\":");js(semantic::routeName(mapped->route));
  printf(",\"designator\":%u,\"trains\":%u,\"approachingOnly\":%s}",mapped->designator,pipelineAudit.frame().occupancy[led],semantic::isApproachingOnly(pipelineAudit.frame(),led)?"true":"false");
 }
 printf("]}\n");return 0;
}
