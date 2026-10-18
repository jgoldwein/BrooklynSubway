// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4L host tests: Wi-Fi provisioning policy and feed failure diagnostics.

#include "feed_transport.h"
#include "wifi_policy.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

using namespace brooklyn::wifi;
namespace tr = brooklyn::transport;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                          \
  do {                                                       \
    ++checks;                                                \
    if (!(cond)) {                                           \
      ++failures;                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                        \
  } while (0)

constexpr const char *kSecret = "hunter2-SECRET-pw";

struct FakeStore : CredentialStore {
  bool has = false;
  Credentials cred{};
  bool otherNamespaceIntact = true;  // models data outside the map's namespace
  int saves = 0;
  bool load(Credentials &out) override {
    if (!has) return false;
    out = cred;
    return true;
  }
  bool save(const Credentials &in) override {
    cred = in;
    has = true;
    ++saves;
    return true;
  }
  bool clear() override {
    has = false;
    cred = Credentials{};
    return true;
  }
};

Credentials make(const char *ssid, const char *pass) {
  Credentials c{};
  strncpy(c.ssid, ssid, kMaxSsidLen);
  strncpy(c.password, pass, kMaxPassLen);
  return c;
}

void testValidation() {
  CHECK(validateCredentials("Home", "password1") == CredentialCheck::Ok);
  CHECK(validateCredentials("Home", "") == CredentialCheck::Ok);  // open network
  CHECK(validateCredentials("Home", nullptr) == CredentialCheck::Ok);
  CHECK(validateCredentials("", "password1") == CredentialCheck::EmptySsid);
  CHECK(validateCredentials(nullptr, "password1") == CredentialCheck::EmptySsid);
  CHECK(validateCredentials(std::string(32, 'a').c_str(), "") == CredentialCheck::Ok);
  CHECK(validateCredentials(std::string(33, 'a').c_str(), "") == CredentialCheck::SsidTooLong);
  CHECK(validateCredentials("Home", "1234567") == CredentialCheck::PasswordTooShort);
  CHECK(validateCredentials("Home", "12345678") == CredentialCheck::Ok);
  CHECK(validateCredentials("Home", std::string(63, 'p').c_str()) == CredentialCheck::Ok);
  CHECK(validateCredentials("Home", std::string(64, 'p').c_str()) == CredentialCheck::PasswordTooLong);
  CHECK(validateCredentials("Hidden Net \xE2\x80\x99", "password1") == CredentialCheck::Ok);
  CHECK(strlen(describeCheck(CredentialCheck::EmptySsid)) > 0);
}

void testBootPolicy() {
  CHECK(decideBootMode(false, false, false) == BootMode::Provisioning);  // first boot
  CHECK(decideBootMode(true, false, false) == BootMode::Station);
  CHECK(decideBootMode(false, true, false) == BootMode::Station);        // dev fallback
  CHECK(decideBootMode(true, true, false) == BootMode::Station);
  CHECK(decideBootMode(true, false, true) == BootMode::Provisioning);    // explicit recovery
  CHECK(decideBootMode(true, true, true) == BootMode::Provisioning);

  const LinkEvent temporary[] = {LinkEvent::SsidUnavailable, LinkEvent::AssociationFailed,
                                 LinkEvent::WifiLost,        LinkEvent::FeedTimeout,
                                 LinkEvent::DnsFailure,      LinkEvent::NoInternet,
                                 LinkEvent::TlsFailure};
  for (LinkEvent e : temporary) {
    CHECK(!eventErasesCredentials(e));
    CHECK(!eventEntersProvisioning(e));
  }
  CHECK(eventEntersProvisioning(LinkEvent::UserRequestedRecovery));
  CHECK(!eventErasesCredentials(LinkEvent::UserRequestedRecovery));  // keeps credentials
  CHECK(eventErasesCredentials(LinkEvent::UserRequestedForget));
}

void testRouteRegistrationIsIdempotent() {
  RouteRegistration routes;
  CHECK(routes.beginRegistration());
  CHECK(!routes.beginRegistration());
  CHECK(!routes.beginRegistration());
}

void testTransaction() {
  FakeStore store;
  const Credentials a = make("NetA", "passwordA1");
  const Credentials b = make("NetB", "passwordB1");

  CHECK(commitIfAssociated(store, a, true));
  CHECK(store.has && strcmp(store.cred.ssid, "NetA") == 0);

  // Failed replacement keeps A.
  CHECK(!commitIfAssociated(store, b, false));
  CHECK(strcmp(store.cred.ssid, "NetA") == 0 && strcmp(store.cred.password, "passwordA1") == 0);
  CHECK(store.saves == 1);

  // Successful replacement stores B.
  CHECK(commitIfAssociated(store, b, true));
  CHECK(strcmp(store.cred.ssid, "NetB") == 0);

  // Invalid candidate never persists, even if "associated".
  const Credentials bad = make("", "x");
  CHECK(!commitIfAssociated(store, bad, true));
  CHECK(strcmp(store.cred.ssid, "NetB") == 0);

  // Forget clears only the map's own record.
  CHECK(store.clear());
  CHECK(!store.has);
  CHECK(store.otherNamespaceIntact);
  Credentials out{};
  CHECK(!store.load(out));
}

void testHold() {
  HoldDetector d;  // 10 s
  CHECK(!d.update(false, 0));
  CHECK(!d.update(true, 1000));
  CHECK(!d.update(true, 10999));
  CHECK(d.update(true, 11000));   // exactly 10 s
  CHECK(!d.update(true, 20000));  // fires once
  CHECK(!d.update(false, 20001));
  CHECK(!d.update(true, 20002));  // re-armed
  CHECK(d.update(true, 30002));

  HoldDetector s;  // short press never fires
  CHECK(!s.update(true, 100));
  CHECK(!s.update(true, 600));
  CHECK(!s.update(false, 700));

  HoldDetector w;  // millis() wraparound
  CHECK(!w.update(true, 0xFFFFFF00u));
  CHECK(!w.update(true, 0x00001000u));
  CHECK(w.update(true, 0xFFFFFF00u + 10000u));
}

void testScan() {
  ScanEntry e[6] = {};
  auto set = [&](int i, const char *s, int rssi, bool enc) {
    strncpy(e[i].ssid, s, kMaxSsidLen);
    e[i].rssi = static_cast<int16_t>(rssi);
    e[i].encrypted = enc;
  };
  set(0, "Alpha", -70, true);
  set(1, "", -30, false);       // hidden: dropped
  set(2, "Beta", -50, false);
  set(3, "Alpha", -40, true);   // duplicate, stronger
  set(4, "Gamma", -80, true);
  set(5, "Beta", -90, false);
  const size_t n = dedupeScan(e, 6);
  CHECK(n == 3);
  CHECK(strcmp(e[0].ssid, "Alpha") == 0 && e[0].rssi == -40);
  CHECK(strcmp(e[1].ssid, "Beta") == 0 && e[1].rssi == -50);
  CHECK(strcmp(e[2].ssid, "Gamma") == 0);
}

void testPortal() {
  ScanEntry e[2] = {};
  strcpy(e[0].ssid, "Cafe <b>\"Wi&Fi\"</b>");
  e[0].rssi = -55;
  e[0].encrypted = true;
  strcpy(e[1].ssid, "OpenNet");
  e[1].rssi = -70;
  e[1].encrypted = false;

  static char page[8192];
  const PortalStatus all[] = {PortalStatus::None,       PortalStatus::Scanning,
                              PortalStatus::Connecting, PortalStatus::Saved,
                              PortalStatus::SaveFailed,
                              PortalStatus::ConnectFailed, PortalStatus::InvalidInput};
  for (PortalStatus s : all) {
    const size_t len = renderPortalPage(page, sizeof(page), e, 2, s, true);
    CHECK(len == strlen(page));
    CHECK(strstr(page, kSecret) == nullptr);  // no password ever rendered
    CHECK(strstr(page, "Wi-Fi setup mode") != nullptr);
    CHECK(strstr(page, "2.4 GHz") != nullptr);
    CHECK(strstr(page, "name=\"ssid\"") == nullptr);       // no manual/hidden entry
    CHECK(strstr(page, "<b>\"Wi&Fi\"") == nullptr);        // SSID is escaped
    CHECK(strstr(page, "dBm") == nullptr);
    if (s == PortalStatus::Saved) {
      CHECK(strstr(page, "<form") == nullptr);  // result page only
      CHECK(strstr(page, "name=\"confirmation\"") == nullptr);
    } else {
      CHECK(strstr(page, "name=\"pass\"") != nullptr);
      CHECK(strstr(page, "name=\"pass\" type=\"password\"") != nullptr);
      CHECK(strstr(page, "name=\"confirmation\"") != nullptr);
      CHECK(strstr(page, ">OpenNet</option>") != nullptr);
      CHECK(strstr(page, "&lt;b&gt;&quot;Wi&amp;Fi&quot;") != nullptr);
    }
    CHECK((strstr(page, "http-equiv=\"refresh\"") != nullptr) ==
          (s == PortalStatus::Scanning || s == PortalStatus::Connecting));
  }
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::None, false);
  CHECK(strstr(page, "Set the initial administrator password") != nullptr);
  CHECK(strstr(page, "name=\"password\" type=\"password\"") != nullptr);
  CHECK(strstr(page, "value=\"password") == nullptr);
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::None, true,
                   nullptr, nullptr, true);
  CHECK(strstr(page, "An administrator password is configured") != nullptr);
  CHECK(strstr(page, "Reset administrator password") != nullptr);
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::Saved, true, "TestNetwork", "192.168.1.42");
  CHECK(strstr(page, "Successfully connected to Wi-Fi: <b>TestNetwork</b>") != nullptr);
  CHECK(strstr(page, "Settings saved") != nullptr);
  CHECK(strstr(page, "LAN address: <a href=\"http://192.168.1.42/\">192.168.1.42</a>") != nullptr);
  CHECK(strstr(page, "open after reconnecting your phone to the same Wi-Fi") != nullptr);
  CHECK(strstr(page, "go to <a href=\"http://192.168.4.1/\">192.168.4.1</a>") != nullptr);
  CHECK(strstr(page, "The setup network closes shortly.") != nullptr);
  CHECK(strstr(page, "No administrator password is set") != nullptr);
  CHECK(strstr(page, kSecret) == nullptr);
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::Saved, true, "TestNetwork", "192.168.1.42", true);
  CHECK(strstr(page, "No administrator password is set") == nullptr);
  CHECK(strstr(page, "<form") == nullptr);
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::Saved, true, "TestNetwork", "10.110.202.133");
  CHECK(strstr(page, "http://10.110.202.133/\">10.110.202.133</a>") != nullptr);
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::Saved, true, e[0].ssid, "192.168.1.42");
  CHECK(strstr(page, "Wi-Fi: <b>Cafe &lt;b&gt;&quot;Wi&amp;Fi&quot;&lt;/b&gt;</b>") != nullptr);
  renderPortalPage(page, sizeof(page), e, 2, PortalStatus::SaveFailed, true, "TestNetwork", "192.168.1.42");
  CHECK(strstr(page, "Successfully connected to Wi-Fi: <b>TestNetwork</b>") != nullptr);
  CHECK(strstr(page, "settings could not be saved") != nullptr);
  CHECK(strstr(page, "LAN address: <a href=\"http://192.168.1.42/\">192.168.1.42</a>") != nullptr);
  CHECK(strstr(page, "<a href=\"http://192.168.4.1/\">192.168.4.1</a>") != nullptr);
  CHECK(strstr(page, kSecret) == nullptr);

  // Tiny buffers never overflow and stay NUL-terminated.
  for (size_t cap = 0; cap < 40; ++cap) {
    char guard[64];
    memset(guard, 0x5A, sizeof(guard));
    const size_t len = renderPortalPage(guard, cap, e, 2, PortalStatus::None, false);
    CHECK(cap == 0 || len < cap);
    CHECK(cap == 0 || guard[len] == 0);
    CHECK(guard[cap] == 0x5A || cap >= sizeof(guard));
  }

  char esc[64];
  htmlEscape(esc, sizeof(esc), "<&>\"'");
  CHECK(strcmp(esc, "&lt;&amp;&gt;&quot;&#39;") == 0);
}

