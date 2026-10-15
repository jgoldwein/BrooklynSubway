// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Copy to include/secrets.h (git-ignored) and fill in. The firmware builds
// without secrets.h; the network then stays disabled. No API key is needed for
// the MTA GTFS-RT feeds. Never commit real credentials.

#pragma once

#define BROOKLYN_WIFI_SSID "your-network-name"
#define BROOKLYN_WIFI_PASSWORD "your-network-password"
