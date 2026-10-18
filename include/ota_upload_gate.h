// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "admin_auth.h"

namespace brooklyn::ota {

// Authorization for the firmware upload, evaluated before the first body byte
// reaches the installer. Returns 0 when authorized, 401 when there is no
// administrator session, 403 when the administrator is not configured or the
// CSRF token is missing/invalid. Reuses the Phase 4V session and CSRF rules;
// there is no network-based or "trusted address" bypass.
int authorizeUpload(admin::Manager &auth, const char *cookieHeader,
                    const char *csrfToken, uint32_t nowMs);

}  // namespace brooklyn::ota
