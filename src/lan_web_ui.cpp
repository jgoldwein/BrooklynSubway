// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_ui.h"

#if BROOKLYN_NETWORK_ENABLED

#include "ota_upload_gate.h"

#include <Arduino.h>

#include <stdio.h>
#include <string.h>

namespace brooklyn::web {
namespace {

constexpr size_t kPageCapacity = 12288;
constexpr size_t kJsonCapacity = 2048;
constexpr size_t kNetworkDiagnosticsJsonCapacity = 1536;
char gPage[kPageCapacity];
char gJson[kJsonCapacity];
char gNetworkDiagnosticsJson[kNetworkDiagnosticsJsonCapacity];

constexpr const char *kSetupStarting =
    "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta http-equiv=\"refresh\" content=\"3;url=http://192.168.4.1/\">"
    "<title>Wi-Fi setup starting</title></head><body>"
    "<h1>Wi-Fi setup starting</h1><p>Join BrooklynMap-Avi, then open "
    "http://192.168.4.1/ if the setup page does not appear.</p></body></html>";

constexpr const char *kRebooting =
    "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<title>Rebooting</title></head><body><h1>Rebooting</h1>"
    "<p>Configuration and Wi-Fi credentials are retained.</p></body></html>";

void eraseString(String &value) {
  for (size_t i = 0; i < value.length(); ++i) value.setCharAt(i, '\0');
  value.clear();
}

void eraseBytes(void *memory, size_t length) {
  volatile uint8_t *bytes = static_cast<volatile uint8_t *>(memory);
  while (length-- != 0) *bytes++ = 0;
}

bool equalStringConstantTime(const String &left, const String &right) {
  const size_t leftLength = left.length();
  const size_t rightLength = right.length();
  const size_t length = leftLength > rightLength ? leftLength : rightLength;
  uint8_t difference = static_cast<uint8_t>(leftLength ^ rightLength);
  for (size_t i = 0; i < length; ++i) {
    const uint8_t leftByte =
        i < leftLength ? static_cast<uint8_t>(left[i]) : 0;
    const uint8_t rightByte =
        i < rightLength ? static_cast<uint8_t>(right[i]) : 0;
    difference |= static_cast<uint8_t>(leftByte ^ rightByte);
  }
  return difference == 0;
}

bool hasNoEmbeddedNull(const String &value) {
  return strlen(value.c_str()) == value.length();
}

}  // namespace

LanWebUi::LanWebUi(config::DeviceConfiguration &configuration)
    : configuration_(configuration),
      auth_(admin::nvsCredentialStore(), admin::espCryptoProvider()),
      server_(80) {}

bool LanWebUi::begin() {
  if (active_) return true;
  if (!auth_.load()) {
    Serial.println("LAN UI: ERROR - administrator credential storage unavailable");
    return false;
  }
  if (!routesRegistered_) {
    registerRoutes();
    routesRegistered_ = true;
  }
  server_.begin();
  active_ = true;
  Serial.printf(
      "LAN UI: HTTP server started on station interface; administrator %s; "
      "read-only status is public; HTTP is unencrypted\n",
      auth_.configured() ? "configured" : "setup required locally");
  return true;
}

void LanWebUi::registerRoutes() {
  const char *headers[] = {"Cookie", "X-CSRF-Token", "Content-Type"};
  server_.collectHeaders(headers, 3);
  server_.on("/update", HTTP_GET, [this]() { handleUpdatePage(); });
  server_.on("/update", HTTP_POST, [this]() { handleUpdateComplete(); },
             [this]() { handleUpdateRaw(); });
  server_.on("/", HTTP_GET, [this]() { handleHome(); });
  server_.on("/login", HTTP_GET, [this]() { handleLoginPage(); });
  server_.on("/login", HTTP_POST, [this]() { handleLogin(); });
  server_.on("/logout", HTTP_POST, [this]() { handleLogout(); });
  server_.on("/admin/password", HTTP_POST,
             [this]() { handlePasswordChange(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/network-diagnostics", HTTP_GET,
             [this]() { handleNetworkDiagnostics(); });
  server_.on("/network-diagnostics/run", HTTP_POST,
             [this]() { handleRunNetworkDiagnostics(); });
  server_.on("/config/brightness", HTTP_POST,
             [this]() { handleBrightness(); });
  server_.on("/config/dns", HTTP_POST, [this]() { handleDnsSettings(); });
  server_.on("/config/routes", HTTP_POST, [this]() { handleRoutes(); });
  server_.on("/wifi/setup", HTTP_GET,
             [this]() { handleSetupConfirmation(); });
  server_.on("/wifi/setup", HTTP_POST,
             [this]() { handleSetupConfirmation(); });
  server_.on("/wifi/setup/confirm", HTTP_POST,
             [this]() { handleSetupRequest(); });
  server_.on("/reboot", HTTP_POST, [this]() { handleReboot(); });
  server_.onNotFound([this]() { server_.send(404, "text/plain", "Not found"); });
}

void LanWebUi::stop() {
  if (!active_) return;
  server_.stop();
  active_ = false;
  Serial.println("LAN UI: HTTP server stopped");
}

void LanWebUi::service(const StatusSnapshot &status) {
  if (!active_) return;
  status_ = status;
  server_.handleClient();
}

void LanWebUi::sendHtml(const char *body, size_t length) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("X-Content-Type-Options", "nosniff");
  server_.sendHeader("X-Frame-Options", "DENY");
  server_.sendHeader(
      "Content-Security-Policy",
      "frame-ancestors 'none'; object-src 'none'; base-uri 'none'");
  server_.sendHeader("Referrer-Policy", "same-origin");
  server_.setContentLength(length);
  server_.send(200, "text/html; charset=utf-8", "");
  server_.sendContent(body, length);
}

void LanWebUi::handleHome() {
  StatusSnapshot pageStatus = status_;
  updateAdminStatus(pageStatus);
  char csrfToken[admin::kTokenSize * 2 + 1] = {};
  if (pageStatus.adminSessionActive) {
    if (!requestSession(csrfToken, sizeof(csrfToken))) {
      pageStatus.adminSessionActive = false;
    }
  }
  const size_t length = renderHomePage(
      gPage, sizeof(gPage), pageStatus, configuration_.brightness(),
      configuration_.routeVisibilityMask(), csrfToken);
  eraseBytes(csrfToken, sizeof(csrfToken));
  if (length == 0) {
    server_.send(500, "text/plain", "Unable to render status page");
    return;
  }
  sendHtml(gPage, length);
}

void LanWebUi::handleStatus() {
  StatusSnapshot publicStatus = status_;
  updateAdminStatus(publicStatus);
  const size_t length = renderStatusJson(
      gJson, sizeof(gJson), publicStatus, configuration_.brightness(),
      configuration_.routeVisibilityMask());
  if (length == 0) {
    server_.send(500, "text/plain", "Unable to render status");
    return;
  }
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("X-Content-Type-Options", "nosniff");
  server_.send(200, "application/json", gJson);
}

void LanWebUi::handleNetworkDiagnostics() {
  const size_t length = renderNetworkDiagnosticsJson(
      gNetworkDiagnosticsJson, sizeof(gNetworkDiagnosticsJson),
      status_.networkDiagnostics);
  if (length == 0) {
    server_.send(500, "text/plain", "Unable to render network diagnostics");
    return;
  }
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("X-Content-Type-Options", "nosniff");
  server_.send(200, "application/json", gNetworkDiagnosticsJson);
}

void LanWebUi::handleRunNetworkDiagnostics() {
  if (!authorizeRequest(true)) return;
  if (server_.args() != 1) {
    server_.send(400, "text/plain", "Invalid diagnostics request");
    return;
  }
  if (status_.networkDiagnostics.state == network::diagnostics::RunState::Running ||
      networkDiagnosticsRequested_) {
    server_.send(409, "application/json", "{\"state\":\"running\"}");
    return;
  }
  networkDiagnosticsRequested_ = true;
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(202, "application/json", "{\"state\":\"accepted\"}");
}

void LanWebUi::handleBrightness() {
  if (!authorizeRequest(true)) return;
  bool brightnessSeen = false;
  bool csrfSeen = false;
  for (int i = 0; i < server_.args(); ++i) {
    const String name = server_.argName(i);
    if (name == "brightness" && !brightnessSeen) brightnessSeen = true;
    else if (name == "csrf" && !csrfSeen) csrfSeen = true;
    else {
      server_.send(400, "text/plain", "Invalid brightness settings");
      return;
    }
  }
  if (server_.args() != 2 || !brightnessSeen || !csrfSeen) {
    server_.send(400, "text/plain", "Invalid brightness settings");
    return;
  }
  if (!server_.hasArg("brightness")) {
    server_.send(400, "text/plain", "Brightness is required");
    return;
  }
  const String value = server_.arg("brightness");
  char input[4] = {};
  if (value.length() == 0 || value.length() >= sizeof(input)) {
    server_.send(400, "text/plain", "Brightness must be an integer from 0 to 255");
    return;
  }
  value.toCharArray(input, sizeof(input));
  uint8_t brightness = 0;
  if (!parseBrightness(input, brightness)) {
    server_.send(400, "text/plain", "Brightness must be an integer from 0 to 255");
    return;
  }
  if (!configuration_.setBrightness(brightness)) {
    server_.send(500, "text/plain", "Unable to save brightness");
    return;
  }
  server_.sendHeader("Location", "/", true);
  server_.send(303, "text/plain", "");
}

void LanWebUi::handleDnsSettings() {
  if (!authorizeRequest(true)) return;
  constexpr size_t kMaxParameters = 3;
  constexpr size_t kNameCapacity = 24;
  constexpr size_t kValueCapacity = 16;
  const int argumentCount = server_.args();
  if (argumentCount < 3 ||
      static_cast<size_t>(argumentCount) > kMaxParameters + 1) {
    server_.send(400, "text/plain", "Invalid DNS settings");
    return;
  }

  char names[kMaxParameters][kNameCapacity]{};
  char values[kMaxParameters][kValueCapacity]{};
  DnsSettingsParam parameters[kMaxParameters]{};
  size_t parameterCount = 0;
  for (int i = 0; i < argumentCount; ++i) {
    const String name = server_.argName(i);
    if (name == "csrf") continue;
    const String value = server_.arg(i);
    if (parameterCount >= kMaxParameters ||
        name.length() >= kNameCapacity || value.length() >= kValueCapacity) {
      server_.send(400, "text/plain", "Invalid DNS settings");
      return;
    }
    name.toCharArray(names[parameterCount], kNameCapacity);
    value.toCharArray(values[parameterCount], kValueCapacity);
    parameters[parameterCount] = {names[parameterCount], values[parameterCount]};
    ++parameterCount;
  }

  bool enabled = false;
  uint32_t ipv4 = 0;
  if (!parseDnsSettings(parameters, parameterCount,
                        enabled, ipv4)) {
    server_.send(400, "text/plain",
                 "Enter a valid unicast IPv4 DNS server address");
    return;
  }
  if (!configuration_.setDnsOverride(enabled, ipv4)) {
    server_.send(500, "text/plain", "Unable to save DNS settings");
    return;
  }
  char address[16] = {};
  config::formatDnsIpv4(ipv4, address, sizeof(address));
  Serial.printf("LAN UI: manual DNS %s%s%s%s\n",
                enabled ? "enabled" : "disabled", ipv4 != 0 ? " (" : "",
                ipv4 != 0 ? address : "", ipv4 != 0 ? ")" : "");
  server_.sendHeader("Location", "/", true);
  server_.send(303, "text/plain", "");
}

void LanWebUi::handleRoutes() {
  if (!authorizeRequest(true)) return;
  constexpr size_t kMaxRouteParameters = semantic::kRouteCount + 1;
  constexpr size_t kNameCapacity = 16;
  constexpr size_t kValueCapacity = 8;
  const int argumentCount = server_.args();
  if (argumentCount < 2 ||
      static_cast<size_t>(argumentCount) > kMaxRouteParameters + 1) {
    server_.send(400, "text/plain", "Invalid route selection");
    return;
  }

  char names[kMaxRouteParameters][kNameCapacity]{};
  char values[kMaxRouteParameters][kValueCapacity]{};
  RouteSelectionParam parameters[kMaxRouteParameters]{};
  size_t parameterCount = 0;
  for (int i = 0; i < argumentCount; ++i) {
    const String name = server_.argName(i);
    if (name == "csrf") continue;
    const String value = server_.arg(i);
    if (parameterCount >= kMaxRouteParameters ||
        name.length() >= kNameCapacity || value.length() >= kValueCapacity) {
      server_.send(400, "text/plain", "Invalid route selection");
      return;
    }
    name.toCharArray(names[parameterCount], kNameCapacity);
    value.toCharArray(values[parameterCount], kValueCapacity);
    parameters[parameterCount] = {names[parameterCount], values[parameterCount]};
    ++parameterCount;
  }

  uint32_t visibilityMask = 0;
  if (!parseRouteSelection(parameters, parameterCount,
                           visibilityMask)) {
    server_.send(400, "text/plain", "Invalid route selection");
    return;
  }
  if (!configuration_.setRouteVisibilityMask(visibilityMask)) {
    server_.send(500, "text/plain", "Unable to save route selection");
    return;
  }
  Serial.printf("LAN UI: route visibility saved (mask 0x%05lx)\n",
                static_cast<unsigned long>(visibilityMask));
  server_.sendHeader("Location", "/", true);
  server_.send(303, "text/plain", "");
}

void LanWebUi::handleSetupConfirmation() {
  if (!authorizeRequest(server_.method() == HTTP_POST)) return;
  if ((server_.method() == HTTP_POST && server_.args() != 1) ||
      (server_.method() == HTTP_GET && server_.args() != 0)) {
    server_.send(400, "text/plain", "Invalid Wi-Fi setup request");
    return;
  }
  char csrfToken[admin::kTokenSize * 2 + 1] = {};
  if (!requestSession(csrfToken, sizeof(csrfToken))) {
    server_.send(401, "text/plain", "Administrator sign in is required");
    return;
  }
  const int written = snprintf(
      gPage, sizeof(gPage),
      "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Change Wi-Fi</title></head><body><h1>Change Wi-Fi</h1>"
      "<p>The setup network will start; existing credentials are retained. "
      "A replacement is saved only after it successfully connects.</p>"
      "<form method=\"post\" action=\"/wifi/setup/confirm\">"
      "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
      "<button type=\"submit\">Start BrooklynMap-Avi setup</button></form>"
      "<p><a href=\"/\">Cancel</a></p></body></html>",
      csrfToken);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(gPage)) {
    eraseBytes(csrfToken, sizeof(csrfToken));
    server_.send(500, "text/plain", "Unable to render setup confirmation");
    return;
  }
  eraseBytes(csrfToken, sizeof(csrfToken));
  sendHtml(gPage, static_cast<size_t>(written));
}

void LanWebUi::handleSetupRequest() {
  if (!authorizeRequest(true)) return;
  if (server_.args() != 1) {
    server_.send(400, "text/plain", "Invalid Wi-Fi setup request");
    return;
  }
  setupRequested_ = true;
  sendHtml(kSetupStarting, strlen(kSetupStarting));
}

void LanWebUi::handleReboot() {
  if (!authorizeRequest(true)) return;
  if (server_.args() != 1) {
    server_.send(400, "text/plain", "Invalid reboot request");
    return;
  }
  rebootRequested_ = true;
  sendHtml(kRebooting, strlen(kRebooting));
}

void LanWebUi::handleLoginPage() {
  if (!auth_.configured()) {
    server_.send(403, "text/plain",
                 "Administrator setup is available only through local provisioning");
    return;
  }
  String existingCookie = server_.header("Cookie");
  if (auth_.hasSession(existingCookie.c_str(), millis())) {
    eraseString(existingCookie);
    sendLoginRedirect();
    return;
  }
  eraseString(existingCookie);
  char cookieToken[admin::kTokenSize * 2 + 1] = {};
  char csrfToken[admin::kTokenSize * 2 + 1] = {};
  if (!auth_.createLoginCsrf(cookieToken, sizeof(cookieToken), csrfToken,
                             sizeof(csrfToken), millis())) {
    server_.send(500, "text/plain", "Unable to initialize sign-in");
    return;
  }
  const size_t length =
      renderLoginPage(gPage, sizeof(gPage), csrfToken);
  if (length == 0) {
    eraseBytes(cookieToken, sizeof(cookieToken));
    eraseBytes(csrfToken, sizeof(csrfToken));
    server_.send(500, "text/plain", "Unable to render sign-in page");
    return;
  }
  char cookie[128] = {};
  const int written =
      snprintf(cookie, sizeof(cookie),
               "%s=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=300",
               admin::kLoginCookieName, cookieToken);
  eraseBytes(cookieToken, sizeof(cookieToken));
  eraseBytes(csrfToken, sizeof(csrfToken));
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(cookie)) {
    eraseBytes(cookie, sizeof(cookie));
    server_.send(500, "text/plain", "Unable to initialize sign-in");
    return;
  }
  server_.sendHeader("Set-Cookie", cookie);
  eraseBytes(cookie, sizeof(cookie));
  sendHtml(gPage, length);
}

