// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "wifi_portal.h"

#if BROOKLYN_NETWORK_ENABLED

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <string.h>

namespace brooklyn::wifi {

namespace {

constexpr const char *kNs = "bkmap_wifi";
constexpr uint8_t kRecordVersion = 1;
const IPAddress kApIp(192, 168, 4, 1);

// Last STA disconnect reason from the Wi-Fi event task (0 = none pending).
// 15 = 4-way handshake timeout (usually wrong password), 201 = AP not found,
// 202 = auth failed, 205 = connection failed, 2 = auth expired.
volatile uint8_t gLastDisconnectReason = 0;
bool gEventHooked = false;

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  Serial.printf("Provisioning: wifi event %d\n", static_cast<int>(event));
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    gLastDisconnectReason = info.wifi_sta_disconnected.reason;
  }
}

}  // namespace

bool NvsCredentialStore::load(Credentials &out) {
  Preferences prefs;
  if (!prefs.begin(kNs, true)) return false;
  bool ok = false;
  if (prefs.getUChar("ver", 0) == kRecordVersion) {
    memset(&out, 0, sizeof(out));
    prefs.getString("ssid", out.ssid, sizeof(out.ssid));
    prefs.getString("pass", out.password, sizeof(out.password));
    ok = validateCredentials(out.ssid, out.password) == CredentialCheck::Ok;
  }
  prefs.end();
  return ok;
}

bool NvsCredentialStore::exists() {
  Credentials tmp;
  const bool ok = load(tmp);
  memset(&tmp, 0, sizeof(tmp));
  return ok;
}

bool NvsCredentialStore::save(const Credentials &in) {
  if (validateCredentials(in.ssid, in.password) != CredentialCheck::Ok) return false;
  Preferences prefs;
  if (!prefs.begin(kNs, false)) return false;
  const bool ssidOk = prefs.putString("ssid", in.ssid) > 0;
  prefs.putString("pass", in.password);  // returns 0 for an empty (open-network) password
  const bool ok = ssidOk && prefs.putUChar("ver", kRecordVersion) > 0;
  prefs.end();
  return ok;
}

bool NvsCredentialStore::clear() {
  Preferences prefs;
  if (!prefs.begin(kNs, false)) return false;
  const bool ok = prefs.clear();  // this namespace only
  prefs.end();
  return ok;
}

Portal::Portal(CredentialStore &store) : store_(store), server_(80) {}

bool Portal::begin(bool hasStored, uint32_t nowMs) {
  if (active_) return true;
  hasStored_ = hasStored;
  startedMs_ = nowMs;
  connecting_ = false;
  saved_ = false;
  lastStatus_ = PortalStatus::None;
  memset(&candidate_, 0, sizeof(candidate_));

  WiFi.persistent(false);  // the driver must not keep its own copy of credentials
  if (!gEventHooked) {
    WiFi.onEvent(onWifiEvent);
    gEventHooked = true;
  }
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  delay(200);
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(kApSsid)) {  // open AP; see docs/PHASE4L_WIFI_PROVISIONING.md
    WiFi.mode(WIFI_STA);
    return false;
  }
  delay(100);
  WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
  Serial.printf("Provisioning: AP IP %s channel %d\n", WiFi.softAPIP().toString().c_str(),
                static_cast<int>(WiFi.channel()));
  dns_.setErrorReplyCode(DNSReplyCode::NoError);
  dns_.start(53, "*", kApIp);

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/scan", HTTP_GET, [this]() { handleScan(); });
  server_.on("/connect", HTTP_POST, [this]() { handleConnect(); });
  server_.onNotFound([this]() { handleOther(); });
  server_.begin();

  scanCount_ = 0;
  delay(500);
  WiFi.scanNetworks(true);
  scanPending_ = true;
  active_ = true;
  return true;
}

void Portal::stop() {
  if (!active_) return;
  server_.stop();
  dns_.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  active_ = false;
  connecting_ = false;
}

void Portal::redirectHome() {
  server_.sendHeader("Location", "http://192.168.4.1/", true);
  server_.send(302, "text/plain", "");
}

void Portal::sendPage(PortalStatus status) {
  renderPortalPage(page_, sizeof(page_), scan_, scanCount_, status, hasStored_);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "text/html", page_);
}

void Portal::handleRoot() {
  if (scanCount_ == 0 && !scanPending_ && !connecting_ && !saved_) {
    WiFi.scanNetworks(true);
    scanPending_ = true;
  }
  PortalStatus s = lastStatus_;
  if (scanPending_ && s == PortalStatus::None) s = PortalStatus::Scanning;
  sendPage(s);
}

void Portal::handleScan() {
  if (!scanPending_) {
    WiFi.scanNetworks(true);
    scanPending_ = true;
  }
  redirectHome();
}

void Portal::handleOther() {
  // Probe URLs and unknown paths all lead back to the portal root.
  redirectHome();
}