void testRoutes() {
  CHECK(classifyPath("/") == PortalRoute::Root);
  CHECK(classifyPath("") == PortalRoute::Root);
  CHECK(classifyPath("/scan") == PortalRoute::Scan);
  CHECK(classifyPath("/connect") == PortalRoute::Connect);
  CHECK(classifyPath("/generate_204") == PortalRoute::Probe);
  CHECK(classifyPath("/gen_204") == PortalRoute::Probe);
  CHECK(classifyPath("/hotspot-detect.html") == PortalRoute::Probe);
  CHECK(classifyPath("/library/test/success.html") == PortalRoute::Probe);
  CHECK(classifyPath("/connecttest.txt") == PortalRoute::Probe);
  CHECK(classifyPath("/ncsi.txt") == PortalRoute::Probe);
  CHECK(classifyPath("/redirect") == PortalRoute::Probe);
  CHECK(classifyPath("/fwlink") == PortalRoute::Probe);
  CHECK(classifyPath("/fwlink/abc") == PortalRoute::Probe);
  CHECK(classifyPath("/anything/else") == PortalRoute::Redirect);
  CHECK(classifyPath("/favicon.ico") == PortalRoute::Redirect);
  CHECK(classifyPath("/unknown/captive-check") == PortalRoute::Redirect);
}

