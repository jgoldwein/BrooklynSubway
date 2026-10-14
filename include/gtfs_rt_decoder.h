// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>
#include <stdint.h>

// Minimal, dependency-free GTFS-Realtime (protobuf wire format) decoder.
//
// It extracts only the fields the map needs from an MTA subway FeedMessage and
// merges each trip's TripUpdate and VehiclePosition entities (joined on
// trip_id) into one fixed-size RtTrain. No heap, no STL, no networking, no GPIO:
// it works on a caller-supplied byte buffer and a caller-supplied output array,
// so the same code is usable on the host and on the ESP32.
namespace brooklyn::gtfsrt {

constexpr size_t kTripIdMax = 32;     // including NUL
constexpr size_t kRouteIdMax = 4;
constexpr size_t kStopIdMax = 8;
constexpr size_t kVehicleIdMax = 16;
constexpr size_t kTrainIdMax = 24;    // NYCT train_id, e.g. "EA 0259+ 207/HOW"

constexpr uint8_t kScheduleRelationshipAbsent = 0xFF;

// VehiclePosition.current_status. Absent means the field was not sent (the
// MTA does this for trips that have not started); it is NOT defaulted to
// IN_TRANSIT_TO.
enum class VehicleStatus : uint8_t {
  Absent = 0,
  IncomingAt = 1,   // protobuf INCOMING_AT (0)
  StoppedAt = 2,    // protobuf STOPPED_AT (1)
  InTransitTo = 3,  // protobuf IN_TRANSIT_TO (2)
};

// NYCT extension TripDescriptor.direction (1001.3).
enum class NyctDirection : uint8_t { None = 0, North = 1, East = 2, South = 3, West = 4 };

struct RtTrain {
  char tripId[kTripIdMax];
  char routeId[kRouteIdMax];
  char vehicleId[kVehicleIdMax];   // VehicleDescriptor.id (often empty at the MTA)
  char trainId[kTrainIdMax];       // NYCT train_id (often empty)
  char vehicleStopId[kStopIdMax];  // VehiclePosition.stop_id
  char nextStopId[kStopIdMax];     // first TripUpdate.stop_time_update.stop_id
  char secondStopId[kStopIdMax];   // second stop_time_update.stop_id (Phase 4F)
  uint32_t vehicleTimestamp;       // 0 when absent
  uint32_t tripUpdateTimestamp;    // 0 when absent
  uint32_t nextStopArrival;        // first stop_time_update arrival.time, 0 if absent
  uint32_t nextStopDeparture;      // first stop_time_update departure.time, 0 if absent
  uint32_t currentStopSequence;    // valid when hasCurrentStopSequence
  uint16_t stopTimeUpdateCount;
  VehicleStatus status;
  NyctDirection nyctDirection;
  uint8_t scheduleRelationship;    // TripDescriptor.schedule_relationship or kScheduleRelationshipAbsent
  bool hasVehicle;
  bool hasTripUpdate;
  bool hasCurrentStopSequence;
  bool fieldTruncated;             // an id did not fit its buffer
};

struct FeedStats {
  uint32_t headerTimestamp;   // FeedHeader.timestamp, 0 when absent
  uint32_t entities;          // FeedEntity messages seen
  uint32_t tripUpdates;
  uint32_t vehicles;
  uint32_t alerts;            // counted, not decoded
  uint32_t deleted;           // is_deleted entities (ignored)
  uint32_t malformedEntities; // bad entity payload or missing trip_id (skipped)
  uint32_t droppedForCapacity;
  uint32_t routeConflicts;    // TripUpdate and VehiclePosition disagree on route_id
};

enum class DecodeStatus : uint8_t {
  Ok,
  Empty,             // zero-length input
  Truncated,         // outer framing ran past the end of the buffer
  Malformed,         // outer framing is not valid protobuf wire format
  CapacityExceeded,  // more trips than the output array holds (partial result)
};

// Decodes `data` and writes up to `capacity` merged trips to `out`.
// `count` receives the number of trips written. Per-entity damage is skipped
// and counted in `stats.malformedEntities`; damage to the outer framing stops
// decoding and returns Truncated/Malformed with whatever was decoded so far.
DecodeStatus decodeFeed(const uint8_t *data, size_t length, RtTrain *out,
                        size_t capacity, size_t &count, FeedStats &stats);

// 'N', 'S', 'E', 'W' from (in order) the platform suffix of the vehicle or next
// stop id, the NYCT direction, or the trip_id's ".N"/".S" marker; '?' if unknown.
char travelDirection(const RtTrain &train);

}  // namespace brooklyn::gtfsrt
