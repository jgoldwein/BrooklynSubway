// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "network_diagnostics.h"

#include <stdio.h>
#include <string.h>

namespace brooklyn::network::diagnostics {
namespace {

constexpr const char *kResolvers[] = {"1.1.1.1", "1.1.1.1", "8.8.8.8",
                                      "8.8.8.8"};
constexpr const char *kHostnames[] = {kNtpHostname, kMtaHostname, kNtpHostname,
                                      kMtaHostname};
constexpr size_t kDnsHeaderSize = 12;
constexpr size_t kMaxCnameLinks = 4;

uint16_t readU16(const uint8_t *bytes) {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                               bytes[1]);
}

bool decodeName(const uint8_t *packet, size_t packetLength, size_t &offset,
                char *name, size_t nameCapacity) {
  size_t cursor = offset;
  size_t consumed = 0;
  size_t outputLength = 0;
  bool jumped = false;
  unsigned pointerHops = 0;

  while (cursor < packetLength) {
    const uint8_t labelLength = packet[cursor];
    if ((labelLength & 0xc0) == 0xc0) {
      if (cursor + 1 >= packetLength || ++pointerHops > 16) return false;
      const size_t pointer =
          (static_cast<size_t>(labelLength & 0x3f) << 8) | packet[cursor + 1];
      if (pointer >= packetLength) return false;
      if (!jumped) consumed = cursor + 2;
      jumped = true;
      cursor = pointer;
      continue;
    }
    if ((labelLength & 0xc0) != 0) return false;
    ++cursor;
    if (!jumped) consumed = cursor;
    if (labelLength == 0) {
      if (outputLength >= nameCapacity) return false;
      name[outputLength] = '\0';
      offset = consumed;
      return true;
    }
    if (labelLength > 63 || cursor + labelLength > packetLength ||
        outputLength + labelLength + (outputLength == 0 ? 0 : 1) >=
            nameCapacity) {
      return false;
    }
    if (outputLength != 0) name[outputLength++] = '.';
    for (uint8_t i = 0; i < labelLength; ++i) {
      const uint8_t character = packet[cursor + i];
      if (character == 0 || character > 0x7f) return false;
      name[outputLength++] = static_cast<char>(character);
    }
    cursor += labelLength;
    if (!jumped) consumed = cursor;
  }
  return false;
}

char asciiLower(char value) {
  if (value >= 'A' && value <= 'Z') {
    return static_cast<char>(value - 'A' + 'a');
  }
  return value;
}

bool sameHostname(const char *left, const char *right) {
  if (left == nullptr || right == nullptr) return false;
  while (*left != '\0' && *right != '\0') {
    if (asciiLower(*left++) != asciiLower(*right++)) return false;
  }
  return *left == '\0' && *right == '\0';
}

void appendIpv4(DirectDnsResult &result, const uint8_t *address) {
  if (result.addressCount >= 4) return;
  char *destination = result.addresses[result.addressCount];
  const int length =
      snprintf(destination, sizeof(result.addresses[0]), "%u.%u.%u.%u",
               address[0], address[1], address[2], address[3]);
  if (length > 0 &&
      static_cast<size_t>(length) < sizeof(result.addresses[0])) {
    ++result.addressCount;
  }
}

bool scanAnswerRecords(const uint8_t *packet, size_t packetLength,
                       size_t answerOffset, uint16_t answerCount,
                       const char *candidateName, char *cnameTarget,
                       size_t cnameTargetCapacity, bool collectAddresses,
                       DirectDnsResult &result, bool &foundCname) {
  size_t offset = answerOffset;
  foundCname = false;
  char owner[256] = {};
  char target[256] = {};
  for (uint16_t i = 0; i < answerCount; ++i) {
    owner[0] = '\0';
    if (!decodeName(packet, packetLength, offset, owner, sizeof(owner)) ||
        offset + 10 > packetLength) {
      return false;
    }
    const uint16_t type = readU16(packet + offset);
    const uint16_t recordClass = readU16(packet + offset + 2);
    const uint16_t dataLength = readU16(packet + offset + 8);
    offset += 10;
    const size_t dataEnd = offset + dataLength;
    if (dataEnd > packetLength) return false;

    if (recordClass == 1 && sameHostname(owner, candidateName)) {
      if (type == 5 && !foundCname) {
        size_t targetOffset = offset;
        if (!decodeName(packet, packetLength, targetOffset, target,
                        sizeof(target)) ||
            targetOffset > dataEnd ||
            strlen(target) + 1 > cnameTargetCapacity) {
          return false;
        }
        memcpy(cnameTarget, target, strlen(target) + 1);
        foundCname = true;
      } else if (type == 1 && dataLength == 4 && collectAddresses) {
        appendIpv4(result, packet + offset);
      }
    }
    offset = dataEnd;
  }
  return true;
}

void markSkipped(CheckResult &result) { result.state = CheckState::Skipped; }

}  // namespace

