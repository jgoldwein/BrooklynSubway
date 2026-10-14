// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "gtfs_rt_decoder.h"

#include <string.h>

namespace brooklyn::gtfsrt {
namespace {

enum Wire : uint8_t { kVarint = 0, kFixed64 = 1, kLen = 2, kFixed32 = 5 };

struct Cursor {
  const uint8_t *p;
  const uint8_t *end;
};

bool readVarint(Cursor &c, uint64_t &v) {
  v = 0;
  for (unsigned shift = 0; shift < 64; shift += 7) {
    if (c.p >= c.end) return false;
    const uint8_t b = *c.p++;
    v |= static_cast<uint64_t>(b & 0x7F) << shift;
    if ((b & 0x80) == 0) return true;
  }
  return false;
}

// Reads a tag and, for length-delimited fields, the payload bounds.
struct Field {
  uint32_t number;
  uint8_t wire;
  uint64_t value;   // varint / fixed value
  Cursor payload;   // length-delimited
};

enum class Next : uint8_t { Ok, End, Truncated, Malformed };

Next nextField(Cursor &c, Field &f) {
  if (c.p >= c.end) return Next::End;
  uint64_t tag;
  if (!readVarint(c, tag)) return Next::Truncated;
  f.number = static_cast<uint32_t>(tag >> 3);
  f.wire = static_cast<uint8_t>(tag & 7);
  f.value = 0;
  f.payload = {nullptr, nullptr};
  if (f.number == 0) return Next::Malformed;
  switch (f.wire) {
    case kVarint:
      return readVarint(c, f.value) ? Next::Ok : Next::Truncated;
    case kFixed64:
      if (c.end - c.p < 8) return Next::Truncated;
      c.p += 8;
      return Next::Ok;
    case kFixed32:
      if (c.end - c.p < 4) return Next::Truncated;
      c.p += 4;
      return Next::Ok;
    case kLen: {
      uint64_t len;
      if (!readVarint(c, len)) return Next::Truncated;
      if (len > static_cast<uint64_t>(c.end - c.p)) return Next::Truncated;
      f.payload = {c.p, c.p + len};
      c.p += len;
      return Next::Ok;
    }
    default:
      return Next::Malformed;  // groups and unknown wire types
  }
}

void copyString(char *dst, size_t cap, const Cursor &src, bool &truncated) {
  size_t n = static_cast<size_t>(src.end - src.p);
  if (n >= cap) {
    n = cap - 1;
    truncated = true;
  }
  memcpy(dst, src.p, n);
  dst[n] = '\0';
}

uint32_t clampU32(uint64_t v) { return v > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(v); }

bool parseNyctTrip(Cursor c, RtTrain &t) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 1 && f.wire == kLen) copyString(t.trainId, sizeof t.trainId, f.payload, t.fieldTruncated);
    if (f.number == 3 && f.wire == kVarint && f.value >= 1 && f.value <= 4)
      t.nyctDirection = static_cast<NyctDirection>(f.value);
  }
  return true;
}

bool parseTripDescriptor(Cursor c, RtTrain &t) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 1 && f.wire == kLen) copyString(t.tripId, sizeof t.tripId, f.payload, t.fieldTruncated);
    else if (f.number == 4 && f.wire == kVarint) t.scheduleRelationship = static_cast<uint8_t>(f.value);
    else if (f.number == 5 && f.wire == kLen) copyString(t.routeId, sizeof t.routeId, f.payload, t.fieldTruncated);
    else if (f.number == 1001 && f.wire == kLen) {
      if (!parseNyctTrip(f.payload, t)) return false;
    }
  }
  return true;
}

bool parseVehicleDescriptor(Cursor c, RtTrain &t) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 1 && f.wire == kLen) copyString(t.vehicleId, sizeof t.vehicleId, f.payload, t.fieldTruncated);
  }
  return true;
}

bool parseStopTimeEvent(Cursor c, uint32_t &time) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 2 && f.wire == kVarint) time = clampU32(f.value);
  }
  return true;
}

bool parseStopTimeUpdate(Cursor c, RtTrain &t, bool first, bool second) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 2 && f.wire == kLen) {
      uint32_t time = 0;
      if (!parseStopTimeEvent(f.payload, time)) return false;
      if (first) t.nextStopArrival = time;
    } else if (f.number == 3 && f.wire == kLen) {
      uint32_t time = 0;
      if (!parseStopTimeEvent(f.payload, time)) return false;
      if (first) t.nextStopDeparture = time;
    } else if (f.number == 4 && f.wire == kLen) {
      if (first) copyString(t.nextStopId, sizeof t.nextStopId, f.payload, t.fieldTruncated);
      if (second) copyString(t.secondStopId, sizeof t.secondStopId, f.payload, t.fieldTruncated);
    }
  }
  return true;
}

