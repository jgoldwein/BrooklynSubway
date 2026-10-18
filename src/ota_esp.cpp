// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "ota_esp.h"

#if BROOKLYN_NETWORK_ENABLED

#include "lan_web_policy.h"
#include "ota_trusted_keys.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>

#include <new>

#if defined(BROOKLYN_TARGET_XIAO)
#define BROOKLYN_OTA_TARGET ::brooklyn::ota::Target::Xiao
#define BROOKLYN_OTA_TARGET_NAME "xiao-s3"
#elif defined(BROOKLYN_TARGET_PCB)
#define BROOKLYN_OTA_TARGET ::brooklyn::ota::Target::BrooklynProduction
#define BROOKLYN_OTA_TARGET_NAME "brooklyn-s3"
#else
#error "OTA requires BROOKLYN_TARGET_XIAO or BROOKLYN_TARGET_PCB"
#endif

namespace brooklyn::ota {
namespace {

class MbedSha256 : public Sha256Stream {
 public:
  MbedSha256() { mbedtls_sha256_init(&context_); }
  ~MbedSha256() override { mbedtls_sha256_free(&context_); }
  void begin() override {
    mbedtls_sha256_starts(&context_, 0);
  }
  void update(const uint8_t *data, size_t length) override {
    mbedtls_sha256_update(&context_, data, length);
  }
  void finish(uint8_t out[kDigestSize]) override {
    mbedtls_sha256_finish(&context_, out);
  }

 private:
  mbedtls_sha256_context context_;
};

class MbedVerifier : public SignatureVerifier {
 public:
  bool verify(const uint8_t publicKey[kPublicKeySize], const uint8_t *message,
              size_t messageLength,
              const uint8_t signature[kSignatureSize]) override {
    uint8_t hash[kDigestSize];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, message, messageLength);
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);

    mbedtls_ecp_group group;
    mbedtls_ecp_point point;
    mbedtls_mpi r;
    mbedtls_mpi s;
    mbedtls_ecp_group_init(&group);
    mbedtls_ecp_point_init(&point);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    bool valid =
        mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
        mbedtls_ecp_point_read_binary(&group, &point, publicKey,
                                      kPublicKeySize) == 0 &&
        mbedtls_ecp_check_pubkey(&group, &point) == 0 &&
        mbedtls_mpi_read_binary(&r, signature, 32) == 0 &&
        mbedtls_mpi_read_binary(&s, signature + 32, 32) == 0 &&
        mbedtls_ecdsa_verify(&group, hash, sizeof(hash), &point, &r, &s) == 0;
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_ecp_point_free(&point);
    mbedtls_ecp_group_free(&group);
    return valid;
  }
};

// Streams into the inactive OTA slot. The candidate becomes bootable only in
// finalizeAndActivate(), after the installer has verified signature and
// digest. Nothing here touches NVS or the running slot.
class EspImageWriter : public ImageWriter {
 public:
  size_t capacity() override {
    partition_ = esp_ota_get_next_update_partition(nullptr);
    return partition_ != nullptr ? partition_->size : 0;
  }
  bool begin(uint32_t payloadSize) override {
    if (partition_ == nullptr || payloadSize > partition_->size) return false;
    if (esp_ota_begin(partition_, payloadSize, &handle_) != ESP_OK) {
      return false;
    }
    open_ = true;
    return true;
  }
  bool write(const uint8_t *data, size_t length) override {
    return open_ && esp_ota_write(handle_, data, length) == ESP_OK;
  }
  bool finalizeAndActivate() override {
    if (!open_) return false;
    open_ = false;
    if (esp_ota_end(handle_) != ESP_OK) return false;
    return esp_ota_set_boot_partition(partition_) == ESP_OK;
  }
  void abort() override {
    if (open_) {
      esp_ota_abort(handle_);
      open_ = false;
    }
  }

 private:
  const esp_partition_t *partition_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  bool open_ = false;
};

bool gSetupHealthy = false;
bool gSetupReported = false;
bool gMarkedValid = false;
constexpr uint32_t kHealthyUptimeMs = 15000;

}  // namespace

class WebOtaSession::Impl {
 public:
  MbedSha256 sha;
  MbedVerifier verifier;
  EspImageWriter writer;
};

WebOtaSession::WebOtaSession() : impl_(new (std::nothrow) Impl) {}
WebOtaSession::~WebOtaSession() = default;

const char *WebOtaSession::targetName() { return BROOKLYN_OTA_TARGET_NAME; }

Result WebOtaSession::start(size_t contentLength) {
  if (!impl_) return Result::WriterBegin;
  if (installer_) installer_->abort();
  const InstallerConfig config{BROOKLYN_OTA_TARGET, kTrustedKeys,
                               kTrustedKeyCount, web::kFirmwareId};
  installer_.reset(new (std::nothrow) Installer(config, impl_->sha,
                                                impl_->verifier,
                                                impl_->writer));
  if (!installer_) return Result::WriterBegin;
  Serial.printf("OTA: update started target=%s length=%u keys=%u\n",
                BROOKLYN_OTA_TARGET_NAME,
                static_cast<unsigned>(contentLength),
                static_cast<unsigned>(
                    provisionedKeyCount(kTrustedKeys, kTrustedKeyCount)));
  const Result result = installer_->start(contentLength);
  if (result != Result::Ok) {
    Serial.printf("OTA: rejected - %s\n", resultName(result));
  }
  return result;
}

Result WebOtaSession::feed(const uint8_t *data, size_t length) {
  if (!installer_) return Result::NotStarted;
  const bool hadHeader = installer_->headerValid();
  const Result result = installer_->feed(data, length);
  if (!hadHeader && installer_->headerValid()) {
    Serial.printf("OTA: header accepted version=%s key=%u downgrade=%s\n",
                  installer_->header().version,
                  static_cast<unsigned>(installer_->header().keyId),
                  installer_->downgrade() ? "YES" : "no");
  }
  if (result != Result::Ok) {
    Serial.printf("OTA: failed - %s\n", resultName(result));
  }
  return result;
}

Result WebOtaSession::finish() {
  if (!installer_) return Result::NotStarted;
  const Result result = installer_->finish();
  Serial.printf("OTA: %s\n", result == Result::Ok
                                 ? "verified; new image activated, reboot permitted"
                                 : resultName(result));
  return result;
}

void WebOtaSession::abort() {
  if (installer_) installer_->abort();
}

void healthSetupComplete(bool setupHealthy) {
  gSetupHealthy = setupHealthy;
  gSetupReported = true;
  Serial.printf("OTA health gate: setup %s; rollback confirmation after %u s\n",
                setupHealthy ? "healthy" : "UNHEALTHY (image will not be confirmed)",
                static_cast<unsigned>(kHealthyUptimeMs / 1000));
}

void healthService(uint32_t nowMs) {
  if (gMarkedValid || !gSetupReported || !gSetupHealthy) return;
  if (nowMs < kHealthyUptimeMs) return;
  gMarkedValid = true;
  const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
  Serial.printf("OTA health gate: image confirmed (%s)\n",
                result == ESP_OK ? "ok" : esp_err_to_name(result));
}

}  // namespace brooklyn::ota

// Overrides the Arduino weak default, which would confirm every image
// immediately. Confirmation now waits for healthService().
extern "C" bool verifyRollbackLater() { return true; }

#endif  // BROOKLYN_NETWORK_ENABLED
