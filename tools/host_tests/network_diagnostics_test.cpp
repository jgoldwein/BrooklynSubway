// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "network_diagnostics.h"

#include <stdio.h>
#include <string.h>

namespace {

int checks = 0;
int failures = 0;

#define CHECK(condition)                                                     \
  do {                                                                       \
    ++checks;                                                                \
    if (!(condition)) {                                                      \
      ++failures;                                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);            \
    }                                                                        \
  } while (0)

using brooklyn::network::diagnostics::CheckResult;
using brooklyn::network::diagnostics::CheckState;
using brooklyn::network::diagnostics::DnsTarget;
using brooklyn::network::diagnostics::Probe;
using brooklyn::network::diagnostics::Runner;
using brooklyn::network::diagnostics::Snapshot;

class FakeProbe : public Probe {
 public:
  bool connected = true;
  CheckState ntpState = CheckState::Succeeded;
  CheckState mtaState = CheckState::Succeeded;
  CheckState tcpState = CheckState::Succeeded;
  bool pendingNtpOnce = false;
  uint32_t tcpElapsedMs = 23;
  uint32_t ntpElapsedMs = 13;
  uint32_t mtaElapsedMs = 17;
  uint32_t ntpTimeoutMs = 0;
  uint32_t mtaTimeoutMs = 0;
  uint32_t tcpTimeoutMs = 0;
  uint16_t tcpPort = 0;
  uint32_t captureCalls = 0;
  uint32_t ntpStarts = 0;
  uint32_t mtaStarts = 0;
  uint32_t ntpPolls = 0;
  uint32_t mtaPolls = 0;
  uint32_t tcpCalls = 0;
  char ntpHostname[32] = {};
  char mtaHostname[32] = {};
  char tcpAddress[16] = {};

  void capture(Snapshot &snapshot) override {
    ++captureCalls;
    snapshot.wifiAssociated = connected;
    snprintf(snapshot.ssid, sizeof(snapshot.ssid), "Test <&\" Wi-Fi");
    snprintf(snapshot.ip, sizeof(snapshot.ip), "192.168.4.20");
    snprintf(snapshot.subnet, sizeof(snapshot.subnet), "255.255.255.0");
    snprintf(snapshot.gateway, sizeof(snapshot.gateway), "192.168.4.1");
    snprintf(snapshot.dnsPrimary, sizeof(snapshot.dnsPrimary), "192.168.4.1");
    snprintf(snapshot.dnsSecondary, sizeof(snapshot.dnsSecondary), "0.0.0.0");
    snapshot.unixTime = 1792300000;
    snapshot.hasUnixTime = true;
  }

  void startDns(DnsTarget target, const char *hostname, uint32_t timeoutMs,
                uint32_t) override {
    if (target == DnsTarget::Ntp) {
      ++ntpStarts;
      ntpTimeoutMs = timeoutMs;
      snprintf(ntpHostname, sizeof(ntpHostname), "%s", hostname);
    } else {
      ++mtaStarts;
      mtaTimeoutMs = timeoutMs;
      snprintf(mtaHostname, sizeof(mtaHostname), "%s", hostname);
    }
  }

  CheckState pollDns(DnsTarget target, uint32_t,
                     CheckResult &result) override {
    if (target == DnsTarget::Ntp) {
      ++ntpPolls;
      if (pendingNtpOnce && ntpPolls == 1) return CheckState::Pending;
      result.elapsedMs = ntpElapsedMs;
      if (ntpState == CheckState::Succeeded) {
        snprintf(result.address, sizeof(result.address), "198.51.100.10");
      }
      return ntpState;
    }
    ++mtaPolls;
    result.elapsedMs = mtaElapsedMs;
    if (mtaState == CheckState::Succeeded) {
      snprintf(result.address, sizeof(result.address), "198.51.100.20");
    }
    return mtaState;
  }

  CheckState connectTcp(const char *ipv4, uint16_t port, uint32_t timeoutMs,
                        uint32_t, uint32_t &elapsedMs) override {
    ++tcpCalls;
    tcpPort = port;
    tcpTimeoutMs = timeoutMs;
    elapsedMs = tcpElapsedMs;
    snprintf(tcpAddress, sizeof(tcpAddress), "%s", ipv4);
    return tcpState;
  }
};

void testDisconnectedSkipsNetworkProbes() {
  FakeProbe probe;
  probe.connected = false;
  Runner runner;
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::NotRun);
  CHECK(runner.start(probe));
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Complete);
  CHECK(!runner.snapshot().wifiAssociated);
  CHECK(runner.snapshot().ntpDns.state == CheckState::Skipped);
  CHECK(runner.snapshot().mtaDns.state == CheckState::Skipped);
  CHECK(runner.snapshot().tcp443.state == CheckState::Skipped);
  runner.step(probe, 1);
  CHECK(probe.ntpStarts == 0 && probe.mtaStarts == 0 && probe.tcpCalls == 0);
}

