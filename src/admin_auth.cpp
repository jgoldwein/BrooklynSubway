// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "admin_auth.h"

#include <string.h>

namespace brooklyn::admin {
namespace {

int hexValue(char character) {
  if (character >= '0' && character <= '9') return character - '0';
  if (character >= 'a' && character <= 'f') return character - 'a' + 10;
  if (character >= 'A' && character <= 'F') return character - 'A' + 10;
  return -1;
}

bool decodeHex(const char *text, uint8_t *out, size_t length) {
  if (text == nullptr || out == nullptr ||
      strlen(text) != length * 2) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const int high = hexValue(text[i * 2]);
    const int low = hexValue(text[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    out[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

void clearBytes(void *memory, size_t length) {
  volatile uint8_t *bytes = static_cast<volatile uint8_t *>(memory);
  while (length-- != 0) *bytes++ = 0;
}

}  // namespace

bool Manager::load() {
  clearSessions();
  clearBytes(&credential_, sizeof(credential_));
  configured_ = false;
  consecutiveFailures_ = 0;
  lockoutActive_ = false;
  bool present = false;
  if (store_.load == nullptr ||
      !store_.load(store_.context, credential_, present)) {
    clearBytes(&credential_, sizeof(credential_));
    return false;
  }
  if (!present) return true;
  if (credential_.version != kCredentialVersion) {
    clearBytes(&credential_, sizeof(credential_));
    return true;
  }
  configured_ = true;
  return true;
}

bool Manager::validPassword(const char *password, size_t &length) {
  if (password == nullptr) return false;
  length = strlen(password);
  return length >= kPasswordMinLength && length <= kPasswordMaxLength;
}

bool Manager::setPassword(const char *password, const char *confirmation) {
  size_t passwordLength = 0;
  size_t confirmationLength = 0;
  if (!validPassword(password, passwordLength) ||
      !validPassword(confirmation, confirmationLength) ||
      passwordLength != confirmationLength ||
      !constantTimeEqual(
          reinterpret_cast<const uint8_t *>(password),
          reinterpret_cast<const uint8_t *>(confirmation), passwordLength) ||
      crypto_.randomBytes == nullptr || crypto_.deriveVerifier == nullptr ||
      store_.save == nullptr) {
    return false;
  }

  CredentialRecord replacement{};
  replacement.version = kCredentialVersion;
  if (!crypto_.randomBytes(crypto_.context, replacement.salt,
                           sizeof(replacement.salt)) ||
      !crypto_.deriveVerifier(crypto_.context, password, passwordLength,
                              replacement.salt, sizeof(replacement.salt),
                              kPbkdf2Iterations, replacement.verifier,
                              sizeof(replacement.verifier)) ||
      !store_.save(store_.context, replacement)) {
    clearBytes(&replacement, sizeof(replacement));
    return false;
  }

  clearBytes(&credential_, sizeof(credential_));
  credential_ = replacement;
  clearBytes(&replacement, sizeof(replacement));
  configured_ = true;
  consecutiveFailures_ = 0;
  lockoutActive_ = false;
  clearSessions();
  return true;
}

bool Manager::clearCredential() {
  if (store_.clear == nullptr || !store_.clear(store_.context)) return false;
  clearBytes(&credential_, sizeof(credential_));
  configured_ = false;
  consecutiveFailures_ = 0;
  lockoutActive_ = false;
  clearSessions();
  return true;
}

bool Manager::loginLocked(uint32_t nowMs) const {
  return lockoutActive_ &&
         static_cast<uint32_t>(nowMs - lockStartedAtMs_) < kLoginLockoutMs;
}

LoginResult Manager::login(const char *password, uint32_t nowMs) {
  LoginResult result{};
  if (!configured_) {
    result.code = LoginResult::Code::NotConfigured;
    return result;
  }
  if (loginLocked(nowMs)) {
    result.code = LoginResult::Code::LockedOut;
    return result;
  }
  if (lockoutActive_) {
    lockoutActive_ = false;
    consecutiveFailures_ = 0;
  }

  size_t passwordLength = password == nullptr ? 0 : strlen(password);
  uint8_t candidate[kVerifierSize] = {};
  const bool usablePassword =
      password != nullptr && passwordLength <= kPasswordMaxLength;
  const bool derived =
      usablePassword && crypto_.deriveVerifier != nullptr &&
      crypto_.deriveVerifier(crypto_.context, password, passwordLength,
                             credential_.salt, sizeof(credential_.salt),
                             kPbkdf2Iterations, candidate,
                             sizeof(candidate));
  const bool matches =
      derived && constantTimeEqual(candidate, credential_.verifier,
                                   sizeof(candidate));
  clearBytes(candidate, sizeof(candidate));

  if (!usablePassword) {
    if (consecutiveFailures_ < kLoginFailureLimit) ++consecutiveFailures_;
    if (consecutiveFailures_ >= kLoginFailureLimit) {
      lockStartedAtMs_ = nowMs;
      lockoutActive_ = true;
      result.code = LoginResult::Code::LockedOut;
    } else {
      result.code = LoginResult::Code::InvalidCredentials;
    }
    return result;
  }
  if (!derived) {
    result.code = LoginResult::Code::CryptoFailure;
    return result;
  }
  if (!matches) {
    if (consecutiveFailures_ < kLoginFailureLimit) ++consecutiveFailures_;
    if (consecutiveFailures_ >= kLoginFailureLimit) {
      lockStartedAtMs_ = nowMs;
      lockoutActive_ = true;
      result.code = LoginResult::Code::LockedOut;
    } else {
      result.code = LoginResult::Code::InvalidCredentials;
    }
    return result;
  }

  consecutiveFailures_ = 0;
  lockoutActive_ = false;
  size_t slot = kSessionCount;
  uint32_t oldestAge = 0;
  for (size_t i = 0; i < kSessionCount; ++i) {
    if (!sessions_[i].active ||
        static_cast<uint32_t>(nowMs - sessions_[i].createdAtMs) >=
            kSessionLifetimeMs) {
      slot = i;
      break;
    }
    const uint32_t age =
        static_cast<uint32_t>(nowMs - sessions_[i].createdAtMs);
    if (slot == kSessionCount || age > oldestAge) {
      slot = i;
      oldestAge = age;
    }
  }

  SessionRecord session{};
  if (crypto_.randomBytes == nullptr ||
      !crypto_.randomBytes(crypto_.context, session.token,
                           sizeof(session.token)) ||
      !crypto_.randomBytes(crypto_.context, session.csrf,
                           sizeof(session.csrf))) {
    clearBytes(&session, sizeof(session));
    result.code = LoginResult::Code::CryptoFailure;
    return result;
  }
  session.createdAtMs = nowMs;
  session.active = true;
  sessions_[slot] = session;
  if (encodeHex(session.token, sizeof(session.token), result.cookieToken,
                sizeof(result.cookieToken)) == 0) {
    clearBytes(&sessions_[slot], sizeof(sessions_[slot]));
    clearBytes(result.cookieToken, sizeof(result.cookieToken));
    clearBytes(&session, sizeof(session));
    result.code = LoginResult::Code::CryptoFailure;
    return result;
  }
  clearBytes(&session, sizeof(session));
  result.code = LoginResult::Code::Accepted;
  return result;
}

int Manager::findSession(const char *cookieHeader, uint32_t nowMs) {
  char tokenHex[kTokenSize * 2 + 1] = {};
  uint8_t token[kTokenSize] = {};
  const bool extracted =
      extractCookieToken(cookieHeader, tokenHex, sizeof(tokenHex));
  const bool decoded =
      extracted && decodeHex(tokenHex, token, sizeof(token));
  if (!decoded) {
    clearBytes(tokenHex, sizeof(tokenHex));
    clearBytes(token, sizeof(token));
    return -1;
  }
  for (size_t i = 0; i < kSessionCount; ++i) {
    SessionRecord &session = sessions_[i];
    if (!session.active) continue;
    if (static_cast<uint32_t>(nowMs - session.createdAtMs) >=
        kSessionLifetimeMs) {
      clearBytes(&session, sizeof(session));
      continue;
    }
    if (constantTimeEqual(session.token, token, sizeof(token))) {
      clearBytes(token, sizeof(token));
      clearBytes(tokenHex, sizeof(tokenHex));
      return static_cast<int>(i);
    }
  }
  clearBytes(token, sizeof(token));
  clearBytes(tokenHex, sizeof(tokenHex));
  return -1;
}

bool Manager::hasSession(const char *cookieHeader, uint32_t nowMs) {
  return findSession(cookieHeader, nowMs) >= 0;
}

bool Manager::csrfForSession(const char *cookieHeader, uint32_t nowMs,
                             char *csrfHex, size_t capacity) {
  const int index = findSession(cookieHeader, nowMs);
  if (index < 0 || csrfHex == nullptr) return false;
  return encodeHex(sessions_[index].csrf, sizeof(sessions_[index].csrf),
                   csrfHex, capacity) != 0;
}

bool Manager::authorizePost(const char *cookieHeader, const char *csrfHex,
                            uint32_t nowMs) {
  const int index = findSession(cookieHeader, nowMs);
  if (index < 0) return false;
  uint8_t candidate[kTokenSize] = {};
  const bool decoded = decodeHex(csrfHex, candidate, sizeof(candidate));
  const bool authorized =
      decoded && constantTimeEqual(sessions_[index].csrf, candidate,
                                   sizeof(candidate));
  clearBytes(candidate, sizeof(candidate));
  return authorized;
}

bool Manager::logout(const char *cookieHeader, const char *csrfHex,
                     uint32_t nowMs) {
  const int index = findSession(cookieHeader, nowMs);
  if (index < 0) return false;
  uint8_t candidate[kTokenSize] = {};
  const bool decoded = decodeHex(csrfHex, candidate, sizeof(candidate));
  const bool valid =
      decoded && constantTimeEqual(sessions_[index].csrf, candidate,
                                   sizeof(candidate));
  clearBytes(candidate, sizeof(candidate));
  if (!valid) return false;
  clearBytes(&sessions_[index], sizeof(sessions_[index]));
  return true;
}

uint8_t Manager::activeSessionCount(uint32_t nowMs) {
  uint8_t count = 0;
  for (SessionRecord &session : sessions_) {
    if (session.active &&
        static_cast<uint32_t>(nowMs - session.createdAtMs) >=
            kSessionLifetimeMs) {
      clearBytes(&session, sizeof(session));
    }
    if (session.active) ++count;
  }
  return count;
}

void Manager::clearSessions() {
  clearBytes(sessions_, sizeof(sessions_));
  clearBytes(loginNonces_, sizeof(loginNonces_));
}

bool extractCookieToken(const char *cookieHeader, char *tokenHex,
                        size_t capacity) {
  return extractNamedCookie(cookieHeader, kSessionCookieName, tokenHex,
                            capacity);
}

bool Manager::createLoginCsrf(char *cookieToken, size_t cookieCapacity,
                              char *csrfToken, size_t csrfCapacity,
                              uint32_t nowMs) {
  if (cookieToken == nullptr || csrfToken == nullptr ||
      cookieCapacity < kTokenSize * 2 + 1 ||
      csrfCapacity < kTokenSize * 2 + 1 || crypto_.randomBytes == nullptr) {
    return false;
  }
  cookieToken[0] = '\0';
  csrfToken[0] = '\0';
  size_t slot = kSessionCount;
  uint32_t oldestAge = 0;
  for (size_t i = 0; i < kSessionCount; ++i) {
    SessionRecord &nonce = loginNonces_[i];
    if (nonce.active &&
        static_cast<uint32_t>(nowMs - nonce.createdAtMs) >=
            kLoginCsrfLifetimeMs) {
      clearBytes(&nonce, sizeof(nonce));
    }
    if (!nonce.active) {
      slot = i;
      break;
    }
    const uint32_t age = static_cast<uint32_t>(nowMs - nonce.createdAtMs);
    if (slot == kSessionCount || age > oldestAge) {
      slot = i;
      oldestAge = age;
    }
  }
  SessionRecord nonce{};
  if (!crypto_.randomBytes(crypto_.context, nonce.token,
                           sizeof(nonce.token)) ||
      !crypto_.randomBytes(crypto_.context, nonce.csrf,
                           sizeof(nonce.csrf))) {
    clearBytes(&nonce, sizeof(nonce));
    return false;
  }
  nonce.createdAtMs = nowMs;
  nonce.active = true;
  const bool encodedCookie =
      encodeHex(nonce.token, sizeof(nonce.token), cookieToken,
                cookieCapacity) != 0;
  const bool encodedCsrf =
      encodeHex(nonce.csrf, sizeof(nonce.csrf), csrfToken,
                csrfCapacity) != 0;
  if (!encodedCookie || !encodedCsrf) {
    clearBytes(cookieToken, cookieCapacity);
    clearBytes(csrfToken, csrfCapacity);
    clearBytes(&nonce, sizeof(nonce));
    return false;
  }
  loginNonces_[slot] = nonce;
  clearBytes(&nonce, sizeof(nonce));
  return true;
}

bool Manager::authorizeLogin(const char *cookieHeader, const char *csrfHex,
                             uint32_t nowMs) {
  char cookieHex[kTokenSize * 2 + 1] = {};
  uint8_t token[kTokenSize] = {};
  uint8_t csrf[kTokenSize] = {};
  const bool validCookie =
      extractNamedCookie(cookieHeader, kLoginCookieName, cookieHex,
                         sizeof(cookieHex)) &&
      decodeHex(cookieHex, token, sizeof(token));
  const bool validCsrf = decodeHex(csrfHex, csrf, sizeof(csrf));
  bool authorized = false;
  for (SessionRecord &nonce : loginNonces_) {
    if (!nonce.active) continue;
    if (static_cast<uint32_t>(nowMs - nonce.createdAtMs) >=
        kLoginCsrfLifetimeMs) {
      clearBytes(&nonce, sizeof(nonce));
      continue;
    }
    const bool matches =
        validCookie && validCsrf &&
        constantTimeEqual(nonce.token, token, sizeof(token)) &&
        constantTimeEqual(nonce.csrf, csrf, sizeof(csrf));
    if (matches) {
      clearBytes(&nonce, sizeof(nonce));
      authorized = true;
    }
  }
  clearBytes(cookieHex, sizeof(cookieHex));
  clearBytes(token, sizeof(token));
  clearBytes(csrf, sizeof(csrf));
  return authorized;
}

bool extractNamedCookie(const char *cookieHeader, const char *name,
                        char *tokenHex, size_t capacity) {
  if (tokenHex == nullptr || capacity < kTokenSize * 2 + 1 ||
      name == nullptr || name[0] == '\0') {
    return false;
  }
  tokenHex[0] = '\0';
  if (cookieHeader == nullptr) return false;
  const size_t nameLength = strlen(name);
  bool found = false;
  const char *cursor = cookieHeader;
  while (*cursor != '\0') {
    while (*cursor == ' ' || *cursor == '\t' || *cursor == ';') ++cursor;
    const char *end = cursor;
    while (*end != '\0' && *end != ';') ++end;
    const char *equal = cursor;
    while (equal < end && *equal != '=') ++equal;
    if (equal < end &&
        static_cast<size_t>(equal - cursor) == nameLength &&
        memcmp(cursor, name, nameLength) == 0) {
      if (found) return false;
      const char *value = equal + 1;
      const size_t valueLength = static_cast<size_t>(end - value);
      if (valueLength != kTokenSize * 2) return false;
      for (size_t i = 0; i < valueLength; ++i) {
        if (hexValue(value[i]) < 0) return false;
      }
      memcpy(tokenHex, value, valueLength);
      tokenHex[valueLength] = '\0';
      found = true;
    }
    cursor = *end == ';' ? end + 1 : end;
  }
  return found;
}

bool isProtectedEndpoint(const char *method, const char *path) {
  if (method == nullptr || path == nullptr) return false;
  if (strcmp(method, "GET") == 0 && strcmp(path, "/wifi/setup") == 0) {
    return true;
  }
  return isMutationEndpoint(method, path);
}

bool isMutationEndpoint(const char *method, const char *path) {
  if (method == nullptr || path == nullptr || strcmp(method, "POST") != 0) {
    return false;
  }
  return strcmp(path, "/config/brightness") == 0 ||
         strcmp(path, "/config/routes") == 0 ||
         strcmp(path, "/config/dns") == 0 ||
         strcmp(path, "/wifi/setup") == 0 ||
         strcmp(path, "/wifi/setup/confirm") == 0 ||
         strcmp(path, "/login") == 0 ||
         strcmp(path, "/reboot") == 0 ||
         strcmp(path, "/network-diagnostics/run") == 0 ||
         strcmp(path, "/admin/password") == 0 ||
         strcmp(path, "/logout") == 0;
}

bool constantTimeEqual(const uint8_t *left, const uint8_t *right,
                       size_t length) {
  if (left == nullptr || right == nullptr) return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < length; ++i) {
    difference |= static_cast<uint8_t>(left[i] ^ right[i]);
  }
  return difference == 0;
}

size_t encodeHex(const uint8_t *bytes, size_t length, char *out,
                 size_t capacity) {
  static constexpr char kHex[] = "0123456789abcdef";
  if (bytes == nullptr || out == nullptr || capacity < (length * 2 + 1)) {
    if (out != nullptr && capacity != 0) out[0] = '\0';
    return 0;
  }
  for (size_t i = 0; i < length; ++i) {
    out[i * 2] = kHex[bytes[i] >> 4];
    out[i * 2 + 1] = kHex[bytes[i] & 0x0f];
  }
  out[length * 2] = '\0';
  return length * 2;
}

}  // namespace brooklyn::admin
