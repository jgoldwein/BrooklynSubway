// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "feed_transport.h"
#include <stdint.h>
namespace brooklyn::observation {
struct RetainedBoot {
  uint32_t magic,idHigh,idLow,boots,unexpected,uptimeMs,freeHeap,minHeap,checksum;
};
inline uint32_t bootChecksum(const RetainedBoot &r) {
  const uint32_t words[]={r.magic,r.idHigh,r.idLow,r.boots,r.unexpected,r.uptimeMs,r.freeHeap,r.minHeap};
  uint32_t hash=2166136261u;
  for(auto word:words)for(unsigned shift=0;shift<32;shift+=8)hash=(hash^((word>>shift)&255u))*16777619u;
  return hash;
}
inline bool retainedValid(const RetainedBoot &r) {return r.magic==0x424d4231 && r.checksum==bootChecksum(r);}
struct BootInfo {
  uint32_t idHigh=0,idLow=0,previousHigh=0,previousLow=0,boots=1,unexpected=0;
  uint32_t previousUptimeMs=0,previousFreeHeap=0,previousMinHeap=0;
  bool previousAvailable=false,previousCheckpointAvailable=false;
};
inline BootInfo startBoot(RetainedBoot &r,bool cold,bool unexpected,uint32_t high,uint32_t low) {
  BootInfo info{};const bool valid=!cold && retainedValid(r);
  if(valid) {
    info.previousAvailable=true;info.previousCheckpointAvailable=r.uptimeMs!=0;
    info.previousHigh=r.idHigh;info.previousLow=r.idLow;info.previousUptimeMs=r.uptimeMs;
    info.previousFreeHeap=r.freeHeap;info.previousMinHeap=r.minHeap;
    info.boots=r.boots==UINT32_MAX?UINT32_MAX:r.boots+1;info.unexpected=r.unexpected;
  }
  if(unexpected && info.unexpected!=UINT32_MAX)++info.unexpected;
  info.idHigh=high;info.idLow=low;
  r={0x424d4231,high,low,info.boots,info.unexpected,0,0,0,0};r.checksum=bootChecksum(r);
  return info;
}
inline void checkpoint(RetainedBoot &r,uint32_t uptime,uint32_t heap,uint32_t minimum) {
  r.uptimeMs=uptime;r.freeHeap=heap;r.minHeap=minimum;
  __atomic_thread_fence(__ATOMIC_RELEASE);r.checksum=bootChecksum(r);
}
struct ObservedFailure { transport::FeedFailure failure{};transport::AttemptDiagnostics details{}; };
constexpr uint8_t kFailureHistory=8;
struct FailureBatch {uint8_t count=0;ObservedFailure items[kFailureHistory]{};};
struct FailureEvent {uint32_t sequence=0;ObservedFailure observation{};};
struct RuntimeReport {
  BootInfo boot{};
  uint64_t deviceId=0;
  const char *resetReason="unknown";
  uint32_t uptimeMs=0,freeHeap=0,minimumHeap=0,largestBlock=0;
  uint32_t workerStackMinimumBytes=0,loopStackMinimumBytes=0,liveStackMinimumBytes=0;
  uint32_t newestSequence=0;
  uint8_t count=0,head=0;
  FailureEvent failures[kFailureHistory]{};
  void record(const ObservedFailure &failure) {
    if(newestSequence==UINT32_MAX)return;
    failures[head].sequence=++newestSequence;failures[head].observation=failure;
    head=(head+1)%kFailureHistory;if(count<kFailureHistory)++count;
  }
};
}
