// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "network_diagnostics.h"

#include <string.h>

namespace brooklyn::network::diagnostics {
namespace {

void markSkipped(CheckResult &result) {
  result.state = CheckState::Skipped;
}

}  // namespace

bool Runner::start(Probe &probe) {
  if (snapshot_.state == RunState::Running) return false;
  snapshot_ = Snapshot{};
  probe.capture(snapshot_);
  snapshot_.state = RunState::Running;
  if (!snapshot_.wifiAssociated) {
    markSkipped(snapshot_.ntpDns);
    markSkipped(snapshot_.mtaDns);
    markSkipped(snapshot_.tcp443);
    snapshot_.state = RunState::Complete;
    stage_ = Stage::Complete;
    return true;
  }
  stage_ = Stage::NtpDns;
  dnsStarted_ = false;
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
      stage_ = Stage::Complete;
      snapshot_.state = RunState::Complete;
      break;
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

}  // namespace brooklyn::network::diagnostics