// ---- Feed diagnostics -----------------------------------------------------

struct Clk : tr::Clock {
  uint32_t now = 0;
  uint32_t nowMs() const override { return now; }
};

struct Conn : tr::FeedConnection {
  Clk *clk = nullptr;
  int variant = 0;  // 0 ok, 1 read timeout after 300 B, 2 open timeout
  size_t sent = 0;
  tr::OpenResult open(const char *, int32_t &len) override {
    sent = 0;
    if (variant == 2) {
      clk->now += 8000;
      return tr::OpenResult::Timeout;
    }
    clk->now += 100;
    len = 1000;
    return tr::OpenResult::Ok;
  }
  int32_t read(uint8_t *dest, size_t max) override {
    if (variant == 1) {
      if (sent == 0) {
        sent = 300;
        memset(dest, 1, 300 < max ? 300 : max);
        clk->now += 50;
        return 300;
      }
      clk->now += 8000;
      return tr::kReadTimeout;
    }
    const size_t n = 1000 - sent < max ? 1000 - sent : max;
    if (n == 0) return tr::kReadEof;
    memset(dest, 2, n);
    sent += n;
    return static_cast<int32_t>(n);
  }
  void close() override {}
};

struct Sink : tr::FeedSink {
  int processed = 0;
  void beginCycle() override {}
  void processFeed(const uint8_t *, size_t) override { ++processed; }
  void finishCycle() override {}
};