bool Runner::start(Probe &probe) {
  if (snapshot_.state == RunState::Running) return false;
  snapshot_ = Snapshot{};
  for (size_t i = 0; i < kDirectDnsCheckCount; ++i) {
    snprintf(snapshot_.directDns[i].resolver,
             sizeof(snapshot_.directDns[i].resolver), "%s",
             directDnsResolver(i));
    snprintf(snapshot_.directDns[i].hostname,
             sizeof(snapshot_.directDns[i].hostname), "%s",
             directDnsHostname(i));
  }
  probe.capture(snapshot_);
  snapshot_.state = RunState::Running;
  if (!snapshot_.wifiAssociated) {
    markSkipped(snapshot_.ntpDns);
    markSkipped(snapshot_.mtaDns);
    markSkipped(snapshot_.tcp443);
    for (DirectDnsResult &result : snapshot_.directDns) {
      result.state = CheckState::Skipped;
    }
    snapshot_.state = RunState::Complete;
    stage_ = Stage::Complete;
    return true;
  }
  stage_ = Stage::NtpDns;
  dnsStarted_ = false;
  directDnsIndex_ = 0;
  directDnsStarted_ = false;
  return true;
}

void Runner::step(Probe &probe, uint32_t nowMs) {
  if (snapshot_.state != RunState::Running) return;

  switch (stage_) {
    case Stage::NtpDns:
    case Stage::MtaDns: {
      const bool ntp = stage_ == Stage::NtpDns;
      CheckResult &result = ntp ? snapshot_.ntpDns : snapshot_.mtaDns;
      const DnsTarget target = ntp ? DnsTarget::Ntp : DnsTarget::Mta;
      const char *hostname = ntp ? kNtpHostname : kMtaHostname;
      if (!dnsStarted_) {
        probe.startDns(target, hostname, kDnsTimeoutMs, nowMs);
        dnsStarted_ = true;
      }
      result.state = probe.pollDns(target, nowMs, result);
      if (result.state != CheckState::Pending) {
        stage_ = ntp ? Stage::MtaDns : Stage::Tcp443;
        dnsStarted_ = false;
      }
      break;
    }
    case Stage::Tcp443:
      if (snapshot_.mtaDns.state == CheckState::Succeeded) {
        snapshot_.tcp443.state = probe.connectTcp(
            snapshot_.mtaDns.address, kMtaHttpsPort, kTcpConnectTimeoutMs,
            nowMs, snapshot_.tcp443.elapsedMs);
      } else {
        markSkipped(snapshot_.tcp443);
      }
      stage_ = Stage::DirectDns;
      break;
    case Stage::DirectDns: {
      if (directDnsIndex_ >= kDirectDnsCheckCount) {
        stage_ = Stage::Complete;
        snapshot_.state = RunState::Complete;
        break;
      }
      DirectDnsResult &result = snapshot_.directDns[directDnsIndex_];
      if (!directDnsStarted_) {
        result.state = CheckState::Pending;
        probe.startDirectDns(directDnsIndex_, kDirectDnsTimeoutMs, nowMs);
        directDnsStarted_ = true;
      }
      result.state = probe.pollDirectDns(directDnsIndex_, nowMs, result);
      if (result.state != CheckState::Pending) {
        ++directDnsIndex_;
        directDnsStarted_ = false;
        if (directDnsIndex_ >= kDirectDnsCheckCount) {
          stage_ = Stage::Complete;
          snapshot_.state = RunState::Complete;
        }
      }
      break;
    }
    case Stage::Complete:
      snapshot_.state = RunState::Complete;
      break;
  }
}

const char *runStateName(RunState state) {
  switch (state) {
    case RunState::NotRun: return "not-run";
    case RunState::Running: return "running";
    case RunState::Complete: return "complete";
  }
  return "unknown";
}

const char *checkStateName(CheckState state) {
  switch (state) {
    case CheckState::NotRun: return "not-run";
    case CheckState::Pending: return "pending";
    case CheckState::Succeeded: return "succeeded";
    case CheckState::Failed: return "failed";
    case CheckState::TimedOut: return "timed-out";
    case CheckState::Skipped: return "skipped";
    case CheckState::Busy: return "busy";
  }
  return "unknown";
}

const char *directDnsParseStateName(DirectDnsParseState state) {
  switch (state) {
    case DirectDnsParseState::Response: return "response";
    case DirectDnsParseState::WrongTransaction: return "wrong-transaction";
    case DirectDnsParseState::UnrelatedQuestion: return "unrelated-question";
    case DirectDnsParseState::Malformed: return "malformed";
  }
  return "unknown";
}

const char *directDnsHostname(size_t checkIndex) {
  return checkIndex < kDirectDnsCheckCount ? kHostnames[checkIndex] : "";
}

