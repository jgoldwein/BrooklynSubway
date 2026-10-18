// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "network_diagnostics.h"

#include <stdint.h>
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
using brooklyn::network::diagnostics::DirectDnsParseState;
using brooklyn::network::diagnostics::DirectDnsResult;
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
  bool pendingDirectDnsOnce = false;
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
  uint32_t directDnsStarts = 0;
  uint32_t directDnsPolls = 0;
  uint32_t directDnsTimeoutMs = 0;
  CheckState directDnsState = CheckState::Succeeded;
  char ntpHostname[32] = {};
  char mtaHostname[32] = {};
  char tcpAddress[16] = {};
  size_t lastDirectDnsIndex = brooklyn::network::diagnostics::
      kDirectDnsCheckCount;

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

  void startDirectDns(size_t checkIndex, uint32_t timeoutMs,
                      uint32_t) override {
    ++directDnsStarts;
    lastDirectDnsIndex = checkIndex;
    directDnsTimeoutMs = timeoutMs;
  }

  CheckState pollDirectDns(size_t checkIndex, uint32_t,
                           DirectDnsResult &result) override {
    ++directDnsPolls;
    if (pendingDirectDnsOnce && directDnsPolls == 1) {
      return CheckState::Pending;
    }
    result.responseReceived = directDnsState != CheckState::TimedOut;
    result.hasRcode = result.responseReceived;
    result.rcode = 0;
    result.addressCount = directDnsState == CheckState::Succeeded ? 1 : 0;
    if (result.addressCount != 0) {
      snprintf(result.addresses[0], sizeof(result.addresses[0]),
               "203.0.113.%u", static_cast<unsigned>(checkIndex + 1));
    }
    result.elapsedMs = 19 + static_cast<uint32_t>(checkIndex);
    return directDnsState;
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
  for (const DirectDnsResult &result : runner.snapshot().directDns) {
    CHECK(result.state == CheckState::Skipped);
  }
  runner.step(probe, 1);
  CHECK(probe.ntpStarts == 0 && probe.mtaStarts == 0 && probe.tcpCalls == 0 &&
        probe.directDnsStarts == 0);
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
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Running);
  probe.pendingDirectDnsOnce = true;
  runner.step(probe, 104);
  CHECK(runner.snapshot().directDns[0].state == CheckState::Pending);
  runner.step(probe, 105);
  CHECK(runner.snapshot().directDns[0].state == CheckState::Succeeded);
  CHECK(runner.snapshot().directDns[0].responseReceived);
  CHECK(runner.snapshot().directDns[0].hasRcode);
  CHECK(runner.snapshot().directDns[0].rcode == 0);
  CHECK(strcmp(runner.snapshot().directDns[0].resolver, "1.1.1.1") == 0);
  CHECK(strcmp(runner.snapshot().directDns[0].hostname,
               brooklyn::network::diagnostics::kNtpHostname) == 0);
  CHECK(strcmp(runner.snapshot().directDns[0].addresses[0], "203.0.113.1") ==
        0);
  CHECK(runner.snapshot().directDns[0].elapsedMs == 19);
  runner.step(probe, 106);
  runner.step(probe, 107);
  runner.step(probe, 108);
  runner.step(probe, 109);
  runner.step(probe, 110);
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Complete);
  CHECK(runner.snapshot().directDns[3].state == CheckState::Succeeded);
  CHECK(strcmp(runner.snapshot().directDns[3].resolver, "8.8.8.8") == 0);
  CHECK(strcmp(runner.snapshot().directDns[3].hostname,
               brooklyn::network::diagnostics::kMtaHostname) == 0);
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
  CHECK(probe.directDnsStarts == 4 && probe.directDnsPolls == 5);
  CHECK(probe.directDnsTimeoutMs ==
        brooklyn::network::diagnostics::kDirectDnsTimeoutMs);
  CHECK(brooklyn::network::diagnostics::directDnsResolver(0) != nullptr);
  CHECK(strcmp(brooklyn::network::diagnostics::directDnsResolver(1),
               "1.1.1.1") == 0);
  CHECK(strcmp(brooklyn::network::diagnostics::directDnsResolver(2),
               "8.8.8.8") == 0);
  CHECK(strcmp(brooklyn::network::diagnostics::directDnsResolver(3),
               "8.8.8.8") == 0);
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
  runner.step(probe, 4);
  runner.step(probe, 5);
  runner.step(probe, 6);
  runner.step(probe, 7);

  probe.ntpState = CheckState::Succeeded;
  probe.mtaState = CheckState::Succeeded;
  probe.tcpState = CheckState::Failed;
  probe.directDnsState = CheckState::TimedOut;
  CHECK(runner.start(probe));
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Running);
  CHECK(runner.snapshot().ntpDns.state == CheckState::NotRun);
  CHECK(runner.snapshot().mtaDns.state == CheckState::NotRun);
  CHECK(runner.snapshot().tcp443.state == CheckState::NotRun);
  runner.step(probe, 8);
  runner.step(probe, 9);
  runner.step(probe, 10);
  runner.step(probe, 11);
  runner.step(probe, 12);
  runner.step(probe, 13);
  runner.step(probe, 14);
  CHECK(runner.snapshot().tcp443.state == CheckState::Failed);
  CHECK(runner.snapshot().tcp443.elapsedMs == probe.tcpElapsedMs);
  CHECK(runner.snapshot().state ==
        brooklyn::network::diagnostics::RunState::Complete);
  CHECK(runner.snapshot().directDns[0].state == CheckState::TimedOut);
  CHECK(!runner.snapshot().directDns[0].responseReceived);
  CHECK(runner.start(probe));
  CHECK(runner.snapshot().directDns[0].state == CheckState::NotRun);
}

