// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "admin_auth.h"

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

struct MemoryStore {
  bool present = false;
  bool failLoad = false;
  bool failSave = false;
  bool failClear = false;
  brooklyn::admin::CredentialRecord record{};
};

bool loadRecord(void *context, brooklyn::admin::CredentialRecord &record,
                bool &present) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failLoad) return false;
  present = store.present;
  if (present) record = store.record;
  return true;
}

bool saveRecord(void *context,
                const brooklyn::admin::CredentialRecord &record) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failSave) return false;
  store.record = record;
  store.present = true;
  return true;
}

bool clearRecord(void *context) {
  auto &store = *static_cast<MemoryStore *>(context);
  if (store.failClear) return false;
  memset(&store.record, 0, sizeof(store.record));
  store.present = false;
  return true;
}

struct FakeCrypto {
  uint8_t randomCounter = 1;
  bool failRandom = false;
  bool failDerive = false;
};

bool fillRandom(void *context, uint8_t *out, size_t length) {
  auto &crypto = *static_cast<FakeCrypto *>(context);
  if (crypto.failRandom || out == nullptr) return false;
  for (size_t i = 0; i < length; ++i) {
    out[i] = static_cast<uint8_t>(crypto.randomCounter + i);
  }
  ++crypto.randomCounter;
  return true;
}

bool derive(void *context, const char *password, size_t passwordLength,
            const uint8_t *salt, size_t saltLength, uint32_t iterations,
            uint8_t *verifier, size_t verifierLength) {
  auto &crypto = *static_cast<FakeCrypto *>(context);
  if (crypto.failDerive || password == nullptr || salt == nullptr ||
      verifier == nullptr) {
    return false;
  }
  uint32_t state = 2166136261u ^ iterations;
  for (size_t i = 0; i < passwordLength; ++i) {
    state = (state ^ static_cast<uint8_t>(password[i])) * 16777619u;
  }
  for (size_t i = 0; i < saltLength; ++i) {
    state = (state ^ salt[i]) * 16777619u;
  }
  for (size_t i = 0; i < verifierLength; ++i) {
    state = (state ^ static_cast<uint8_t>(i + 1)) * 16777619u;
    verifier[i] = static_cast<uint8_t>(state >> ((i % 4) * 8));
  }
  return true;
}

brooklyn::admin::Manager makeManager(MemoryStore &store, FakeCrypto &crypto) {
  brooklyn::admin::CredentialStore credentialStore{
      &store, loadRecord, saveRecord, clearRecord};
  brooklyn::admin::CryptoProvider provider{&crypto, fillRandom, derive};
  return brooklyn::admin::Manager(credentialStore, provider);
}

brooklyn::admin::LoginResult login(
    brooklyn::admin::Manager &manager, const char *password,
    uint32_t nowMs, char *cookieHeader, size_t cookieCapacity) {
  brooklyn::admin::LoginResult result = manager.login(password, nowMs);
  if (result.code == brooklyn::admin::LoginResult::Code::Accepted) {
    snprintf(cookieHeader, cookieCapacity, "%s=%s",
             brooklyn::admin::kSessionCookieName, result.cookieToken);
  }
  return result;
}

void testCredentialLifecycle() {
  MemoryStore store;
  FakeCrypto crypto;
  brooklyn::admin::Manager manager = makeManager(store, crypto);
  CHECK(manager.load());
  CHECK(!manager.configured());
  CHECK(manager.login("notconfigured", 0).code ==
        brooklyn::admin::LoginResult::Code::NotConfigured);
  CHECK(!manager.setPassword("short", "short"));
  CHECK(!manager.setPassword("longenough1", "longenough2"));
  crypto.failRandom = true;
  CHECK(!manager.setPassword("longenough1", "longenough1"));
  crypto.failRandom = false;
  store.failSave = true;
  CHECK(!manager.setPassword("longenough1", "longenough1"));
  CHECK(!manager.configured());
  store.failSave = false;
  CHECK(manager.setPassword("longenough1", "longenough1"));
  CHECK(manager.configured());
  CHECK(store.present);
  CHECK(store.record.version == brooklyn::admin::kCredentialVersion);

  brooklyn::admin::Manager reloaded = makeManager(store, crypto);
  CHECK(reloaded.load());
  CHECK(reloaded.configured());
  crypto.failDerive = true;
  CHECK(reloaded.login("longenough1", 49).code ==
        brooklyn::admin::LoginResult::Code::CryptoFailure);
  crypto.failDerive = false;
  CHECK(reloaded.login("longenough1", 50).code ==
        brooklyn::admin::LoginResult::Code::Accepted);

  store.failClear = true;
  CHECK(!reloaded.clearCredential());
  CHECK(reloaded.configured());
  store.failClear = false;
  CHECK(reloaded.clearCredential());
  CHECK(!reloaded.configured());
  CHECK(!store.present);

  store.record.version = 255;
  store.present = true;
  brooklyn::admin::Manager unsupportedVersion = makeManager(store, crypto);
  CHECK(unsupportedVersion.load());
  CHECK(!unsupportedVersion.configured());
  store.present = false;

  store.failLoad = true;
  brooklyn::admin::Manager unavailable = makeManager(store, crypto);
  CHECK(!unavailable.load());
  CHECK(!unavailable.configured());
}

