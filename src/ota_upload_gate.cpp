// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "ota_upload_gate.h"

namespace brooklyn::ota {

int authorizeUpload(admin::Manager &auth, const char *cookieHeader,
                    const char *csrfToken, uint32_t nowMs) {
  if (!auth.configured()) return 403;
  if (cookieHeader == nullptr || !auth.hasSession(cookieHeader, nowMs)) {
    return 401;
  }
  if (csrfToken == nullptr || csrfToken[0] == '\0' ||
      !auth.authorizePost(cookieHeader, csrfToken, nowMs)) {
    return 403;
  }
  return 0;
}

}  // namespace brooklyn::ota