void testDiagnosticsJsonEscapingAndShape() {
  FakeProbe probe;
  Runner runner;
  CHECK(runner.start(probe));
  runner.step(probe, 1);
  runner.step(probe, 2);
  runner.step(probe, 3);
  runner.step(probe, 4);
  runner.step(probe, 5);
  runner.step(probe, 6);
  runner.step(probe, 7);
  runner.step(probe, 8);
  runner.step(probe, 9);
  runner.step(probe, 10);
  char json[2048];
  const size_t length = brooklyn::web::renderNetworkDiagnosticsJson(
      json, sizeof(json), runner.snapshot());
  CHECK(length > 0);
  CHECK(strstr(json, "\"state\":\"complete\"") != nullptr);
  CHECK(strstr(json, "\"wifiAssociated\":true") != nullptr);
  CHECK(strstr(json, "\"ssid\":\"Test <&\\\" Wi-Fi\"") != nullptr);
  CHECK(strstr(json, "\"mtaTcp443\":{\"state\":\"succeeded\"") != nullptr);
  CHECK(strstr(json, "\"unixTime\":1792300000") != nullptr);
  CHECK(strstr(json, "\"directDns\":[") != nullptr);
  CHECK(strstr(json, "\"resolver\":\"1.1.1.1\"") != nullptr);
  CHECK(strstr(json, "\"hostname\":\"pool.ntp.org\"") != nullptr);
  CHECK(strstr(json, "\"addresses\":[\"203.0.113.1\"]") != nullptr);
  CHECK(strstr(json, "password") == nullptr);
  printf("DIAG_JSON_TEST:%s\n", json);
  CHECK(brooklyn::web::renderNetworkDiagnosticsJson(json, 4,
                                                          runner.snapshot()) == 0);
}

size_t encodeName(const char *hostname, uint8_t *packet, size_t offset) {
  const char *label = hostname;
  while (*label != '\0') {
    const char *end = strchr(label, '.');
    const size_t labelLength =
        end == nullptr ? strlen(label) : static_cast<size_t>(end - label);
    packet[offset++] = static_cast<uint8_t>(labelLength);
    memcpy(packet + offset, label, labelLength);
    offset += labelLength;
    if (end == nullptr) break;
    label = end + 1;
  }
  packet[offset++] = 0;
  return offset;
}

size_t makeDnsResponse(uint8_t *packet, const char *question,
                             uint16_t transactionId, uint8_t rcode,
                             const char *answerOwner,
                             const uint8_t (*addresses)[4], size_t addressCount) {
  memset(packet, 0, 512);
  packet[0] = static_cast<uint8_t>(transactionId >> 8);
  packet[1] = static_cast<uint8_t>(transactionId);
  packet[2] = 0x81;
  packet[3] = static_cast<uint8_t>(0x80 | (rcode & 0x0f));
  packet[5] = 1;
  packet[6] = static_cast<uint8_t>(addressCount >> 8);
  packet[7] = static_cast<uint8_t>(addressCount);
  size_t offset = encodeName(question, packet, 12);
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 1;
  for (size_t i = 0; i < addressCount; ++i) {
    if (answerOwner == nullptr) {
      packet[offset++] = 0xc0;
      packet[offset++] = 0x0c;
    } else {
      offset = encodeName(answerOwner, packet, offset);
    }
    packet[offset++] = 0;
    packet[offset++] = 1;
    packet[offset++] = 0;
    packet[offset++] = 1;
    packet[offset++] = 0;
    packet[offset++] = 0;
    packet[offset++] = 0;
    packet[offset++] = 30;
    packet[offset++] = 0;
    packet[offset++] = 4;
    memcpy(packet + offset, addresses[i], 4);
    offset += 4;
  }
  return offset;
}

size_t makeCnameDnsResponse(uint8_t *packet, uint16_t transactionId) {
  memset(packet, 0, 512);
  packet[0] = static_cast<uint8_t>(transactionId >> 8);
  packet[1] = static_cast<uint8_t>(transactionId);
  packet[2] = 0x81;
  packet[3] = 0x80;
  packet[5] = 1;
  packet[7] = 2;
  size_t offset = encodeName("pool.ntp.org", packet, 12);
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 1;

  packet[offset++] = 0xc0;
  packet[offset++] = 0x0c;
  packet[offset++] = 0;
  packet[offset++] = 5;
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 0;
  packet[offset++] = 0;
  packet[offset++] = 30;
  const size_t rdLengthOffset = offset;
  offset += 2;
  const size_t targetOffset = offset;
  offset = encodeName("ntp-alias.example", packet, offset);
  const size_t rdLength = offset - targetOffset;
  packet[rdLengthOffset] = static_cast<uint8_t>(rdLength >> 8);
  packet[rdLengthOffset + 1] = static_cast<uint8_t>(rdLength);

  offset = encodeName("ntp-alias.example", packet, offset);
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 0;
  packet[offset++] = 0;
  packet[offset++] = 30;
  packet[offset++] = 0;
  packet[offset++] = 4;
  packet[offset++] = 9;
  packet[offset++] = 8;
  packet[offset++] = 7;
  packet[offset++] = 6;
  return offset;
}

