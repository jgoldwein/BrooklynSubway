// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "wifi_policy.h"

#include <string.h>

namespace brooklyn::wifi {

namespace {

// Bounded appender; always leaves out NUL-terminated.
class Out {
 public:
  Out(char *buf, size_t cap) : buf_(buf), cap_(cap), len_(0) {
    if (cap_ != 0) buf_[0] = 0;
  }
  void put(const char *s) {
    while (*s != 0) putc(*s++);
  }
  void putc(char c) {
    if (cap_ != 0 && len_ + 1 < cap_) {
      buf_[len_++] = c;
      buf_[len_] = 0;
    }
  }
  void putInt(int v) {
    char tmp[12];
    size_t n = 0;
    unsigned u = v < 0 ? static_cast<unsigned>(-(v + 1)) + 1u : static_cast<unsigned>(v);
    do {
      tmp[n++] = static_cast<char>('0' + u % 10);
      u /= 10;
    } while (u != 0);
    if (v < 0) putc('-');
    while (n != 0) putc(tmp[--n]);
  }
  void escaped(const char *s) {
    for (; *s != 0; ++s) {
      switch (*s) {
        case '&': put("&amp;"); break;
        case '<': put("&lt;"); break;
        case '>': put("&gt;"); break;
        case '"': put("&quot;"); break;
        case '\'': put("&#39;"); break;
        default: putc(*s);
      }
    }
  }
  size_t length() const { return len_; }

