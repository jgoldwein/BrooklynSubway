// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace brooklyn::wan {
constexpr size_t kAddressSize = 16;
constexpr size_t kMaxBody = 64;
constexpr uint32_t kRefreshMs = 60u * 60u * 1000u;
constexpr uint32_t kRetryMs = 5u * 60u * 1000u;
constexpr const char *kUrl = "https://api.ipify.org";

// Canonical dotted IPv4 only; no embedded whitespace/NUL or leading-zero octets.
// Public mode also rejects private/shared/documentation/reserved addresses.
// Output is unchanged on error. Leading/trailing HTTP text whitespace is allowed.
bool parseIpv4(const uint8_t *data, size_t length, char out[kAddressSize], bool publicOnly);

// Loop-owned cache. Results from an old association/monitoring session are ignored.
class Cache {
 public:
  void observe(bool enabledAndOnline);
  bool due(uint32_t nowMs) const;
  void started(uint32_t nowMs);
  bool finish(const uint8_t *body, size_t length, bool downloaded,
              uint32_t nowMs, uint32_t epoch);
  const char *value(uint32_t nowMs) const;
  uint32_t epoch() const { return epoch_; }
 private:
  char address_[kAddressSize]{};
  uint32_t epoch_ = 0, attemptAt_ = 0, checkedAt_ = 0;
  bool online_ = false, attempted_ = false, valid_ = false;
};
}