void testLoginCsrf() {
  MemoryStore store;
  FakeCrypto crypto;
  brooklyn::admin::Manager manager = makeManager(store, crypto);
  CHECK(manager.load());
  CHECK(manager.setPassword("secure-pass-1", "secure-pass-1"));

  char token[33] = {};
  char csrf[33] = {};
  CHECK(manager.createLoginCsrf(token, sizeof(token), csrf, sizeof(csrf), 100));
  char header[96] = {};
  snprintf(header, sizeof(header), "other=abc; %s=%s",
           brooklyn::admin::kLoginCookieName, token);
  CHECK(manager.authorizeLogin(header, csrf, 101));
  CHECK(!manager.authorizeLogin(header, csrf, 102));

  CHECK(manager.createLoginCsrf(token, sizeof(token), csrf, sizeof(csrf), 200));
  snprintf(header, sizeof(header), "%s=%s", brooklyn::admin::kLoginCookieName,
           token);
  CHECK(!manager.authorizeLogin(header, "bad", 201));
  CHECK(manager.authorizeLogin(header, csrf, 202));

  CHECK(manager.createLoginCsrf(token, sizeof(token), csrf, sizeof(csrf), 300));
  snprintf(header, sizeof(header), "%s=%s", brooklyn::admin::kLoginCookieName,
           token);
  CHECK(!manager.authorizeLogin(header, csrf,
                                300 + brooklyn::admin::kLoginCsrfLifetimeMs));

  CHECK(manager.createLoginCsrf(token, sizeof(token), csrf, sizeof(csrf), 500));
  snprintf(header, sizeof(header), "%s=%s; %s=%s",
           brooklyn::admin::kLoginCookieName, token,
           brooklyn::admin::kLoginCookieName, token);
  CHECK(!manager.authorizeLogin(header, csrf, 501));

  char tooSmall[32] = {};
  CHECK(!manager.createLoginCsrf(tooSmall, sizeof(tooSmall), csrf,
                                 sizeof(csrf), 600));
  CHECK(tooSmall[0] == '\0');
}