void testSuccessfulRunIsStagedAndBounded() {
  FakeProbe probe;
  probe.pendingNtpOnce = true;
  Runner runner;
  CHECK(runner.start(probe));
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Running);
  CHECK(!runner.start(probe));

  runner.step(probe, 100);
  CHECK(runner.snapshot().ntpDns.state == CheckState::Pending);
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Running);
  runner.step(probe, 101);
  CHECK(runner.snapshot().ntpDns.state == CheckState::Succeeded);
  CHECK(strcmp(runner.snapshot().ntpDns.address, "198.51.100.10") == 0);
  CHECK(runner.snapshot().ntpDns.elapsedMs == probe.ntpElapsedMs);
  runner.step(probe, 102);
  CHECK(runner.snapshot().mtaDns.state == CheckState::Succeeded);
  CHECK(strcmp(runner.snapshot().mtaDns.address, "198.51.100.20") == 0);
  CHECK(runner.snapshot().mtaDns.elapsedMs == probe.mtaElapsedMs);
  runner.step(probe, 103);
  CHECK(runner.snapshot().tcp443.state == CheckState::Succeeded);
  CHECK(runner.snapshot().tcp443.elapsedMs == probe.tcpElapsedMs);
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Complete);

  CHECK(probe.captureCalls == 1);
  CHECK(probe.ntpStarts == 1 && probe.mtaStarts == 1 && probe.tcpCalls == 1);
  CHECK(strcmp(probe.ntpHostname,
               brooklyn::network::diagnostics::kNtpHostname) == 0);
  CHECK(strcmp(probe.mtaHostname,
               brooklyn::network::diagnostics::kMtaHostname) == 0);
  CHECK(probe.ntpTimeoutMs ==
        brooklyn::network::diagnostics::kDnsTimeoutMs);
  CHECK(probe.mtaTimeoutMs ==
        brooklyn::network::diagnostics::kDnsTimeoutMs);
  CHECK(probe.tcpPort == 443);
  CHECK(probe.tcpTimeoutMs ==
        brooklyn::network::diagnostics::kTcpConnectTimeoutMs);
  CHECK(strcmp(probe.tcpAddress, "198.51.100.20") == 0);
}

void testFailuresSkipDependentTcpAndRerunsReplaceSnapshot() {
  FakeProbe probe;
  probe.ntpState = CheckState::TimedOut;
  probe.mtaState = CheckState::Failed;
  Runner runner;
  CHECK(runner.start(probe));
  runner.step(probe, 1);
  runner.step(probe, 2);
  runner.step(probe, 3);
  CHECK(runner.snapshot().ntpDns.state == CheckState::TimedOut);
  CHECK(runner.snapshot().mtaDns.state == CheckState::Failed);
  CHECK(runner.snapshot().tcp443.state == CheckState::Skipped);
  CHECK(probe.tcpCalls == 0);

  probe.ntpState = CheckState::Succeeded;
  probe.mtaState = CheckState::Succeeded;
  probe.tcpState = CheckState::Failed;
  CHECK(runner.start(probe));
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Running);
  CHECK(runner.snapshot().ntpDns.state == CheckState::NotRun);
  CHECK(runner.snapshot().mtaDns.state == CheckState::NotRun);
  CHECK(runner.snapshot().tcp443.state == CheckState::NotRun);
  runner.step(probe, 4);
  runner.step(probe, 5);
  runner.step(probe, 6);
  CHECK(runner.snapshot().tcp443.state == CheckState::Failed);
  CHECK(runner.snapshot().tcp443.elapsedMs == probe.tcpElapsedMs);
}

void testDiagnosticsJsonEscapingAndShape() {
  FakeProbe probe;
  Runner runner;
  CHECK(runner.start(probe));
  runner.step(probe, 1);
  runner.step(probe, 2);
  runner.step(probe, 3);
  char json[768];
  const size_t length = brooklyn::web::renderNetworkDiagnosticsJson(
      json, sizeof(json), runner.snapshot());
  CHECK(length > 0);
  CHECK(strstr(json, "\"state\":\"complete\"") != nullptr);
  CHECK(strstr(json, "\"wifiAssociated\":true") != nullptr);
  CHECK(strstr(json, "\"ssid\":\"Test <&\\\" Wi-Fi\"") != nullptr);
  CHECK(strstr(json, "\"mtaTcp443\":{\"state\":\"succeeded\"") != nullptr);
  CHECK(strstr(json, "\"unixTime\":1792300000") != nullptr);
  CHECK(strstr(json, "password") == nullptr);
  CHECK(brooklyn::web::renderNetworkDiagnosticsJson(
            json, 4, runner.snapshot()) == 0);
}

}  // namespace

int main() {
  static_assert(sizeof(Snapshot) <= 200,
                "network diagnostic snapshot must stay fixed and small");
  testDisconnectedSkipsNetworkProbes();
  testSuccessfulRunIsStagedAndBounded();
  testFailuresSkipDependentTcpAndRerunsReplaceSnapshot();
  testDiagnosticsJsonEscapingAndShape();
  printf("Network diagnostics tests: %d checks, %d failures\n", checks,
         failures);
  return failures == 0 ? 0 : 1;
}
