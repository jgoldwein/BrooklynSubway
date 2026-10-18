// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "admin_auth.h"

#include "network_config.h"

#if BROOKLYN_NETWORK_ENABLED

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

namespace brooklyn::admin {
namespace {

constexpr char kNamespace[] = "bkmap_cfg";
constexpr char kCredentialKey[] = "admin_v1";

bool loadCredential(void *, CredentialRecord &record, bool &present) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return false;
  present = preferences.isKey(kCredentialKey);
  const size_t read =
      present ? preferences.getBytes(kCredentialKey, &record, sizeof(record))
              : 0;
  preferences.end();
  return !present || read == sizeof(record);
}

bool saveCredential(void *, const CredentialRecord &record) {
  if (record.version != kCredentialVersion) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const size_t written =
      preferences.putBytes(kCredentialKey, &record, sizeof(record));
  preferences.end();
  return written == sizeof(record);
}

bool clearCredential(void *) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool removed = preferences.remove(kCredentialKey);
  const bool absent = !preferences.isKey(kCredentialKey);
  preferences.end();
  return removed || absent;
}

bool randomBytes(void *, uint8_t *out, size_t length) {
  if (out == nullptr || length == 0) return false;
  esp_fill_random(out, length);
  return true;
}

bool deriveVerifier(void *, const char *password, size_t passwordLength,
                    const uint8_t *salt, size_t saltLength,
                    uint32_t iterations, uint8_t *verifier,
                    size_t verifierLength) {
  if (password == nullptr || salt == nullptr || verifier == nullptr ||
      verifierLength > UINT32_MAX || iterations == 0) {
    return false;
  }
  const mbedtls_md_info_t *sha256 =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (sha256 == nullptr) return false;
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  const bool initialized = mbedtls_md_setup(&context, sha256, 1) == 0;
  const bool derived =
      initialized &&
      mbedtls_pkcs5_pbkdf2_hmac(
          &context, reinterpret_cast<const unsigned char *>(password),
          passwordLength, salt, saltLength, iterations,
          static_cast<uint32_t>(verifierLength), verifier) == 0;
  mbedtls_md_free(&context);
  return derived;
}

}  // namespace

CredentialStore nvsCredentialStore() {
  return {nullptr, loadCredential, saveCredential, clearCredential};
}

CryptoProvider espCryptoProvider() {
  return {nullptr, randomBytes, deriveVerifier};
}

}  // namespace brooklyn::admin

#else

namespace brooklyn::admin {

CredentialStore nvsCredentialStore() { return {}; }
CryptoProvider espCryptoProvider() { return {}; }

}  // namespace brooklyn::admin

#endif