struct Obs : tr::FeedFailureObserver {
  std::vector<tr::FeedFailure> seen;
  void onFeedFailure(const tr::FeedFailure &f) override { seen.push_back(f); }
};

void testFeedDiagnostics() {
  static uint8_t mem[2048];
  const tr::FeedBuffer buf{mem, sizeof(mem)};
  const brooklyn::network::FeedSpec feeds[2] = {{"ACE", "u1"}, {"G", "u2"}};
  Clk clk;
  Conn conn;
  conn.clk = &clk;
  Sink sink;
  Obs obs;
  tr::TransportCounters counters{};

  // Read-idle timeout after 300 body bytes: feed identified, bytes and time reported.
  conn.variant = 1;
  tr::CycleResult r = tr::runPollCycle(sink, conn, buf, feeds, 2, {true, true}, false, counters,
                                       tr::CycleBudget(&clk, 60000), &obs);
  CHECK(r.outcome == tr::CycleOutcome::NoFeeds);
  CHECK(obs.seen.size() == 2);
  CHECK(strcmp(obs.seen[0].name, "ACE") == 0 && obs.seen[0].index == 0);
  CHECK(obs.seen[0].status == tr::DownloadStatus::Timeout);
  CHECK(obs.seen[0].bytesReceived == 300);
  CHECK(obs.seen[0].elapsedMs == 8150);
  CHECK(strcmp(obs.seen[1].name, "G") == 0);
  CHECK(counters.timeouts == 2);  // counters unchanged in meaning
  CHECK(sink.processed == 0);     // partial body never processed

  char line[96];
  formatFeedFailure(line, sizeof(line), obs.seen[0]);
  CHECK(strcmp(line, "Feed ACE FAIL: timeout bytes=300 elapsed=8150ms") == 0);

  // Connect timeout: zero bytes.
  obs.seen.clear();
  conn.variant = 2;
  clk.now = 0;
  tr::runPollCycle(sink, conn, buf, feeds, 2, {true, true}, false, counters,
                   tr::CycleBudget(&clk, 60000), &obs);
  CHECK(obs.seen.size() == 2);
  CHECK(obs.seen[0].bytesReceived == 0 && obs.seen[0].elapsedMs == 8000);

  // Success: no failure callback, and the observer is optional.
  obs.seen.clear();
  conn.variant = 0;
  r = tr::runPollCycle(sink, conn, buf, feeds, 2, {true, true}, false, counters,
                       tr::CycleBudget(&clk, 60000), &obs);
  CHECK(r.outcome == tr::CycleOutcome::Complete && obs.seen.empty());
  tr::TransportCounters c2{};
  r = tr::runPollCycle(sink, conn, buf, feeds, 2, {true, true}, false, c2);
  CHECK(r.outcome == tr::CycleOutcome::Complete);

  // Formatting never overflows and carries no body bytes.
  char tiny[8];
  const size_t n = formatFeedFailure(tiny, sizeof(tiny), obs.seen.empty() ? tr::FeedFailure{"X", 0, tr::DownloadStatus::Partial, 5, 6} : obs.seen[0]);
  CHECK(n < sizeof(tiny) && tiny[n] == 0);
}

}  // namespace

int main() {
  testValidation();
  testBootPolicy();
  testRouteRegistrationIsIdempotent();
  testTransaction();
  testHold();
  testScan();
  testPortal();
  testRoutes();
  testFeedDiagnostics();
  printf("Wi-Fi provisioning tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
