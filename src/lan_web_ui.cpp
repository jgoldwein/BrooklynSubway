// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_ui.h"

#if BROOKLYN_NETWORK_ENABLED

#include <Arduino.h>

#include <string.h>

namespace brooklyn::web {
namespace {

constexpr size_t kPageCapacity = 8192;
constexpr size_t kJsonCapacity = 2048;
constexpr size_t kNetworkDiagnosticsJsonCapacity = 768;
char gPage[kPageCapacity];
char gJson[kJsonCapacity];
char gNetworkDiagnosticsJson[kNetworkDiagnosticsJsonCapacity];

constexpr const char *kSetupConfirmation =
    "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Change Wi-Fi</title></head><body><h1>Change Wi-Fi</h1>"
    "<p>The setup network will start; existing credentials are retained. "
    "A replacement is saved only after it successfully connects.</p>"
    "<form method=\"post\" action=\"/wifi/setup/confirm\">"
    "<button type=\"submit\">Start BrooklynMap-Avi setup</button></form>"
    "<p><a href=\"/\">Cancel</a></p></body></html>";

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

}  // namespace

LanWebUi::LanWebUi(config::DeviceConfiguration &configuration)
    : configuration_(configuration), server_(80) {}

bool LanWebUi::begin() {
  if (active_) return true;
  if (!routesRegistered_) {
    registerRoutes();
    routesRegistered_ = true;
  }
  server_.begin();
  active_ = true;
  Serial.println("LAN UI: HTTP server started on station interface");
  return true;
}

void LanWebUi::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleHome(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/network-diagnostics", HTTP_GET,
             [this]() { handleNetworkDiagnostics(); });
  server_.on("/network-diagnostics/run", HTTP_POST,
             [this]() { handleRunNetworkDiagnostics(); });
  server_.on("/config/brightness", HTTP_POST,
             [this]() { handleBrightness(); });
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
  server_.setContentLength(length);
  server_.send(200, "text/html; charset=utf-8", "");
  server_.sendContent(body, length);
}

void LanWebUi::handleHome() {
  const size_t length =
        renderHomePage(gPage, sizeof(gPage), status_, configuration_.brightness(),
                       configuration_.routeVisibilityMask());
  if (length == 0) {
    server_.send(500, "text/plain", "Unable to render status page");
    return;
  }
  sendHtml(gPage, length);
}

void LanWebUi::handleStatus() {
  const size_t length = renderStatusJson(
      gJson, sizeof(gJson), status_, configuration_.brightness(),
      configuration_.routeVisibilityMask());
  if (length == 0) {
    server_.send(500, "text/plain", "Unable to render status");
    return;
  }
  server_.sendHeader("Cache-Control", "no-store");
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
  server_.send(200, "application/json", gNetworkDiagnosticsJson);
}

void LanWebUi::handleRunNetworkDiagnostics() {
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

void LanWebUi::handleRoutes() {
  constexpr size_t kMaxRouteParameters = semantic::kRouteCount + 1;
  constexpr size_t kNameCapacity = 16;
  constexpr size_t kValueCapacity = 8;
  const int argumentCount = server_.args();
  if (argumentCount < 1 ||
      static_cast<size_t>(argumentCount) > kMaxRouteParameters) {
    server_.send(400, "text/plain", "Invalid route selection");
    return;
  }

  char names[kMaxRouteParameters][kNameCapacity]{};
  char values[kMaxRouteParameters][kValueCapacity]{};
  RouteSelectionParam parameters[kMaxRouteParameters]{};
  for (int i = 0; i < argumentCount; ++i) {
    const String name = server_.argName(i);
    const String value = server_.arg(i);
    if (name.length() >= kNameCapacity || value.length() >= kValueCapacity) {
      server_.send(400, "text/plain", "Invalid route selection");
      return;
    }
    name.toCharArray(names[i], kNameCapacity);
    value.toCharArray(values[i], kValueCapacity);
    parameters[i] = {names[i], values[i]};
  }

  uint32_t visibilityMask = 0;
  if (!parseRouteSelection(parameters, static_cast<size_t>(argumentCount),
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
  sendHtml(kSetupConfirmation, strlen(kSetupConfirmation));
}

void LanWebUi::handleSetupRequest() {
  setupRequested_ = true;
  sendHtml(kSetupStarting, strlen(kSetupStarting));
}

void LanWebUi::handleReboot() {
  rebootRequested_ = true;
  sendHtml(kRebooting, strlen(kRebooting));
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