bool parseTripUpdate(Cursor c, RtTrain &t) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 1 && f.wire == kLen) {
      if (!parseTripDescriptor(f.payload, t)) return false;
    } else if (f.number == 2 && f.wire == kLen) {
      if (!parseStopTimeUpdate(f.payload, t, t.stopTimeUpdateCount == 0,
                               t.stopTimeUpdateCount == 1)) return false;
      if (t.stopTimeUpdateCount < 0xFFFF) ++t.stopTimeUpdateCount;
    } else if (f.number == 3 && f.wire == kLen) {
      if (!parseVehicleDescriptor(f.payload, t)) return false;
    } else if (f.number == 4 && f.wire == kVarint) {
      t.tripUpdateTimestamp = clampU32(f.value);
    }
  }
  t.hasTripUpdate = true;
  return true;
}

bool parseVehicle(Cursor c, RtTrain &t) {
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n != Next::Ok) return false;
    if (f.number == 1 && f.wire == kLen) {
      if (!parseTripDescriptor(f.payload, t)) return false;
    } else if (f.number == 3 && f.wire == kVarint) {
      t.currentStopSequence = clampU32(f.value);
      t.hasCurrentStopSequence = true;
    } else if (f.number == 4 && f.wire == kVarint) {
      // protobuf enum: INCOMING_AT=0, STOPPED_AT=1, IN_TRANSIT_TO=2
      switch (f.value) {
        case 0: t.status = VehicleStatus::IncomingAt; break;
        case 1: t.status = VehicleStatus::StoppedAt; break;
        case 2: t.status = VehicleStatus::InTransitTo; break;
        default: break;  // unknown enum value: leave Absent
      }
    } else if (f.number == 5 && f.wire == kVarint) {
      t.vehicleTimestamp = clampU32(f.value);
    } else if (f.number == 7 && f.wire == kLen) {
      copyString(t.vehicleStopId, sizeof t.vehicleStopId, f.payload, t.fieldTruncated);
    } else if (f.number == 8 && f.wire == kLen) {
      if (!parseVehicleDescriptor(f.payload, t)) return false;
    }
  }
  t.hasVehicle = true;
  return true;
}

void mergeInto(RtTrain &dst, const RtTrain &src, FeedStats &stats) {
  if (dst.routeId[0] == '\0') {
    memcpy(dst.routeId, src.routeId, sizeof dst.routeId);
  } else if (src.routeId[0] != '\0' && strcmp(dst.routeId, src.routeId) != 0) {
    ++stats.routeConflicts;
  }
  if (src.scheduleRelationship != kScheduleRelationshipAbsent &&
      (dst.scheduleRelationship == kScheduleRelationshipAbsent || src.scheduleRelationship == 3))
    dst.scheduleRelationship = src.scheduleRelationship;
  if (dst.vehicleId[0] == '\0') memcpy(dst.vehicleId, src.vehicleId, sizeof dst.vehicleId);
  if (dst.trainId[0] == '\0') memcpy(dst.trainId, src.trainId, sizeof dst.trainId);
  if (dst.nyctDirection == NyctDirection::None) dst.nyctDirection = src.nyctDirection;
  dst.fieldTruncated = dst.fieldTruncated || src.fieldTruncated;
  if (src.hasVehicle) {
    dst.hasVehicle = true;
    dst.status = src.status;
    dst.vehicleTimestamp = src.vehicleTimestamp;
    dst.currentStopSequence = src.currentStopSequence;
    dst.hasCurrentStopSequence = src.hasCurrentStopSequence;
    memcpy(dst.vehicleStopId, src.vehicleStopId, sizeof dst.vehicleStopId);
  }
  if (src.hasTripUpdate) {
    dst.hasTripUpdate = true;
    dst.tripUpdateTimestamp = src.tripUpdateTimestamp;
    dst.nextStopArrival = src.nextStopArrival;
    dst.nextStopDeparture = src.nextStopDeparture;
    dst.stopTimeUpdateCount = src.stopTimeUpdateCount;
    memcpy(dst.nextStopId, src.nextStopId, sizeof dst.nextStopId);
    memcpy(dst.secondStopId, src.secondStopId, sizeof dst.secondStopId);
  }
}

void initTrain(RtTrain &t) {
  memset(&t, 0, sizeof t);
  t.scheduleRelationship = kScheduleRelationshipAbsent;
}

}  // namespace