 private:
  char *buf_;
  size_t cap_;
  size_t len_;
};

bool startsWith(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

}  // namespace

CredentialCheck validateCredentials(const char *ssid, const char *password) {
  if (ssid == nullptr || ssid[0] == 0) return CredentialCheck::EmptySsid;
  if (strlen(ssid) > kMaxSsidLen) return CredentialCheck::SsidTooLong;
  const size_t plen = password == nullptr ? 0 : strlen(password);
  if (plen != 0 && plen < kMinPassLen) return CredentialCheck::PasswordTooShort;
  if (plen > kMaxPassLen) return CredentialCheck::PasswordTooLong;
  return CredentialCheck::Ok;
}

const char *describeCheck(CredentialCheck check) {
  switch (check) {
    case CredentialCheck::Ok: return "ok";
    case CredentialCheck::EmptySsid: return "network name is empty";
    case CredentialCheck::SsidTooLong: return "network name is longer than 32 characters";
    case CredentialCheck::PasswordTooShort: return "password must be 8-63 characters (or empty for an open network)";
    case CredentialCheck::PasswordTooLong: return "password is longer than 63 characters";
  }
  return "invalid";
}

bool commitIfAssociated(CredentialStore &store, const Credentials &candidate, bool associated) {
  if (!associated) return false;
  if (validateCredentials(candidate.ssid, candidate.password) != CredentialCheck::Ok) return false;
  return store.save(candidate);
}

BootMode decideBootMode(bool hasStored, bool hasDeveloperFallback, bool recoveryRequested) {
  if (recoveryRequested) return BootMode::Provisioning;
  return (hasStored || hasDeveloperFallback) ? BootMode::Station : BootMode::Provisioning;
}

bool eventErasesCredentials(LinkEvent event) { return event == LinkEvent::UserRequestedForget; }

bool eventEntersProvisioning(LinkEvent event) { return event == LinkEvent::UserRequestedRecovery; }

bool HoldDetector::update(bool pressed, uint32_t nowMs) {
  if (!pressed) {
    down_ = false;
    fired_ = false;
    return false;
  }
  if (!down_) {
    down_ = true;
    fired_ = false;
    downAtMs_ = nowMs;
    return false;
  }
  if (!fired_ && static_cast<uint32_t>(nowMs - downAtMs_) >= holdMs_) {
    fired_ = true;
    return true;
  }
  return false;
}

size_t dedupeScan(ScanEntry *entries, size_t count) {
  size_t n = 0;
  for (size_t i = 0; i < count; ++i) {
    if (entries[i].ssid[0] == 0) continue;
    bool merged = false;
    for (size_t j = 0; j < n; ++j) {
      if (strcmp(entries[j].ssid, entries[i].ssid) == 0) {
        if (entries[i].rssi > entries[j].rssi) entries[j] = entries[i];
        merged = true;
        break;
      }
    }
    if (!merged) entries[n++] = entries[i];
  }
  for (size_t i = 1; i < n; ++i) {  // insertion sort, strongest first
    const ScanEntry key = entries[i];
    size_t j = i;
    while (j > 0 && entries[j - 1].rssi < key.rssi) {
      entries[j] = entries[j - 1];
      --j;
    }
    entries[j] = key;
  }
  return n;
}

size_t htmlEscape(char *out, size_t capacity, const char *in) {
  Out o(out, capacity);
  o.escaped(in);
  return o.length();
}

PortalRoute classifyPath(const char *path) {
  if (path == nullptr || path[0] == 0 || strcmp(path, "/") == 0) return PortalRoute::Root;
  if (strcmp(path, "/scan") == 0) return PortalRoute::Scan;
  if (strcmp(path, "/connect") == 0) return PortalRoute::Connect;
  if (strcmp(path, "/generate_204") == 0 || strcmp(path, "/gen_204") == 0 ||
      strcmp(path, "/hotspot-detect.html") == 0 || strcmp(path, "/library/test/success.html") == 0 ||
      strcmp(path, "/connecttest.txt") == 0 || strcmp(path, "/ncsi.txt") == 0 ||
      strcmp(path, "/redirect") == 0 || strcmp(path, "/fwlink") == 0 ||
      startsWith(path, "/fwlink")) {
    return PortalRoute::Probe;
  }
  return PortalRoute::Redirect;
}

size_t renderPortalPage(char *out, size_t capacity, const ScanEntry *entries, size_t count,
                        PortalStatus status, bool hasStored, const char *connectedSsid,
                        const char *connectedIp, bool adminConfigured) {
  Out o(out, capacity);
  o.put("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
  if (status == PortalStatus::Scanning || status == PortalStatus::Connecting) {
    o.put("<meta http-equiv=\"refresh\" content=\"2\">");
  }
  o.put("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Brooklyn Active Subway Map</title>"
        "<style>body{font-family:sans-serif;max-width:30em;margin:1em auto;padding:0 1em}"
        "input,select,button{font-size:1em;width:100%;margin:.3em 0;padding:.4em}"
        ".s{padding:.5em;background:#eee}</style></head><body>"
        "<h1>Brooklyn Active Subway Map</h1>"
        "<p class=\"s\"><b>Wi-Fi setup mode</b></p>"
        "<p>This map connects to <b>2.4 GHz</b> Wi-Fi only (5 GHz networks will not work).</p>");

  switch (status) {
    case PortalStatus::None: break;
    case PortalStatus::Scanning: o.put("<p class=\"s\">Scan in progress; reload in a moment.</p>"); break;
    case PortalStatus::Connecting: o.put("<p class=\"s\">Connecting...</p>"); break;
    case PortalStatus::Saved:
      o.put("<p class=\"s\">Successfully connected to Wi-Fi");
      if (connectedSsid != nullptr && connectedSsid[0] != 0) {
        o.put(": <b>");
        o.escaped(connectedSsid);
        o.put("</b>");
      }
      o.put(". Settings saved.");
      if (connectedIp != nullptr && connectedIp[0] != 0) {
        o.put(" LAN address: <a href=\"http://");
        o.escaped(connectedIp);
        o.put("/\">");
        o.escaped(connectedIp);
        o.put("</a> (open after reconnecting your phone to the same Wi-Fi).");
      }
      o.put(" To return to this setup page while setup Wi-Fi is "
            "available, go to <a href=\"http://192.168.4.1/\">192.168.4.1</a>. "
            "The setup network closes shortly.</p>");
      break;
    case PortalStatus::SaveFailed:
      o.put("<p class=\"s\">Successfully connected to Wi-Fi");
      if (connectedSsid != nullptr && connectedSsid[0] != 0) {
        o.put(": <b>");
        o.escaped(connectedSsid);
        o.put("</b>");
      }
      o.put(", but settings could not be saved. The previous saved settings are unchanged. ");
      if (connectedIp != nullptr && connectedIp[0] != 0) {
        o.put("LAN address: <a href=\"http://");
        o.escaped(connectedIp);
        o.put("/\">");
        o.escaped(connectedIp);
        o.put("</a> (open after reconnecting your phone to the same Wi-Fi). ");
      }
      o.put("To return to this setup page while setup Wi-Fi is available, go to "
            "<a href=\"http://192.168.4.1/\">192.168.4.1</a>.</p>");
      break;
    case PortalStatus::ConnectFailed:
      o.put("<p class=\"s\">Could not connect. Check the name and password"
            "; previously saved settings were not changed.</p>");
      break;
    case PortalStatus::InvalidInput:
      o.put("<p class=\"s\">Invalid network name or password.</p>");
      break;
  }
  if (status == PortalStatus::Saved) {
    // Provisioning is done: show only the result, not the setup forms.
    if (!adminConfigured) {
      o.put("<p>No administrator password is set, so the LAN page stays "
            "locked. Reconnect to the setup Wi-Fi at "
            "<a href=\"http://192.168.4.1/\">192.168.4.1</a> to set one.</p>");
    }
    o.put("</body></html>");
    return o.length();
  }
  if (hasStored) o.put("<p>Saved Wi-Fi settings exist and stay in place until a new network connects.</p>");

  o.put("<form method=\"POST\" action=\"/connect\"><label>Visible networks</label>"
        "<select name=\"pick\"><option value=\"\">(choose a network)</option>");
  for (size_t i = 0; i < count && i < kMaxScanEntries; ++i) {
    o.put("<option value=\"");
    o.escaped(entries[i].ssid);
    o.put("\">");
    o.escaped(entries[i].ssid);
    o.put("</option>");
  }
  o.put("</select>"
        "<label>Password (leave empty for an open network)</label>"
        "<input name=\"pass\" type=\"password\" maxlength=\"63\" autocomplete=\"off\" autocapitalize=\"off\" autocorrect=\"off\" spellcheck=\"false\">"
        "<button type=\"submit\">Save and connect</button></form>"
        "<section><h2>Administrator password</h2>");
  if (adminConfigured) {
    o.put("<p>An administrator password is configured. Setting a new one "
          "here resets normal-LAN administrator sessions.</p>");
  } else {
    o.put("<p>Set the initial administrator password to unlock normal-LAN "
          "configuration.</p>");
  }
  o.put("<form method=\"POST\" action=\"/admin/setup\">"
        "<label>New password (8 to 128 characters)</label>"
        "<input name=\"password\" type=\"password\" minlength=\"8\" "
        "maxlength=\"128\" autocomplete=\"new-password\" required>"
        "<label>Confirm password</label>"
        "<input name=\"confirmation\" type=\"password\" minlength=\"8\" "
        "maxlength=\"128\" autocomplete=\"new-password\" required>"
        "<button type=\"submit\">");
  o.put(adminConfigured ? "Reset administrator password"
                        : "Set administrator password");
  o.put("</button></form></section>"
        "<p><a href=\"/scan\">Rescan</a></p></body></html>");
  return o.length();
}

}  // namespace brooklyn::wifi
