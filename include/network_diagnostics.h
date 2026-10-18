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

struct CheckResult {
  CheckState state = CheckState::NotRun;
  char address[16] = {};
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
};

class Runner {
 public:
  bool start(Probe &probe);
  void step(Probe &probe, uint32_t nowMs);
  const Snapshot &snapshot() const { return snapshot_; }

 private:
  enum class Stage : uint8_t { NtpDns, MtaDns, Tcp443, Complete };
  Snapshot snapshot_{};
  Stage stage_ = Stage::Complete;
  bool dnsStarted_ = false;
};

const char *runStateName(RunState state);
const char *checkStateName(CheckState state);

constexpr uint32_t kDnsTimeoutMs = 4000;
constexpr uint32_t kTcpConnectTimeoutMs = 2500;
constexpr uint16_t kMtaHttpsPort = 443;
constexpr const char *kNtpHostname = "pool.ntp.org";
constexpr const char *kMtaHostname = "api-endpoint.mta.info";

}  // namespace brooklyn::network::diagnostics