DecodeStatus decodeFeed(const uint8_t *data, size_t length, RtTrain *out,
                        size_t capacity, size_t &count, FeedStats &stats) {
  count = 0;
  memset(&stats, 0, sizeof stats);
  if (data == nullptr || length == 0) return DecodeStatus::Empty;

  DecodeStatus result = DecodeStatus::Ok;
  Cursor c{data, data + length};
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n == Next::Truncated) return DecodeStatus::Truncated;
    if (n == Next::Malformed) return DecodeStatus::Malformed;

    if (f.number == 1 && f.wire == kLen) {  // FeedHeader
      Cursor h = f.payload;
      Field hf;
      for (Next hn; (hn = nextField(h, hf)) != Next::End;) {
        if (hn != Next::Ok) break;
        if (hf.number == 3 && hf.wire == kVarint) stats.headerTimestamp = clampU32(hf.value);
      }
      continue;
    }
    if (f.number != 2 || f.wire != kLen) continue;  // not a FeedEntity

    ++stats.entities;
    Cursor e = f.payload;
    Field ef;
    bool ok = true, deleted = false;
    Cursor tuPayload{nullptr, nullptr}, vehPayload{nullptr, nullptr};
    for (Next en; (en = nextField(e, ef)) != Next::End;) {
      if (en != Next::Ok) { ok = false; break; }
      if (ef.number == 2 && ef.wire == kVarint) deleted = ef.value != 0;
      else if (ef.number == 3 && ef.wire == kLen) tuPayload = ef.payload;
      else if (ef.number == 4 && ef.wire == kLen) vehPayload = ef.payload;
      else if (ef.number == 5 && ef.wire == kLen) ++stats.alerts;
    }
    if (!ok) { ++stats.malformedEntities; continue; }
    if (deleted) { ++stats.deleted; continue; }

    for (int kind = 0; kind < 2; ++kind) {
      const Cursor &payload = kind == 0 ? tuPayload : vehPayload;
      if (payload.p == nullptr) continue;
      if (kind == 0) ++stats.tripUpdates; else ++stats.vehicles;

      RtTrain tmp;
      initTrain(tmp);
      const bool parsed = kind == 0 ? parseTripUpdate(payload, tmp) : parseVehicle(payload, tmp);
      if (!parsed || tmp.tripId[0] == '\0') { ++stats.malformedEntities; continue; }

      RtTrain *slot = nullptr;
      for (size_t i = 0; i < count; ++i)
        if (strcmp(out[i].tripId, tmp.tripId) == 0) { slot = &out[i]; break; }
      if (slot == nullptr) {
        if (count >= capacity) { ++stats.droppedForCapacity; result = DecodeStatus::CapacityExceeded; continue; }
        slot = &out[count++];
        initTrain(*slot);
        memcpy(slot->tripId, tmp.tripId, sizeof slot->tripId);
      }
      mergeInto(*slot, tmp, stats);
    }
  }
  return result;
}

DecodeStatus visitVehicles(const uint8_t *data, size_t length, VehicleVisitor visitor,
                           void *context, FeedStats &stats) {
  memset(&stats, 0, sizeof stats);
  if (data == nullptr || length == 0) return DecodeStatus::Empty;

  Cursor c{data, data + length};
  Field f;
  for (Next n; (n = nextField(c, f)) != Next::End;) {
    if (n == Next::Truncated) return DecodeStatus::Truncated;
    if (n == Next::Malformed) return DecodeStatus::Malformed;

    if (f.number == 1 && f.wire == kLen) {  // FeedHeader
      Cursor h = f.payload;
      Field hf;
      for (Next hn; (hn = nextField(h, hf)) != Next::End;) {
        if (hn != Next::Ok) break;
        if (hf.number == 3 && hf.wire == kVarint) stats.headerTimestamp = clampU32(hf.value);
      }
      continue;
    }
    if (f.number != 2 || f.wire != kLen) continue;

    ++stats.entities;
    Cursor e = f.payload;
    Field ef;
    bool ok = true, deleted = false, hasTu = false;
    Cursor vehPayload{nullptr, nullptr};
    for (Next en; (en = nextField(e, ef)) != Next::End;) {
      if (en != Next::Ok) { ok = false; break; }
      if (ef.number == 2 && ef.wire == kVarint) deleted = ef.value != 0;
      else if (ef.number == 3 && ef.wire == kLen) hasTu = true;
      else if (ef.number == 4 && ef.wire == kLen) vehPayload = ef.payload;
      else if (ef.number == 5 && ef.wire == kLen) ++stats.alerts;
    }
    if (!ok) { ++stats.malformedEntities; continue; }
    if (deleted) { ++stats.deleted; continue; }
    if (hasTu) ++stats.tripUpdates;
    if (vehPayload.p == nullptr) continue;

    ++stats.vehicles;
    RtTrain tmp;
    initTrain(tmp);
    if (!parseVehicle(vehPayload, tmp) || tmp.tripId[0] == '\0') {
      ++stats.malformedEntities;
      continue;
    }
    if (visitor != nullptr) visitor(context, tmp, stats.headerTimestamp);
  }
  return DecodeStatus::Ok;
}
char travelDirection(const RtTrain &t) {
  const char *const stops[2] = {t.vehicleStopId, t.nextStopId};
  for (const char *id : stops) {
    const size_t n = strlen(id);
    if (n >= 4 && (id[n - 1] == 'N' || id[n - 1] == 'S')) return id[n - 1];
  }
  switch (t.nyctDirection) {
    case NyctDirection::North: return 'N';
    case NyctDirection::South: return 'S';
    case NyctDirection::East: return 'E';
    case NyctDirection::West: return 'W';
    default: break;
  }
  const char *dot = strstr(t.tripId, "..");
  if (dot && (dot[2] == 'N' || dot[2] == 'S')) return dot[2];
  return '?';
}

}  // namespace brooklyn::gtfsrt