void testSessionCsrfAndExpiry() {
  MemoryStore store;
  FakeCrypto crypto;
  brooklyn::admin::Manager manager = makeManager(store, crypto);
  CHECK(manager.load());
  CHECK(manager.setPassword("secure-pass-1", "secure-pass-1"));

  char cookie[96] = {};
  CHECK(login(manager, "secure-pass-1", 100, cookie, sizeof(cookie)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(manager.hasSession(cookie, 101));
  char csrf[33] = {};
  CHECK(manager.csrfForSession(cookie, 102, csrf, sizeof(csrf)));
  CHECK(manager.authorizePost(cookie, csrf, 103));
  CHECK(!manager.authorizePost(cookie, "00000000000000000000000000000000",
                               104));
  CHECK(!manager.authorizePost(cookie, nullptr, 104));
  CHECK(!manager.logout(cookie, "00000000000000000000000000000000", 105));
  CHECK(manager.logout(cookie, csrf, 106));
  CHECK(!manager.hasSession(cookie, 107));

  CHECK(login(manager, "secure-pass-1", 1000, cookie, sizeof(cookie)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(manager.hasSession(cookie, 1000 + 10 * 60 * 1000));
  CHECK(!manager.hasSession(cookie, 1000 +
                                       brooklyn::admin::kSessionLifetimeMs));

  char first[96] = {};
  char second[96] = {};
  char third[96] = {};
  CHECK(login(manager, "secure-pass-1", 100000, first, sizeof(first)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(login(manager, "secure-pass-1", 100001, second, sizeof(second)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(login(manager, "secure-pass-1", 100002, third, sizeof(third)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(manager.activeSessionCount(100003) == brooklyn::admin::kSessionCount);
  CHECK(!manager.hasSession(first, 100003));
  CHECK(manager.hasSession(second, 100003));
  CHECK(manager.hasSession(third, 100003));
}

void testRateLimitPasswordChangeAndWraparound() {
  MemoryStore store;
  FakeCrypto crypto;
  brooklyn::admin::Manager manager = makeManager(store, crypto);
  CHECK(manager.load());
  CHECK(manager.setPassword("secure-pass-1", "secure-pass-1"));
  for (uint8_t i = 0; i < brooklyn::admin::kLoginFailureLimit - 1; ++i) {
    CHECK(manager.login("wrong-password", i).code ==
          brooklyn::admin::LoginResult::Code::InvalidCredentials);
  }
  CHECK(manager.login("wrong-password", 10).code ==
        brooklyn::admin::LoginResult::Code::LockedOut);
  CHECK(manager.login("secure-pass-1", 11).code ==
        brooklyn::admin::LoginResult::Code::LockedOut);
  CHECK(manager.loginLocked(11));
  CHECK(manager.login("secure-pass-1",
                      10 + brooklyn::admin::kLoginLockoutMs).code ==
        brooklyn::admin::LoginResult::Code::Accepted);

  char oversized[brooklyn::admin::kPasswordMaxLength + 2];
  memset(oversized, 'x', sizeof(oversized) - 1);
  oversized[sizeof(oversized) - 1] = '\0';
  for (uint8_t i = 0; i < brooklyn::admin::kLoginFailureLimit; ++i) {
    const auto result = manager.login(oversized, 40000 + i);
    CHECK(result.code == (i + 1 == brooklyn::admin::kLoginFailureLimit
                              ? brooklyn::admin::LoginResult::Code::LockedOut
                              : brooklyn::admin::LoginResult::Code::InvalidCredentials));
  }
  CHECK(manager.login("secure-pass-1",
                      40000 + brooklyn::admin::kLoginFailureLimit +
                          brooklyn::admin::kLoginLockoutMs)
            .code == brooklyn::admin::LoginResult::Code::Accepted);

  char cookie[96] = {};
  CHECK(login(manager, "secure-pass-1", 40000, cookie, sizeof(cookie)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(manager.hasSession(cookie, 40000));
  CHECK(manager.setPassword("new-secure-pass", "new-secure-pass"));
  CHECK(!manager.hasSession(cookie, 40001));
  CHECK(manager.login("secure-pass-1", 40002).code ==
        brooklyn::admin::LoginResult::Code::InvalidCredentials);
  CHECK(manager.login("new-secure-pass", 40003).code ==
        brooklyn::admin::LoginResult::Code::Accepted);

  char wrapped[96] = {};
  const uint32_t started = 0xffffff00u;
  CHECK(login(manager, "new-secure-pass", started, wrapped,
              sizeof(wrapped)).code ==
        brooklyn::admin::LoginResult::Code::Accepted);
  CHECK(manager.hasSession(wrapped, 500));
  CHECK(!manager.hasSession(
      wrapped, started + brooklyn::admin::kSessionLifetimeMs));
}

void testCookieAndEndpointPolicy() {
  char token[33] = {};
  CHECK(brooklyn::admin::extractCookieToken(
      "other=x; bkmap_session=0123456789abcdef0123456789abcdef", token,
      sizeof(token)));
  CHECK(strcmp(token, "0123456789abcdef0123456789abcdef") == 0);
  CHECK(!brooklyn::admin::extractCookieToken(
      "bkmap_session=0123456789abcdef0123456789abcdef; "
      "bkmap_session=0123456789abcdef0123456789abcdef",
      token, sizeof(token)));
  CHECK(!brooklyn::admin::extractCookieToken(
      "bkmap_session=0123456789abcdef0123456789abcg", token, sizeof(token)));
  CHECK(!brooklyn::admin::extractNamedCookie("x=1", "", token, sizeof(token)));

  const char *const mutations[] = {
      "/config/brightness", "/config/routes", "/config/dns", "/wifi/setup",
      "/wifi/setup/confirm", "/reboot", "/network-diagnostics/run",
      "/admin/password", "/logout", "/login"};
  for (const char *path : mutations) {
    CHECK(brooklyn::admin::isMutationEndpoint("POST", path));
    CHECK(brooklyn::admin::isProtectedEndpoint("POST", path));
  }
  CHECK(brooklyn::admin::isProtectedEndpoint("GET", "/wifi/setup"));
  CHECK(!brooklyn::admin::isMutationEndpoint("GET", "/wifi/setup"));
  CHECK(!brooklyn::admin::isProtectedEndpoint("GET", "/"));
  CHECK(!brooklyn::admin::isProtectedEndpoint("GET", "/api/status"));
  CHECK(!brooklyn::admin::isProtectedEndpoint(
      "GET", "/api/network-diagnostics"));
  CHECK(!brooklyn::admin::isProtectedEndpoint("GET", "/login"));
  CHECK(!brooklyn::admin::isProtectedEndpoint("POST", "/admin/setup"));
  CHECK(!brooklyn::admin::isProtectedEndpoint("POST", "/unknown"));
  CHECK(!brooklyn::admin::isProtectedEndpoint("DELETE", "/config/routes"));
  CHECK(!brooklyn::admin::constantTimeEqual(
      nullptr, reinterpret_cast<const uint8_t *>(token), sizeof(token)));
}

}  // namespace

int main() {
  testCredentialLifecycle();
  testLoginCsrf();
  testSessionCsrfAndExpiry();
  testRateLimitPasswordChangeAndWraparound();
  testCookieAndEndpointPolicy();
  printf("Admin auth tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