const char *directDnsResolver(size_t checkIndex) {
  return checkIndex < kDirectDnsCheckCount ? kResolvers[checkIndex] : "";
}

DirectDnsParseState parseDirectDnsResponse(const uint8_t *packet,
                                           size_t packetLength,
                                           uint16_t transactionId,
                                           const char *expectedHostname,
                                           DirectDnsResult &result) {
  result.responseReceived = false;
  result.hasRcode = false;
  result.rcode = 0;
  result.addressCount = 0;
  memset(result.addresses, 0, sizeof(result.addresses));
  if (packet == nullptr || packetLength < kDnsHeaderSize) {
    return DirectDnsParseState::Malformed;
  }
  if (readU16(packet) != transactionId) {
    return DirectDnsParseState::WrongTransaction;
  }

  const uint16_t flags = readU16(packet + 2);
  result.responseReceived = (flags & 0x8000) != 0;
  result.hasRcode = result.responseReceived;
  result.rcode = static_cast<uint8_t>(flags & 0x000f);
  if (!result.responseReceived || (flags & 0x7800) != 0 ||
      readU16(packet + 4) != 1) {
    return DirectDnsParseState::Malformed;
  }

  size_t offset = kDnsHeaderSize;
  char question[256] = {};
  if (!decodeName(packet, packetLength, offset, question, sizeof(question)) ||
      offset + 4 > packetLength) {
    return DirectDnsParseState::Malformed;
  }
  const uint16_t questionType = readU16(packet + offset);
  const uint16_t questionClass = readU16(packet + offset + 2);
  offset += 4;
  if (!sameHostname(question, expectedHostname) || questionType != 1 ||
      questionClass != 1) {
    return DirectDnsParseState::UnrelatedQuestion;
  }

  const uint16_t answerCount = readU16(packet + 6);
  if (result.rcode == 0) {
    char currentName[256] = {};
    char nextName[256] = {};
    if (strlen(expectedHostname) >= sizeof(currentName)) {
      return DirectDnsParseState::Malformed;
    }
    memcpy(currentName, expectedHostname, strlen(expectedHostname) + 1);
    for (size_t depth = 0; depth < kMaxCnameLinks; ++depth) {
      nextName[0] = '\0';
      bool foundCname = false;
      if (!scanAnswerRecords(packet, packetLength, offset, answerCount,
                             currentName, nextName, sizeof(nextName), false,
                             result, foundCname)) {
        return DirectDnsParseState::Malformed;
      }
      if (nextName[0] != '\0' && sameHostname(nextName, currentName)) {
        return DirectDnsParseState::Malformed;
      }
      if (!foundCname) break;
      memcpy(currentName, nextName, strlen(nextName) + 1);
    }
    bool unusedCname = false;
    if (!scanAnswerRecords(packet, packetLength, offset, answerCount,
                           currentName, nextName, sizeof(nextName), true,
                           result, unusedCname)) {
      return DirectDnsParseState::Malformed;
    }
  } else {
    char ignoredCname[256] = {};
    bool unusedCname = false;
    if (!scanAnswerRecords(packet, packetLength, offset, answerCount,
                           expectedHostname, ignoredCname,
                           sizeof(ignoredCname), false, result,
                           unusedCname)) {
      return DirectDnsParseState::Malformed;
    }
  }
  return DirectDnsParseState::Response;
}

size_t buildDirectDnsQuery(const char *hostname, uint16_t transactionId,
                           uint8_t *packet, size_t packetCapacity) {
  if (hostname == nullptr || packet == nullptr ||
      packetCapacity < kDnsHeaderSize + 6) {
    return 0;
  }
  memset(packet, 0, kDnsHeaderSize);
  packet[0] = static_cast<uint8_t>(transactionId >> 8);
  packet[1] = static_cast<uint8_t>(transactionId);
  packet[2] = 0x01;
  packet[5] = 0x01;
  size_t length = kDnsHeaderSize;
  const char *label = hostname;
  while (*label != '\0') {
    const char *end = label;
    size_t labelLength = 0;
    while (*end != '\0' && *end != '.') {
      const unsigned char c = static_cast<unsigned char>(*end);
      if (c <= 0x20 || c > 0x7e) return 0;
      ++labelLength;
      ++end;
    }
    if (labelLength == 0 || labelLength > 63 ||
        length + labelLength + 1 + 5 > packetCapacity) {
      return 0;
    }
    packet[length++] = static_cast<uint8_t>(labelLength);
    for (size_t i = 0; i < labelLength; ++i) {
      packet[length++] = static_cast<uint8_t>(label[i]);
    }
    if (*end == '\0') {
      label = end;
    } else {
      label = end + 1;
      if (*label == '\0') return 0;
    }
  }
  packet[length++] = 0;
  packet[length++] = 0;
  packet[length++] = 1;
  packet[length++] = 0;
  packet[length++] = 1;
  return length;
}

}  // namespace brooklyn::network::diagnostics
