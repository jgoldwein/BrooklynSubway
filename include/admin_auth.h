// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::admin {

constexpr size_t kPasswordMinLength = 8;
constexpr size_t kPasswordMaxLength = 128;
constexpr size_t kSaltSize = 16;
constexpr size_t kVerifierSize = 32;
constexpr size_t kTokenSize = 16;
constexpr size_t kSessionCount = 2;
constexpr uint8_t kCredentialVersion = 1;
constexpr uint32_t kPbkdf2Iterations = 10000;
constexpr uint32_t kSessionLifetimeMs = 30UL * 60UL * 1000UL;
constexpr uint8_t kLoginFailureLimit = 5;
constexpr uint32_t kLoginLockoutMs = 30UL * 1000UL;
constexpr uint32_t kLoginCsrfLifetimeMs = 5UL * 60UL * 1000UL;
constexpr const char *kSessionCookieName = "bkmap_session";
constexpr const char *kLoginCookieName = "bkmap_login";

struct CredentialRecord {
  uint8_t version;
  uint8_t salt[kSaltSize];
  uint8_t verifier[kVerifierSize];
};

struct SessionRecord {
  uint8_t token[kTokenSize];
  uint8_t csrf[kTokenSize];
  uint32_t createdAtMs;
  bool active;
};

struct CredentialStore {
  void *context;
  bool (*load)(void *context, CredentialRecord &record, bool &present);
  bool (*save)(void *context, const CredentialRecord &record);
  bool (*clear)(void *context);
};

struct CryptoProvider {
  void *context;
  bool (*randomBytes)(void *context, uint8_t *out, size_t length);
  bool (*deriveVerifier)(void *context, const char *password,
                         size_t passwordLength, const uint8_t *salt,
                         size_t saltLength, uint32_t iterations,
                         uint8_t *verifier, size_t verifierLength);
};

struct NewSession {
  char cookieToken[kTokenSize * 2 + 1];
};

struct LoginResult {
  enum class Code : uint8_t {
    Accepted,
    InvalidCredentials,
    NotConfigured,
    LockedOut,
    CryptoFailure,
  } code = Code::InvalidCredentials;
  char cookieToken[kTokenSize * 2 + 1] = {};
};

class Manager {
 public:
  Manager(CredentialStore store, CryptoProvider crypto)
      : store_(store), crypto_(crypto) {}

  bool load();
  bool configured() const { return configured_; }
  bool setPassword(const char *password, const char *confirmation);
  bool clearCredential();
  LoginResult login(const char *password, uint32_t nowMs);
  bool hasSession(const char *cookieHeader, uint32_t nowMs);
  bool csrfForSession(const char *cookieHeader, uint32_t nowMs,
                      char *csrfHex, size_t capacity);
  bool authorizePost(const char *cookieHeader, const char *csrfHex,
                     uint32_t nowMs);
  bool logout(const char *cookieHeader, const char *csrfHex, uint32_t nowMs);
  bool createLoginCsrf(char *cookieToken, size_t cookieCapacity,
                       char *csrfToken, size_t csrfCapacity, uint32_t nowMs);
  bool authorizeLogin(const char *cookieHeader, const char *csrfHex,
                      uint32_t nowMs);
  uint8_t activeSessionCount(uint32_t nowMs);
  bool loginLocked(uint32_t nowMs) const;
  void clearSessions();

 private:
  int findSession(const char *cookieHeader, uint32_t nowMs);
  static bool validPassword(const char *password, size_t &length);

  CredentialStore store_;
  CryptoProvider crypto_;
  CredentialRecord credential_{};
  SessionRecord sessions_[kSessionCount]{};
  SessionRecord loginNonces_[kSessionCount]{};
  uint32_t lockStartedAtMs_ = 0;
  uint8_t consecutiveFailures_ = 0;
  bool lockoutActive_ = false;
  bool configured_ = false;
};

bool extractCookieToken(const char *cookieHeader, char *tokenHex,
                        size_t capacity);
bool extractNamedCookie(const char *cookieHeader, const char *name,
                        char *value, size_t capacity);
bool isProtectedEndpoint(const char *method, const char *path);
bool isMutationEndpoint(const char *method, const char *path);
bool constantTimeEqual(const uint8_t *left, const uint8_t *right,
                       size_t length);
size_t encodeHex(const uint8_t *bytes, size_t length, char *out,
                 size_t capacity);

CredentialStore nvsCredentialStore();
CryptoProvider espCryptoProvider();

}  // namespace brooklyn::admin