void testDirectDnsQueryAndResponseParser() {
  using brooklyn::network::diagnostics::buildDirectDnsQuery;
  using brooklyn::network::diagnostics::parseDirectDnsResponse;
  uint8_t query[256] = {};
  const size_t queryLength =
      buildDirectDnsQuery("pool.ntp.org", 0x1234, query, sizeof(query));
  CHECK(queryLength > 0);
  CHECK(query[0] == 0x12 && query[1] == 0x34);
  CHECK(query[2] == 0x01 && query[5] == 1);
  CHECK(buildDirectDnsQuery("bad..name", 1, query, sizeof(query)) == 0);
  CHECK(buildDirectDnsQuery("pool.ntp.org", 1, query, 12) == 0);

  const uint8_t addresses[2][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}};
  uint8_t packet[512] = {};
  DirectDnsResult result{};
  size_t length = makeDnsResponse(packet, "pool.ntp.org", 0x1234, 0, nullptr,
                                        addresses, 1);
  CHECK(parseDirectDnsResponse(packet, length, 0x1234, "pool.ntp.org",
                                     result) == DirectDnsParseState::Response);
  CHECK(result.responseReceived && result.hasRcode && result.rcode == 0);
  CHECK(result.addressCount == 1);
  CHECK(strcmp(result.addresses[0], "1.2.3.4") == 0);

  length = makeDnsResponse(packet, "pool.ntp.org", 0x1234, 3, nullptr,
                                 addresses, 0);
  CHECK(parseDirectDnsResponse(packet, length, 0x1234, "pool.ntp.org",
                                     result) == DirectDnsParseState::Response);
  CHECK(result.responseReceived && result.rcode == 3);
  CHECK(result.addressCount == 0);

  length = makeDnsResponse(packet, "pool.ntp.org", 0x1234, 0, nullptr,
                                 addresses, 1);
  CHECK(parseDirectDnsResponse(packet, length, 0x9999, "pool.ntp.org",
                                     result) ==
        DirectDnsParseState::WrongTransaction);
  CHECK(parseDirectDnsResponse(packet, length - 1, 0x1234, "pool.ntp.org",
                                     result) == DirectDnsParseState::Malformed);

  length = makeDnsResponse(packet, "pool.ntp.org", 0x1234, 0,
                                 "unrelated.example", addresses, 1);
  CHECK(parseDirectDnsResponse(packet, length, 0x1234, "pool.ntp.org",
                                     result) == DirectDnsParseState::Response);
  CHECK(result.responseReceived && result.rcode == 0);
  CHECK(result.addressCount == 0);

  length = makeDnsResponse(packet, "pool.ntp.org", 0x1234, 0, nullptr,
                                 addresses, 2);
  CHECK(parseDirectDnsResponse(packet, length, 0x1234, "POOL.NTP.ORG",
                                     result) == DirectDnsParseState::Response);
  CHECK(result.addressCount == 2);
  CHECK(strcmp(result.addresses[0], "1.2.3.4") == 0);
  CHECK(strcmp(result.addresses[1], "5.6.7.8") == 0);

  length = makeCnameDnsResponse(packet, 0x1234);
  CHECK(parseDirectDnsResponse(packet, length, 0x1234, "pool.ntp.org",
                               result) == DirectDnsParseState::Response);
  CHECK(result.addressCount == 1);
  CHECK(strcmp(result.addresses[0], "9.8.7.6") == 0);

  length = makeDnsResponse(packet, "pool.ntp.org", 0x1234, 0, nullptr,
                                 addresses, 0);
  CHECK(parseDirectDnsResponse(packet, length, 0x1234, "pool.ntp.org",
                                     result) == DirectDnsParseState::Response);
  CHECK(result.addressCount == 0);
  CHECK(parseDirectDnsResponse(nullptr, 0, 0x1234, "pool.ntp.org", result) ==
        DirectDnsParseState::Malformed);
}

}  // namespace

int main() {
  static_assert(sizeof(Snapshot) <= 720,
                "network diagnostic snapshot must stay fixed and small");
  testDisconnectedSkipsNetworkProbes();
  testSuccessfulRunIsStagedAndBounded();
  testFailuresSkipDependentTcpAndRerunsReplaceSnapshot();
  testDiagnosticsJsonEscapingAndShape();
  testDirectDnsQueryAndResponseParser();
  printf("Network diagnostics tests: %d checks, %d failures\n", checks,
         failures);
  return failures == 0 ? 0 : 1;
}