void Portal::handleConnect() {
  char ssid[kMaxSsidLen + 2];
  char pass[kMaxPassLen + 2];
  const String chosen = server_.arg("pick");
  const String password = server_.arg("pass");
  strncpy(ssid, chosen.c_str(), sizeof(ssid) - 1);
  ssid[sizeof(ssid) - 1] = 0;
  strncpy(pass, password.c_str(), sizeof(pass) - 1);
  pass[sizeof(pass) - 1] = 0;

  if (connecting_ || saved_) {
    sendPage(connecting_ ? PortalStatus::Connecting : PortalStatus::Saved);
  } else if (validateCredentials(ssid, pass) != CredentialCheck::Ok) {
    lastStatus_ = PortalStatus::InvalidInput;
    sendPage(lastStatus_);
  } else {
    memset(&candidate_, 0, sizeof(candidate_));
    strncpy(candidate_.ssid, ssid, kMaxSsidLen);
    strncpy(candidate_.password, pass, kMaxPassLen);
    connecting_ = true;
    connectStartMs_ = millis();
    lastStatus_ = PortalStatus::Connecting;
    Serial.printf("Provisioning: trying new network (SSID length %u, password length %u)\n",
                  static_cast<unsigned>(strlen(candidate_.ssid)),
                  static_cast<unsigned>(strlen(candidate_.password)));
    gLastDisconnectReason = 0;
    // An in-flight scan keeps the STA idle and WiFi.begin() silently does nothing.
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    scanPending_ = false;
    WiFi.disconnect(false);
    delay(100);
    Serial.printf("Provisioning: mode before begin = %d\n", static_cast<int>(WiFi.getMode()));
    WiFi.mode(WIFI_AP_STA);
    const wl_status_t beginStatus = WiFi.begin(candidate_.ssid, candidate_.password);
    Serial.printf("Provisioning: WiFi.begin returned %d, mode = %d\n",
                  static_cast<int>(beginStatus), static_cast<int>(WiFi.getMode()));
    sendPage(PortalStatus::Connecting);
  }
  memset(pass, 0, sizeof(pass));
}

void Portal::harvestScan() {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanPending_ = false;
  scanCount_ = 0;
  Serial.printf("Provisioning: scan found %d network(s)\n", n);
  if (n > 0) {
    for (int i = 0; i < n && scanCount_ < kMaxScanEntries; ++i) {
      ScanEntry &e = scan_[scanCount_++];
      memset(&e, 0, sizeof(e));
      strncpy(e.ssid, WiFi.SSID(i).c_str(), kMaxSsidLen);
      e.rssi = static_cast<int16_t>(WiFi.RSSI(i));
      e.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      Serial.printf("Provisioning:   [%d] \"%s\" ch %d rssi %d auth %d\n", i, e.ssid,
                    static_cast<int>(WiFi.channel(i)), static_cast<int>(e.rssi),
                    static_cast<int>(WiFi.encryptionType(i)));
    }
    scanCount_ = dedupeScan(scan_, scanCount_);
  }
  WiFi.scanDelete();
  if (lastStatus_ == PortalStatus::Scanning) lastStatus_ = PortalStatus::None;
}

Portal::Result Portal::service(uint32_t nowMs) {
  if (!active_) return Result::TimedOut;
  dns_.processNextRequest();
  static int lastClients = -1;
  const int clients = WiFi.softAPgetStationNum();
  if (clients != lastClients) {
    lastClients = clients;
    Serial.printf("Provisioning: AP clients = %d\n", clients);
  }
  server_.handleClient();
  if (scanPending_ && !connecting_) harvestScan();

  if (connecting_) {
    const uint8_t reason = gLastDisconnectReason;
    if (reason != 0) {
      gLastDisconnectReason = 0;
      Serial.printf("Provisioning: STA disconnect reason %u, WiFi.status() = %d\n",
                    static_cast<unsigned>(reason), static_cast<int>(WiFi.status()));
    }
    if (WiFi.status() == WL_CONNECTED) {
      connecting_ = false;
      if (commitIfAssociated(store_, candidate_, true)) {
        saved_ = true;
        savedAtMs_ = nowMs;
        lastStatus_ = PortalStatus::Saved;
        Serial.println("Provisioning: association OK; credentials saved");
      } else {
        lastStatus_ = PortalStatus::ConnectFailed;
        Serial.println("Provisioning: association OK but save failed; old credentials kept");
      }
    } else if (static_cast<uint32_t>(nowMs - connectStartMs_) >= kConnectAttemptMs) {
      connecting_ = false;
      // 1=NO_SSID_AVAIL (not seen / 5 GHz), 4=CONNECT_FAILED (usually wrong password), 6=DISCONNECTED
      Serial.printf("Provisioning: WiFi.status() = %d\n", static_cast<int>(WiFi.status()));
      WiFi.disconnect(false);
      lastStatus_ = PortalStatus::ConnectFailed;
      Serial.println("Provisioning: association FAILED; stored credentials unchanged");
    }
  }

  if (saved_ && static_cast<uint32_t>(nowMs - savedAtMs_) >= kSavedLingerMs) {
    stop();
    return Result::Connected;
  }
  if (hasStored_ && !connecting_ && !saved_ &&
      static_cast<uint32_t>(nowMs - startedMs_) >= kStoredPortalTimeoutMs) {
    stop();
    return Result::TimedOut;
  }
  return Result::Active;
}

}  // namespace brooklyn::wifi

#endif  // BROOKLYN_NETWORK_ENABLED
