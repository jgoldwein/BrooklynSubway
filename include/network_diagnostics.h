// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::network::diagnostics {

enum class RunState : uint8_t { NotRun, Running, Complete };
enum class CheckState : uint8_t {
  NotRun,
  Pending,
  Succeeded,
  Failed,
  TimedOut,
  Skipped,
  Busy,
};
enum class DnsTarget : uint8_t { Ntp, Mta };
enum class DirectDnsParseState : uint8_t {
  Response,
  WrongTransaction,
  UnrelatedQuestion,
  Malformed,
};

struct CheckResult {
  CheckState state = CheckState::NotRun;
  char address[16] = {};
  uint32_t elapsedMs = 0;
};

struct DirectDnsResult {
  CheckState state = CheckState::NotRun;
  char resolver[16] = {};
  char hostname[32] = {};
  bool responseReceived = false;
  bool hasRcode = false;
  uint8_t rcode = 0;
  uint8_t addressCount = 0;
  char addresses[4][16] = {};
  uint32_t elapsedMs = 0;
};

struct Snapshot {
  RunState state = RunState::NotRun;
  bool wifiAssociated = false;
  char ssid[33] = {};
  char ip[16] = {};
  char subnet[16] = {};
  char gateway[16] = {};
  char dnsPrimary[16] = {};
  char dnsSecondary[16] = {};
  uint32_t unixTime = 0;
  bool hasUnixTime = false;
  CheckResult ntpDns{};
  CheckResult mtaDns{};
  CheckResult tcp443{};
  DirectDnsResult directDns[4]{};
};

class Probe {
 public:
  virtual void capture(Snapshot &snapshot) = 0;
  virtual void startDns(DnsTarget target, const char *hostname,
                        uint32_t timeoutMs, uint32_t nowMs) = 0;
  virtual CheckState pollDns(DnsTarget target, uint32_t nowMs,
                             CheckResult &result) = 0;
  virtual CheckState connectTcp(const char *ipv4, uint16_t port,
                                uint32_t timeoutMs, uint32_t nowMs,
                                uint32_t &elapsedMs) = 0;
  virtual void startDirectDns(size_t checkIndex, uint32_t timeoutMs,
                              uint32_t nowMs) = 0;
  virtual CheckState pollDirectDns(size_t checkIndex, uint32_t nowMs,
                                   DirectDnsResult &result) = 0;
};

class Runner {
 public:
  bool start(Probe &probe);
  void step(Probe &probe, uint32_t nowMs);
  const Snapshot &snapshot() const { return snapshot_; }

 private:
  enum class Stage : uint8_t { NtpDns, MtaDns, Tcp443, DirectDns, Complete };
  Snapshot snapshot_{};
  Stage stage_ = Stage::Complete;
  bool dnsStarted_ = false;
  size_t directDnsIndex_ = 0;
  bool directDnsStarted_ = false;
};

const char *runStateName(RunState state);
const char *checkStateName(CheckState state);
const char *directDnsParseStateName(DirectDnsParseState state);
const char *directDnsHostname(size_t checkIndex);
const char *directDnsResolver(size_t checkIndex);

DirectDnsParseState parseDirectDnsResponse(const uint8_t *packet,
                                           size_t packetLength,
                                           uint16_t transactionId,
                                           const char *expectedHostname,
                                           DirectDnsResult &result);
size_t buildDirectDnsQuery(const char *hostname, uint16_t transactionId,
                           uint8_t *packet, size_t packetCapacity);

constexpr uint32_t kDnsTimeoutMs = 4000;
constexpr uint32_t kTcpConnectTimeoutMs = 2500;
constexpr uint32_t kDirectDnsTimeoutMs = 2000;
constexpr uint16_t kMtaHttpsPort = 443;
constexpr const char *kNtpHostname = "pool.ntp.org";
constexpr const char *kMtaHostname = "api-endpoint.mta.info";
constexpr size_t kDirectDnsCheckCount = 4;

}  // namespace brooklyn::network::diagnostics