void LanWebUi::handleLogin() {
  server_.sendHeader("Cache-Control", "no-store");
  if (!admin::isMutationEndpoint("POST", "/login")) {
    server_.send(500, "text/plain", "Sign-in authorization policy mismatch");
    return;
  }
  if (!auth_.configured()) {
    server_.send(403, "text/plain",
                 "Administrator setup is available only through local provisioning");
    return;
  }
  int csrfCount = 0;
  bool hasPassword = false;
  for (int i = 0; i < server_.args(); ++i) {
    const String name = server_.argName(i);
    if (name == "csrf") ++csrfCount;
    else if (name == "password") hasPassword = true;
    else {
      server_.send(400, "text/plain", "Invalid sign-in request");
      return;
    }
  }
  if (csrfCount != 1 || !hasPassword || server_.args() != 2) {
    server_.send(400, "text/plain", "Invalid sign-in request");
    return;
  }
  String cookieHeader = server_.header("Cookie");
  String csrf = server_.arg("csrf");
  const bool csrfValid =
      hasNoEmbeddedNull(csrf) &&
      auth_.authorizeLogin(cookieHeader.c_str(), csrf.c_str(), millis());
  eraseString(cookieHeader);
  if (!csrfValid) {
    eraseString(csrf);
    server_.send(403, "text/plain", "Sign-in request expired; reload the page");
    return;
  }
  eraseString(csrf);

  String password = server_.arg("password");
  char passwordBuffer[admin::kPasswordMaxLength + 2] = {};
  if (hasNoEmbeddedNull(password)) {
    password.toCharArray(passwordBuffer, sizeof(passwordBuffer));
  } else {
    memset(passwordBuffer, 'x', sizeof(passwordBuffer) - 1);
  }
  eraseString(password);
  admin::LoginResult result = auth_.login(passwordBuffer, millis());
  eraseBytes(passwordBuffer, sizeof(passwordBuffer));
  if (result.code != admin::LoginResult::Code::Accepted) {
    eraseBytes(result.cookieToken, sizeof(result.cookieToken));
    sendLoginFailure(result.code);
    return;
  }

  char cookie[128] = {};
  const int written =
      snprintf(cookie, sizeof(cookie),
               "%s=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=1800",
               admin::kSessionCookieName, result.cookieToken);
  eraseBytes(result.cookieToken, sizeof(result.cookieToken));
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(cookie)) {
    eraseBytes(cookie, sizeof(cookie));
    server_.send(500, "text/plain", "Unable to create administrator session");
    return;
  }
  server_.sendHeader("Set-Cookie", cookie);
  eraseBytes(cookie, sizeof(cookie));
  sendLoginRedirect();
}

