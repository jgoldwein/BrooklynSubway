// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "lan_web_policy.h"
#include "admin_auth.h"
#include "network_config.h"
#include "normal_config.h"

#if BROOKLYN_NETWORK_ENABLED

#include "ota_esp.h"

#include <WebServer.h>

namespace brooklyn::web {

class LanWebUi {
 public:
  explicit LanWebUi(config::DeviceConfiguration &configuration);
  bool begin();
  void stop();
  void service(const StatusSnapshot &status);
  bool active() const { return active_; }
  bool consumeSetupRequest();
  bool consumeRebootRequest();
  bool consumeNetworkDiagnosticsRequest();

 private:
  void registerRoutes();
  void handleHome();
  void handleStatus();
  void handleNetworkDiagnostics();
  void handleBrightness();
  void handleDnsSettings();
  void handleRoutes();
  void handleRunNetworkDiagnostics();
  void handleSetupConfirmation();
  void handleSetupRequest();
  void handleReboot();
  void handleLoginPage();
  void handleLogin();
  void handleLogout();
  void handlePasswordChange();
  void handleUpdatePage();
  void handleUpdateRaw();
  void handleUpdateComplete();
  int updateAuthorizationStatus();
  bool updateContentTypeAccepted();
  void sendHtml(const char *body, size_t length);
  bool authorizeRequest(bool requireCsrf);
  bool requestSession(char *csrfToken, size_t capacity);
  void sendLoginRedirect();
  void sendLoginFailure(admin::LoginResult::Code code);
  void updateAdminStatus(StatusSnapshot &status);

  config::DeviceConfiguration &configuration_;
  admin::Manager auth_;
  WebServer server_;
  StatusSnapshot status_{};
  bool active_ = false;
  bool routesRegistered_ = false;
  bool setupRequested_ = false;
  bool rebootRequested_ = false;
  bool networkDiagnosticsRequested_ = false;
  ota::WebOtaSession otaSession_;
  int updateDeniedStatus_ = 0;
  bool updateStarted_ = false;
  ota::Result updateResult_ = ota::Result::NotStarted;
};

}  // namespace brooklyn::web

#endif  // BROOKLYN_NETWORK_ENABLED
