// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "network_config.h"

#if BROOKLYN_NETWORK_ENABLED

#include "ota_package.h"

#include <memory>

namespace brooklyn::ota {

// One web upload attempt: mbedTLS SHA-256/ECDSA P-256 plus esp_ota_*.
class WebOtaSession {
 public:
  WebOtaSession();
  ~WebOtaSession();
  Result start(size_t contentLength);
  Result feed(const uint8_t *data, size_t length);
  Result finish();
  void abort();
  const Installer *installer() const { return installer_.get(); }
  static const char *targetName();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
  std::unique_ptr<Installer> installer_;
};

// Boot-health gate for automatic rollback of OTA-installed images. Does not
// depend on Wi-Fi or MTA data. Call healthSetupComplete() at the end of
// setup() and healthService() from loop().
void healthSetupComplete(bool setupHealthy);
void healthService(uint32_t nowMs);

}  // namespace brooklyn::ota

#endif  // BROOKLYN_NETWORK_ENABLED