void LanWebUi::handleLogout() {
  if (!authorizeRequest(true)) return;
  if (server_.args() != 1) {
    server_.send(400, "text/plain", "Invalid sign-out request");
    return;
  }
  String cookieHeader = server_.header("Cookie");
  String csrf = server_.arg("csrf");
  const bool loggedOut =
      auth_.logout(cookieHeader.c_str(), csrf.c_str(), millis());
  eraseString(cookieHeader);
  if (!loggedOut) {
    eraseString(csrf);
    server_.send(403, "text/plain", "Invalid sign-out request");
    return;
  }
  eraseString(csrf);
  server_.sendHeader(
      "Set-Cookie",
      "bkmap_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  sendLoginRedirect();
}

void LanWebUi::handlePasswordChange() {
  if (!authorizeRequest(true)) return;
  if (server_.args() != 3) {
    server_.send(400, "text/plain", "New password and confirmation are required");
    return;
  }
  bool passwordSeen = false;
  bool confirmationSeen = false;
  for (int i = 0; i < server_.args(); ++i) {
    const String name = server_.argName(i);
    if (name == "csrf") continue;
    if (name == "password" && !passwordSeen) passwordSeen = true;
    else if (name == "confirmation" && !confirmationSeen) {
      confirmationSeen = true;
    } else {
      server_.send(400, "text/plain", "Invalid password change request");
      return;
    }
  }
  if (!passwordSeen || !confirmationSeen) {
    server_.send(400, "text/plain", "Invalid password change request");
    return;
  }
  String password = server_.arg("password");
  String confirmation = server_.arg("confirmation");
  if (password.length() < admin::kPasswordMinLength ||
      password.length() > admin::kPasswordMaxLength ||
      confirmation.length() > admin::kPasswordMaxLength ||
      !hasNoEmbeddedNull(password) || !hasNoEmbeddedNull(confirmation) ||
      !equalStringConstantTime(password, confirmation)) {
    eraseString(password);
    eraseString(confirmation);
    server_.send(400, "text/plain",
                 "Passwords must match and be 8 to 128 characters");
    return;
  }
  char passwordBuffer[admin::kPasswordMaxLength + 1] = {};
  char confirmationBuffer[admin::kPasswordMaxLength + 1] = {};
  password.toCharArray(passwordBuffer, sizeof(passwordBuffer));
  confirmation.toCharArray(confirmationBuffer, sizeof(confirmationBuffer));
  eraseString(password);
  eraseString(confirmation);
  const bool saved = auth_.setPassword(passwordBuffer, confirmationBuffer);
  eraseBytes(passwordBuffer, sizeof(passwordBuffer));
  eraseBytes(confirmationBuffer, sizeof(confirmationBuffer));
  if (!saved) {
    server_.send(500, "text/plain", "Unable to save administrator password");
    return;
  }
  server_.sendHeader(
      "Set-Cookie",
      "bkmap_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  sendLoginRedirect();
}

bool LanWebUi::updateContentTypeAccepted() {
  const String type = server_.header("Content-Type");
  return type.startsWith("application/octet-stream");
}

// Authorization for the streamed upload: session cookie plus CSRF token in
// the X-CSRF-Token header, evaluated before any body byte is consumed.
int LanWebUi::updateAuthorizationStatus() {
  String cookieHeader = server_.header("Cookie");
  String csrf = server_.header("X-CSRF-Token");
  int status = 403;
  if (hasNoEmbeddedNull(csrf) && hasNoEmbeddedNull(cookieHeader)) {
    status = ota::authorizeUpload(auth_, cookieHeader.c_str(), csrf.c_str(),
                                  millis());
  }
  eraseString(csrf);
  eraseString(cookieHeader);
  return status;
}

void LanWebUi::handleUpdatePage() {
  if (!authorizeRequest(false)) return;
  char csrf[96];
  if (!requestSession(csrf, sizeof(csrf))) {
    server_.send(401, "text/plain", "Administrator sign-in is required");
    return;
  }
  for (const char *c = csrf; *c != '\0'; ++c) {
    const bool safe = (*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
                      (*c >= 'A' && *c <= 'Z') || *c == '-' || *c == '_';
    if (!safe) {
      server_.send(500, "text/plain", "Unsupported session token");
      return;
    }
  }
  const int length = snprintf(
      gPage, kPageCapacity,
      "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Update firmware</title><style>"
      "body{font:16px system-ui,sans-serif;max-width:40rem;margin:1rem auto;"
      "padding:0 1rem}.warn{padding:.7rem;background:#fff4d6}"
      "button,input{font:inherit;padding:.5rem}</style></head><body>"
      "<h1>Update firmware</h1>"
      "<p>Running: %s | Target: %s</p>"
      "<p class=\"warn\">Upload a signed <b>.bkota</b> package built for this "
      "target. Unsigned or wrong-target packages are rejected. HTTP is not "
      "encrypted; use only a trusted network. Older signed versions are "
      "accepted (downgrade). Configuration and Wi-Fi credentials are kept. "
      "The device reboots after a verified update.</p>"
      "<input type=\"file\" id=\"f\" accept=\".bkota\"> "
      "<button id=\"b\">Upload and install</button>"
      "<p id=\"s\" role=\"status\"></p><p><a href=\"/\">Back</a></p>"
      "<script>var t=\"%s\";document.getElementById(\"b\").onclick=function(){"
      "var f=document.getElementById(\"f\").files[0],s=document.getElementById"
      "(\"s\");if(!f){s.textContent=\"Choose a package first.\";return}"
      "var x=new XMLHttpRequest();x.open(\"POST\",\"/update\");"
      "x.setRequestHeader(\"X-CSRF-Token\",t);x.setRequestHeader("
      "\"Content-Type\",\"application/octet-stream\");x.upload.onprogress="
      "function(e){s.textContent=\"Uploading \"+Math.round(100*e.loaded/"
      "e.total)+\"%%\"};x.onload=function(){s.textContent=x.status+\": \"+"
      "x.responseText};x.onerror=function(){s.textContent=\"Connection lost"
      " (the device may be rebooting).\"};x.send(f)}</script></body></html>",
      web::kFirmwareId, ota::WebOtaSession::targetName(), csrf);
  eraseBytes(csrf, sizeof(csrf));
  if (length <= 0 || static_cast<size_t>(length) >= kPageCapacity) {
    server_.send(500, "text/plain", "Page too large");
    return;
  }
  sendHtml(gPage, static_cast<size_t>(length));
}

// Streaming callback. Never sends a response; handleUpdateComplete() does.
void LanWebUi::handleUpdateRaw() {
  if (server_.uri() != "/update" || !updateContentTypeAccepted()) return;
  HTTPRaw &raw = server_.raw();
  switch (raw.status) {
    case RAW_START: {
      updateStarted_ = true;
      updateDeniedStatus_ = updateAuthorizationStatus();
      updateResult_ = ota::Result::NotStarted;
      if (updateDeniedStatus_ != 0) {
        Serial.printf("OTA: upload denied (HTTP %d)\n", updateDeniedStatus_);
        break;
      }
      const int length = server_.clientContentLength();
      updateResult_ =
          otaSession_.start(length > 0 ? static_cast<size_t>(length) : 0);
      break;
    }
    case RAW_WRITE:
      if (updateStarted_ && updateDeniedStatus_ == 0 &&
          updateResult_ == ota::Result::Ok) {
        updateResult_ = otaSession_.feed(raw.buf, raw.currentSize);
      }
      break;
    case RAW_END:
      if (updateStarted_ && updateDeniedStatus_ == 0 &&
          updateResult_ == ota::Result::Ok) {
        updateResult_ = otaSession_.finish();
      }
      break;
    case RAW_ABORTED:
      otaSession_.abort();
      updateStarted_ = false;
      break;
  }
}

void LanWebUi::handleUpdateComplete() {
  server_.sendHeader("Cache-Control", "no-store");
  const bool started = updateStarted_;
  const int denied = updateDeniedStatus_;
  const ota::Result result = updateResult_;
  updateStarted_ = false;
  updateDeniedStatus_ = 0;
  updateResult_ = ota::Result::NotStarted;
  if (!updateContentTypeAccepted() || !started) {
    server_.send(415, "text/plain",
                 "Send the package as application/octet-stream");
    return;
  }
  if (denied == 401) {
    server_.send(401, "text/plain", "Administrator sign-in is required");
    return;
  }
  if (denied != 0) {
    server_.send(denied, "text/plain", "A valid CSRF token is required");
    return;
  }
  if (result != ota::Result::Ok) {
    server_.send(ota::resultHttpStatus(result), "text/plain",
                 ota::resultName(result));
    return;
  }
  const ota::Installer *installer = otaSession_.installer();
  const bool downgrade = installer != nullptr && installer->downgrade();
  const int length = snprintf(
      gPage, kPageCapacity,
      "Firmware %s verified and installed. Rebooting.%s",
      installer != nullptr ? installer->header().version : "?",
      downgrade ? " WARNING: this is an older version (downgrade)." : "");
  rebootRequested_ = true;
  server_.send(200, "text/plain", length > 0 ? gPage : "Installed. Rebooting.");
}

bool LanWebUi::authorizeRequest(bool requireCsrf) {
  server_.sendHeader("Cache-Control", "no-store");
  const String uri = server_.uri();
  const char *method = server_.method() == HTTP_GET ? "GET" : "POST";
  if (!admin::isProtectedEndpoint(method, uri.c_str())) {
    server_.send(500, "text/plain", "Authorization policy mismatch");
    return false;
  }
  if (!auth_.configured()) {
    server_.send(403, "text/plain",
                 "Administrator setup is available only through local provisioning");
    return false;
  }
  String cookieHeader = server_.header("Cookie");
  const bool sessionActive =
      auth_.hasSession(cookieHeader.c_str(), millis());
  if (!sessionActive) {
    eraseString(cookieHeader);
    server_.send(401, "text/plain", "Administrator sign-in is required");
    return false;
  }
  if (requireCsrf) {
    int csrfCount = 0;
    for (int i = 0; i < server_.args(); ++i) {
      if (server_.argName(i) == "csrf") ++csrfCount;
    }
    if (csrfCount != 1) {
      eraseString(cookieHeader);
      server_.send(403, "text/plain", "A valid CSRF token is required");
      return false;
    }
    String csrf = server_.arg("csrf");
    const bool csrfValid =
        hasNoEmbeddedNull(csrf) &&
        auth_.authorizePost(cookieHeader.c_str(), csrf.c_str(), millis());
    eraseString(csrf);
    eraseString(cookieHeader);
    if (!csrfValid) {
      server_.send(403, "text/plain", "A valid CSRF token is required");
      return false;
    }
  } else {
    eraseString(cookieHeader);
  }
  return true;
}

bool LanWebUi::requestSession(char *csrfToken, size_t capacity) {
  if (csrfToken == nullptr || capacity == 0) return false;
  csrfToken[0] = '\0';
  String cookieHeader = server_.header("Cookie");
  const bool success =
      auth_.csrfForSession(cookieHeader.c_str(), millis(), csrfToken, capacity);
  eraseString(cookieHeader);
  return success;
}

void LanWebUi::sendLoginRedirect() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("Location", "/", true);
  server_.send(303, "text/plain", "");
}

void LanWebUi::sendLoginFailure(admin::LoginResult::Code code) {
  server_.sendHeader("Cache-Control", "no-store");
  switch (code) {
    case admin::LoginResult::Code::LockedOut:
      server_.sendHeader("Retry-After", "30");
      server_.send(429, "text/plain",
                   "Too many sign-in attempts; try again later");
      break;
    case admin::LoginResult::Code::NotConfigured:
      server_.send(403, "text/plain",
                   "Administrator setup is available only through local provisioning");
      break;
    case admin::LoginResult::Code::CryptoFailure:
      Serial.println("LAN UI: ERROR - administrator password verification failed");
      server_.send(500, "text/plain", "Unable to verify administrator password");
      break;
    case admin::LoginResult::Code::InvalidCredentials:
      server_.send(401, "text/plain",
                   "Invalid administrator password. Reload /login to try again");
      break;
    case admin::LoginResult::Code::Accepted:
      server_.send(500, "text/plain", "Unexpected sign-in result");
      break;
  }
}

void LanWebUi::updateAdminStatus(StatusSnapshot &status) {
  status.adminConfigured = auth_.configured();
  String cookieHeader = server_.header("Cookie");
  status.adminSessionActive =
      status.adminConfigured &&
      auth_.hasSession(cookieHeader.c_str(), millis());
  eraseString(cookieHeader);
}

bool LanWebUi::consumeSetupRequest() {
  const bool requested = setupRequested_;
  setupRequested_ = false;
  return requested;
}

bool LanWebUi::consumeRebootRequest() {
  const bool requested = rebootRequested_;
  rebootRequested_ = false;
  return requested;
}

bool LanWebUi::consumeNetworkDiagnosticsRequest() {
  const bool requested = networkDiagnosticsRequested_;
  networkDiagnosticsRequested_ = false;
  return requested;
}

}  // namespace brooklyn::web

#endif  // BROOKLYN_NETWORK_ENABLED
