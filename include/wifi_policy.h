// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
//
// Phase 4L: pure (Arduino-free, host-testable) Wi-Fi provisioning policy.
//
// Holds credential validation, the boot/recovery policy, the replacement
// transaction, the long-press recovery detector and the captive-portal HTML
// renderer. Nothing here touches NVS, Wi-Fi or GPIO. The renderer never takes
// a password, so a password cannot appear in rendered output.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace brooklyn::wifi {

constexpr size_t kMaxSsidLen = 32;      // IEEE 802.11
constexpr size_t kMinPassLen = 8;       // WPA2-PSK passphrase
constexpr size_t kMaxPassLen = 63;
constexpr uint32_t kRecoveryHoldMs = 10000;  // manufactured rotary-push contract
constexpr size_t kMaxScanEntries = 24;

struct Credentials {
  char ssid[kMaxSsidLen + 1];
  char password[kMaxPassLen + 1];  // empty = open network
};

enum class CredentialCheck : uint8_t {
  Ok,
  EmptySsid,
  SsidTooLong,
  PasswordTooShort,  // non-empty but < 8 characters
  PasswordTooLong,
};

// Empty password = open network (allowed). SSID may be hidden/manual.
CredentialCheck validateCredentials(const char *ssid, const char *password);
const char *describeCheck(CredentialCheck check);

// ---- Storage abstraction (NVS on the device, a fake in host tests) ----------

class CredentialStore {
 public:
  virtual ~CredentialStore() = default;
  virtual bool load(Credentials &out) = 0;  // false if none stored
  virtual bool save(const Credentials &in) = 0;
  virtual bool clear() = 0;                 // removes only the map's own record
};

class RouteRegistration {
 public:
  bool beginRegistration() {
    if (registered_) return false;
    registered_ = true;
    return true;
  }

 private:
  bool registered_ = false;
};

// Replacement transaction: the candidate is persisted ONLY if the association
// with it succeeded. On failure the stored credentials are untouched.
bool commitIfAssociated(CredentialStore &store, const Credentials &candidate, bool associated);

// ---- Boot / recovery policy -------------------------------------------------

enum class BootMode : uint8_t { Provisioning, Station };

// Provisioning only when no credentials exist anywhere, or when the user
// explicitly asked for recovery. Stored credentials always win over the
// developer fallback.
BootMode decideBootMode(bool hasStored, bool hasDeveloperFallback, bool recoveryRequested);

enum class LinkEvent : uint8_t {
  SsidUnavailable,        // WL_NO_SSID_AVAIL
  AssociationFailed,
  WifiLost,
  FeedTimeout,
  DnsFailure,
  NoInternet,
  TlsFailure,
  UserRequestedRecovery,  // serial W / 10 s hold: enters the portal, keeps credentials
  UserRequestedForget,    // explicit, confirmed clear
};

// Only an explicit forget erases credentials.
bool eventErasesCredentials(LinkEvent event);
// Only an explicit user action enters provisioning mode.
bool eventEntersProvisioning(LinkEvent event);

// ---- Long-press detector (rotary push) --------------------------------------

// Feed it the debounced pressed state. Fires exactly once per continuous hold
// of at least holdMs; releasing re-arms it.
class HoldDetector {
 public:
  explicit HoldDetector(uint32_t holdMs = kRecoveryHoldMs) : holdMs_(holdMs) {}
  bool update(bool pressed, uint32_t nowMs);
  void reset() { down_ = false; fired_ = false; }

 private:
  uint32_t holdMs_;
  bool down_ = false;
  bool fired_ = false;
  uint32_t downAtMs_ = 0;
};

// ---- Scan list ---------------------------------------------------------------

struct ScanEntry {
  char ssid[kMaxSsidLen + 1];
  int16_t rssi;
  bool encrypted;
};

// Drops empty SSIDs, keeps the strongest entry per SSID, sorts by RSSI
// (strongest first). Returns the new count.
size_t dedupeScan(ScanEntry *entries, size_t count);

// ---- Portal HTML -------------------------------------------------------------

enum class PortalStatus : uint8_t {
  None,
  Scanning,
  Connecting,
  Saved,           // connected and stored
  SaveFailed,      // connected but could not store credentials
  ConnectFailed,   // old credentials kept
  InvalidInput,
};

// Writes NUL-terminated HTML into out (never overflows; truncates safely).
// Returns the length. `hasStored` only toggles a hint; no password is rendered.
size_t renderPortalPage(char *out, size_t capacity, const ScanEntry *entries, size_t count,
                        PortalStatus status, bool hasStored, const char *connectedSsid = nullptr,
                        const char *connectedIp = nullptr,
                        bool adminConfigured = false);

// Escapes &, <, >, ", ' for HTML text/attributes. Returns the length written.
size_t htmlEscape(char *out, size_t capacity, const char *in);

// Pure path classification for the captive portal.
enum class PortalRoute : uint8_t { Root, Scan, Connect, Probe, Redirect };
PortalRoute classifyPath(const char *path);

}  // namespace brooklyn::wifi
