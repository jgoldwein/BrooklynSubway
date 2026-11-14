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
  void handleLedDiagnosticPage();
  void handleLedDiagnosticRun();
  void handleStatus();
  void handleNetworkDiagnostics();
  void handleBrightness();
  void handlePollInterval();
  void handleBlueHold();
  void handleOledDimmer();
  void handleUnknownTiming();
  void handleOledAddress();
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
  void handleUpdateCheck();
  void handleUpdateInstall();
  void handleUpdateReboot();
  void handleUpdateAuto();
  void handleUpdateInterval();
  void serviceFirmwareUpdates(uint32_t nowMs);
  bool checkLatestRelease();
  bool installAvailableRelease();
  int updateAuthorizationStatus();
  bool updateContentTypeAccepted();
  void sendHtml(const char *body, size_t length);
  void sendNotice(int httpCode, const char *title, const char *message,
                  int kind, int refreshSeconds,
                  const char *backPath = "/update",
                  const char *backLabel = "Back to firmware update");
  void handleUpdateProgress();
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
  enum class AllDiagnosticState : uint8_t { NotRun, Pending, Complete, Stopped };
  AllDiagnosticState allDiagnosticState_ = AllDiagnosticState::NotRun;
  bool routeDiagnostic_ = false;
  ota::WebOtaSession otaSession_;
  int updateDeniedStatus_ = 0;
  bool updateStarted_ = false;
  ota::Result updateResult_ = ota::Result::NotStarted;
  bool forceReleaseCheck_ = false;
  bool installReleaseRequested_ = false;
  bool autoInstall_ = false;
  uint32_t checkIntervalMs_ = 0;
  // 0 idle, 1 downloading, 2 staged, 3 failed
  uint8_t installPhase_ = 0;
  uint32_t installReceived_ = 0;
  uint32_t installTotal_ = 0;
  bool hasReleaseCheck_ = false;
  bool updateAvailable_ = false;
  uint32_t lastReleaseCheckMs_ = 0;
  char availableVersion_[33] = {};
  char releaseStatus_[112] = "Waiting for a network connection";
};

}  // namespace brooklyn::web

#endif  // BROOKLYN_NETWORK_ENABLED
