// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "password_visibility.h"
#include "web_ui_assets.h"
#include "normal_config.h"
#include "semantic_api.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace brooklyn::web {
namespace {

class Writer {
 public:
  Writer(char *out, size_t capacity) : out_(out), capacity_(capacity) {
    if (capacity_ != 0) {
      out_[0] = '\0';
    }
  }

  bool append(const char *text) {
    if (!valid_ || text == nullptr) {
      valid_ = false;
      return false;
    }
    const size_t length = strlen(text);
    if (length >= capacity_ - used_) {
      valid_ = false;
      return false;
    }
    memcpy(out_ + used_, text, length + 1);
    used_ += length;
    return true;
  }

  bool format(const char *format, ...) {
    if (!valid_ || used_ >= capacity_) {
      valid_ = false;
      return false;
    }
    va_list args;
    va_start(args, format);
    const int length =
        vsnprintf(out_ + used_, capacity_ - used_, format, args);
    va_end(args);
    if (length < 0 || static_cast<size_t>(length) >= capacity_ - used_) {
      valid_ = false;
      return false;
    }
    used_ += static_cast<size_t>(length);
    return true;
  }

  bool html(const char *text) {
    if (text == nullptr) {
      text = "";
    }
    for (const unsigned char *p =
             reinterpret_cast<const unsigned char *>(text);
         *p != 0; ++p) {
      const char *escaped = nullptr;
      switch (*p) {
        case '&': escaped = "&amp;"; break;
        case '<': escaped = "&lt;"; break;
        case '>': escaped = "&gt;"; break;
        case '"': escaped = "&quot;"; break;
        case '\'': escaped = "&#39;"; break;
        default: break;
      }
      if (escaped != nullptr) {
        if (!append(escaped)) return false;
      } else if (*p < 0x20 || *p == 0x7f) {
        if (!format("&#x%02x;", static_cast<unsigned>(*p))) return false;
      } else {
        const char character[] = {static_cast<char>(*p), '\0'};
        if (!append(character)) return false;
      }
    }
    return valid_;
  }

  bool json(const char *text) {
    if (text == nullptr) {
      text = "";
    }
    for (const unsigned char *p =
             reinterpret_cast<const unsigned char *>(text);
         *p != 0; ++p) {
      switch (*p) {
        case '"':
          if (!append("\\\"")) return false;
          break;
        case '\\':
          if (!append("\\\\")) return false;
          break;
        case '\b':
          if (!append("\\b")) return false;
          break;
        case '\f':
          if (!append("\\f")) return false;
          break;
        case '\n':
          if (!append("\\n")) return false;
          break;
        case '\r':
          if (!append("\\r")) return false;
          break;
        case '\t':
          if (!append("\\t")) return false;
          break;
        default:
          if (*p < 0x20) {
            if (!format("\\u%04x", static_cast<unsigned>(*p))) return false;
          } else {
            const char character[] = {static_cast<char>(*p), '\0'};
            if (!append(character)) return false;
          }
          break;
      }
    }
    return valid_;
  }

  size_t size() const { return valid_ ? used_ : 0; }

 private:
  char *out_;
  size_t capacity_;
  size_t used_ = 0;
  bool valid_ = out_ != nullptr && capacity_ != 0;
};

bool validIpv4(const char *address) {
  if (address == nullptr || address[0] == '\0') return false;
  unsigned octet = 0;
  unsigned dots = 0;
  unsigned digits = 0;
  for (const char *p = address;; ++p) {
    if (*p >= '0' && *p <= '9') {
      octet = octet * 10 + static_cast<unsigned>(*p - '0');
      if (octet > 255 || ++digits > 3) return false;
    } else if (*p == '.' || *p == '\0') {
      if (digits == 0) return false;
      if (*p == '\0') return dots == 3;
      if (++dots > 3) return false;
      octet = 0;
      digits = 0;
    } else {
      return false;
    }
  }
}

const char *displayStateName(display::State state) {
  switch (state) {
    case display::State::WaitingForData: return "WaitingForData";
    case display::State::Fresh: return "Fresh";
    case display::State::DegradedHold: return "DegradedHold";
    case display::State::DegradedPartial: return "DegradedPartial";
    case display::State::StaleHold: return "StaleHold";
    case display::State::NoData: return "NoData";
  }
  return "Unknown";
}

const char *cycleStateName(transport::CycleOutcome outcome) {
  switch (outcome) {
    case transport::CycleOutcome::Complete: return "COMPLETE";
    case transport::CycleOutcome::Partial: return "PARTIAL";
    case transport::CycleOutcome::NoFeeds: return "NO FEEDS";
    case transport::CycleOutcome::SkippedBuffer: return "SKIPPED BUFFER";
    case transport::CycleOutcome::SkippedWifi: return "SKIPPED WIFI";
    case transport::CycleOutcome::SkippedTime: return "SKIPPED TIME";
  }
  return "UNKNOWN";
}

bool appendNullableAge(Writer &writer, bool available, uint32_t age) {
  return available ? writer.format("%lu", static_cast<unsigned long>(age))
                   : writer.append("null");
}

void appendCheckJson(Writer &writer,
                     const network::diagnostics::CheckResult &result) {
  writer.append("{\"state\":\"");
  writer.append(network::diagnostics::checkStateName(result.state));
  writer.append("\",\"address\":");
  if (result.address[0] != '\0') {
    writer.append("\"");
    writer.json(result.address);
    writer.append("\"");
  } else {
    writer.append("null");
  }
  writer.format(",\"elapsedMs\":%lu}",
                static_cast<unsigned long>(result.elapsedMs));
}

}  // namespace

bool ServerOwnership::enterProvisioning() {
  if (mode_ == ServerMode::NormalLan) return false;
  mode_ = ServerMode::Provisioning;
  return true;
}

bool ServerOwnership::enterNormalLan() {
  if (mode_ == ServerMode::Provisioning) return false;
  mode_ = ServerMode::NormalLan;
  return true;
}

bool parseBrightness(const char *text, uint8_t &brightness) {
  if (text == nullptr || text[0] == '\0') return false;
  unsigned value = 0;
  for (const char *p = text; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') return false;
    value = value * 10 + static_cast<unsigned>(*p - '0');
    if (value > 255) return false;
  }
  brightness = static_cast<uint8_t>(value);
  return true;
}

bool parseDnsSettings(const DnsSettingsParam *parameters, size_t count,
                      bool &enabled, uint32_t &ipv4) {
  if (parameters == nullptr || (count != 2 && count != 3)) return false;
  bool markerSeen = false;
  bool enabledSeen = false;
  bool serverSeen = false;
  bool candidateEnabled = false;
  uint32_t candidateIpv4 = 0;
  for (size_t i = 0; i < count; ++i) {
    const DnsSettingsParam &parameter = parameters[i];
    if (parameter.name == nullptr || parameter.value == nullptr) return false;
    if (strcmp(parameter.name, "dnsForm") == 0) {
      if (markerSeen || strcmp(parameter.value, "1") != 0) return false;
      markerSeen = true;
    } else if (strcmp(parameter.name, "manualDnsEnabled") == 0) {
      if (enabledSeen || strcmp(parameter.value, "1") != 0) return false;
      enabledSeen = true;
      candidateEnabled = true;
    } else if (strcmp(parameter.name, "dnsServer") == 0) {
      if (serverSeen) return false;
      serverSeen = true;
      if (parameter.value[0] != '\0' &&
          !config::parseDnsIpv4(parameter.value, candidateIpv4)) {
        return false;
      }
    } else {
      return false;
    }
  }
  if (!markerSeen || !serverSeen || (candidateEnabled && candidateIpv4 == 0)) {
    return false;
  }
  enabled = candidateEnabled;
  ipv4 = candidateIpv4;
  return true;
}

bool parseRouteSelection(const RouteSelectionParam *parameters, size_t count,
                         uint32_t &visibilityMask) {
  if (parameters == nullptr || count == 0 ||
      count > semantic::kRouteCount + 1) {
    return false;
  }

  bool formMarkerSeen = false;
  uint32_t candidate = 0;
  for (size_t i = 0; i < count; ++i) {
    const RouteSelectionParam &parameter = parameters[i];
    if (parameter.name == nullptr || parameter.value == nullptr) return false;
    if (strcmp(parameter.name, "routeForm") == 0) {
      if (formMarkerSeen || strcmp(parameter.value, "1") != 0) return false;
      formMarkerSeen = true;
      continue;
    }
    if (strcmp(parameter.name, "route") != 0) return false;

    uint32_t bit = 0;
    for (uint8_t route = 1; route <= semantic::kRouteCount; ++route) {
      if (strcmp(parameter.value, semantic::kRouteNames[route]) == 0) {
        bit = uint32_t{1} << (route - 1);
        break;
      }
    }
    if (bit == 0 || (candidate & bit) != 0) return false;
    candidate |= bit;
  }
  if (!formMarkerSeen) return false;
  visibilityMask = candidate;
  return true;
}

size_t renderOledStatus(char *out, size_t capacity, const oled::Status &s) {
  const int n = snprintf(out, capacity, "%s; startup address 0x%02X; active address 0x%02X; I2C code %u",
      oled::stateName(s.state), s.requestedAddress, s.activeAddress, s.error);
  return n > 0 && static_cast<size_t>(n) < capacity ? static_cast<size_t>(n) : 0;
}

// Compact, read-only report: counts describe the latest poll, not optical feedback.
size_t renderBlueSummary(char *out, size_t capacity, const pipeline::BlueReport &report) {
  Writer w(out, capacity);
  if (!report.timestamp) { w.append("Waiting for a completed poll."); return w.size(); }
  w.format("Latest poll: %lu (Unix). Station LEDs, visible routes; live timers or held/paused display may differ.\n",
           static_cast<unsigned long>(report.timestamp));
  w.format("Station indications: %u recent matching stop LEDs, %u uncertain LEDs.\n", report.steadyLeds, report.uncertainLeds);
  w.append("Reported STOPPED_AT candidates:\nRoute  LEDs Trains Old NextDiff Future NoETA Shared Changed\n");
  for (uint8_t i = 1; i <= semantic::kRouteCount; ++i) {
    const auto &r = report.routes[i];
    if (!r.leds) continue;
    w.format("%-5s %4u %6u %3u %8u %6u %5u %6u %7u\n", semantic::routeName(static_cast<semantic::Route>(i)),
        r.leds, r.trains, r.old, r.different, r.future, r.missing, r.shared, r.changed);
  }
  w.format("Compared %u prior stopped trips; history overflow %u.\n", report.comparedTrips, report.historyDropped);
  w.append("Old: last detected movement >90s. NextDiff: prediction names another stop.\n"
           "Future: predicted arrival at this stop is still ahead. NoETA: no usable prediction.\n"
           "Shared: LEDs with multiple trains; recent matching stops take priority.\n"
           "Changed: same trip changed station since the prior poll while still STOPPED_AT.\n"
           "These are STOPPED_AT reports, not confirmed station dwell.\n");
  if (report.sampleCount) w.append("Examples to review (up to 8):\n");
  for (uint8_t i = 0; i < report.sampleCount && i < pipeline::kBlueSamples; ++i) {
    const auto &sample = report.samples[i];
    const auto *m = semantic::semanticForLed(sample.led);
    if (!m) continue;
    const char *name = semantic::stationName(m->stationId);
    w.format("%s %.28s [LED%u]: %u sharing, movement age ", semantic::routeName(m->route), name ? name : "?", m->designator, sample.trains);
    if (sample.flags & pipeline::UnknownAge) w.append("unknown");
    else w.format("%s%us", sample.age == 255 ? ">=" : "", sample.age);
    w.format(", next %s%s%s%s%s\n", sample.nextStop[0] ? sample.nextStop : "mixed/none",
        sample.flags & pipeline::NextDifferent ? " (different)" : "",
        sample.flags & pipeline::FutureArrival ? " (future)" : "",
        sample.flags & pipeline::NoPrediction ? " (no ETA)" : "",
        sample.flags & pipeline::StopChanged ? " (stop changed)" : "");
  }
  return w.size();
}

size_t renderHomePage(char *out, size_t capacity,
                      const StatusSnapshot &status, uint8_t brightness,
                      uint32_t routeVisibilityMask, const char *csrfToken) {
  Writer writer(out, capacity);
  // Refresh read-only values in place; never reload forms or rotate login tokens.
  const bool inlineLogin = status.adminConfigured && !status.adminSessionActive &&
                           csrfToken != nullptr && csrfToken[0] != '\0';
  writer.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<title>Brooklyn Subway</title>");
  writer.append(kWebUiAssetHead);
  writer.append("</head><body><header><div class=\"brand\"><span class=\"brand-mark\" aria-hidden=\"true\">BK</span>"
                "<div><h1>Brooklyn Subway</h1><p class=\"subtitle\">");
  writer.append("Specially Designed for Avi Yao</p></div></div>");
  if (status.adminSessionActive) writer.append("<span class=\"account\">Settings unlocked</span>");
  else if (status.adminConfigured) writer.append("<a class=\"button secondary\" href=\"/login\">Sign in</a>");
  else writer.append("<a class=\"button secondary\" href=\"#advanced\">Set up</a>");
  writer.append("</header>"); writer.append(kWebUiNavigation);
  writer.append("<main><section class=\"hero\" data-view=\"overview\" id=\"overview\"><p class=\"kicker\">Your map right now</p>"
                "<h2 data-status=\"mapPower\">");
  writer.append(status.physicalLedOutputEnabled?"Lights on":status.physicalPowerPaused?"Lights paused":"Lights off");
  writer.append("</h2><p data-status=\"friendlyState\">");
  writer.append(status.displayState==display::State::Fresh?"Train data is up to date":
                status.displayState==display::State::WaitingForData?"Waiting for the first train update":
                status.displayState==display::State::NoData?"No current train data":"Some train updates are delayed");
  writer.append("</p><p class=\"hint\">Firmware <span data-status=\"firmwareVersion\">");
  writer.html(status.firmwareVersion[0] != '\0' ? status.firmwareVersion : "unknown");
  writer.append("</span></p><p><a class=\"hint\" href=\"#wifi\" data-status=\"connectionLabel\">");
  writer.append(status.networkConnected?"Wi-Fi connected":"Wi-Fi disconnected");
  writer.append("</a></p><div class=\"metrics\"><div class=\"metric\"><strong data-status=\"resolvedTrains\">");
  writer.format("%lu</strong><span>Mapped trains</span></div><div class=\"metric\"><strong data-status=\"feedSummary\">%u / %u</strong>"
                "<span>Feeds working</span></div><div class=\"metric\"><strong data-status=\"dataAgeSeconds\">",
                static_cast<unsigned long>(status.resolvedTrains),status.feedsSucceeded,status.feedsTotal);
  if (status.hasDataAge) writer.format("%lu seconds",static_cast<unsigned long>(status.dataAgeSeconds)); else writer.append("Waiting");
  writer.append("</strong><span>Latest update</span></div></div><p class=\"hint\"><strong data-status=\"visibleMapLeds\">");
  writer.format("%u</strong> lit map LEDs. Several trains can share one light; uncertain markers may fade. <a href=\"/monitor\">Train monitor</a></p>",status.visibleMapLeds);
  writer.append("<p class=\"hint\">POWER switches displays; hold 3 seconds to restart. BRIGHT/DIM work with one display on.</p>"
                "<div class=\"actions\"><a class=\"button\" href=\"#lights\">Adjust train lights</a>"
                "<a class=\"button secondary\" href=\"#screen\">Screen brightness</a></div></section>"
                "<section class=\"wide\" data-view=\"advanced\" id=\"advanced\"><p class=\"kicker\">Tools &amp; help</p><h2>Keep your map running</h2>"
                "<p>Check for updates or test the lights. You may need to sign in first.</p><div class=\"actions\">"
                "<a class=\"button\" href=\"/update\">Firmware updates</a><a class=\"button secondary\" href=\"/hardware-test\">Test the lights</a><a class=\"button secondary\" href=\"/monitor\">Train monitor</a></div></section>"
                "<details class=\"card wide\" data-view=\"advanced\"><summary>Device information</summary><dl><dt>Device name</dt><dd>");
  writer.html(status.deviceName);
  writer.append("</dd><dt>Firmware</dt><dd>");
  writer.append(kFirmwareId);
  writer.append("</dd><dt>Firmware version</dt><dd>");
  writer.html(status.firmwareVersion[0] != '\0' ? status.firmwareVersion : "unknown");
  writer.append("</dd><dt>Available release</dt><dd>");
  if (status.availableVersion[0] != '\0') {
    writer.html(status.availableVersion);
    writer.append(" (<a href=\"/update\">review update</a>)");
  } else {
    writer.append("none newer known");
  }
  writer.append("</dd><dt>Target</dt><dd>");
  writer.html(status.target);
  writer.append("</dd><dt>Uptime</dt><dd data-status=\"uptimeSeconds\">");
  writer.format("%lu seconds", static_cast<unsigned long>(status.uptimeSeconds));
  writer.append("</dd><dt>Reset reason</dt><dd>");
  writer.html(status.resetReason);
  writer.append("</dd><dt>Physical LED output</dt><dd>");
  writer.append(status.physicalLedOutputEnabled ? "ENABLED (cap 16/255)" :
                status.physicalPowerPaused ? "PAUSED (POWER resumes)" : "DISABLED");
  writer.format("</dd><dt>Live frames sent</dt><dd>%lu</dd>"
                "<dt>Map data changes received</dt><dd>%lu</dd>"
                "<dt>Blinking station LEDs</dt><dd>%u</dd><dt>Last live frame sent</dt><dd>",
                static_cast<unsigned long>(status.physicalFramesWritten),
                static_cast<unsigned long>(status.physicalMapChanges), status.physicalBlinkingLeds);
  if (status.physicalFramesWritten) writer.format("%lu seconds ago", static_cast<unsigned long>(status.physicalFrameAgeSeconds));
  else writer.append("Not yet");
  writer.append("</dd></dl></details><details class=\"card wide\" data-view=\"advanced\"");
  if (!status.adminConfigured) writer.append(" open");
  writer.append("><summary>Sign-in &amp; password</summary><p>");
  if (!status.adminConfigured) {
    writer.append("Set a password first. Hold the knob for 10 seconds, connect to "
                  "BrooklynMap-Avi, then open 192.168.4.1.");
  } else if (status.adminSessionActive) {
    writer.append("You are signed in. You can change the map settings.");
  } else if (inlineLogin) {
    writer.append("Settings are locked. Sign in to change them.</p>"
                  "<form method=\"post\" action=\"/login\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><label>Password <input type=\"password\" name=\"password\" "
                  "autocomplete=\"current-password\" required></label> ");
    writer.append(kPasswordVisibilityControl);
    writer.append("<button type=\"submit\">Sign in</button></form><p "
                  "class=\"muted\">Live data refreshes without clearing the sign-in "
                  "form.");
  } else {
    writer.append("Settings are locked. <a href=\"/login\">Sign in</a>");
  }
  writer.append("</p>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/logout\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><button type=\"submit\">Sign out</button></form>"
                  "<p><a href=\"/hardware-test\">Hardware diagnostics</a></p>"
                  "<p><a href=\"/update\">Update firmware</a><br>Current: ");
    writer.html(status.firmwareVersion[0] != '\0' ? status.firmwareVersion : "unknown");
    writer.append(" | Available release: ");
    writer.html(status.availableVersion[0] != '\0' ? status.availableVersion
                                                    : "none newer known");
    writer.append("</p>"
                  "<details><summary>Change password</summary>"
                  "<form method=\"post\" action=\"/admin/password\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><label>New password <input type=\"password\" "
                  "name=\"password\" minlength=\"8\" maxlength=\"128\" "
                  "autocomplete=\"new-password\" required></label> "
                  "<label>Confirm password <input type=\"password\" "
                  "name=\"confirmation\" minlength=\"8\" maxlength=\"128\" "
                  "autocomplete=\"new-password\" required></label> ");
    writer.append(kPasswordVisibilityControl);
    writer.append("<button type=\"submit\">Change password</button></form></details>");
  }
  writer.append("<p class=\"muted\">Normal-LAN access uses HTTP; passwords "
                "and page contents are not encrypted in transit. Do not use "
                "this interface on an untrusted network.</p></details>"
                "<section class=\"wide\" data-view=\"wifi\" id=\"wifi\"><h2>Wi-Fi</h2><p>");
  writer.append(status.networkConnected?"Connected to ":"Not connected to Wi-Fi. Last network: "); writer.html(status.ssid);
  writer.append("</p><details><summary>Connection details</summary><dl><dt>Connection</dt><dd>");
  writer.append(status.networkConnected ? "Connected" : "Disconnected");
  writer.append("</dd><dt>SSID</dt><dd>");
  writer.html(status.ssid);
  writer.append("</dd><dt>LAN IP</dt><dd>");
  if (validIpv4(status.ip)) {
    writer.append("<a href=\"http://");
    writer.html(status.ip);
    writer.append("/\">");
    writer.html(status.ip);
    writer.append("</a>");
  } else {
    writer.append("Unavailable");
  }
  writer.append("</dd><dt>RSSI</dt><dd>");
  if (status.networkConnected) {
    writer.format("%d dBm", static_cast<int>(status.rssi));
  } else {
    writer.append("Unavailable");
  }
  writer.append("</dd><dt>Subnet</dt><dd>");
  writer.html(status.subnet[0] != '\0' ? status.subnet : "Unavailable");
  writer.append("</dd><dt>Gateway</dt><dd>");
  writer.html(status.gateway[0] != '\0' ? status.gateway : "Unavailable");
  writer.append("</dd><dt>DHCP DNS 1</dt><dd>");
  writer.html(status.dnsPrimary[0] != '\0' ? status.dnsPrimary : "Unavailable");
  writer.append("</dd><dt>DHCP DNS 2</dt><dd>");
  writer.html(status.dnsSecondary[0] != '\0' ? status.dnsSecondary : "Unavailable");
  writer.append("</dd><dt>Hostname</dt><dd>");
  writer.append(kHostname);
  writer.append("</dd><dt>Time synchronization</dt><dd>");
  writer.append(status.timeSynchronized ? "Synchronized" : "Waiting");
  writer.append("</dd><dt>Unix time</dt><dd>");
  if (status.timeSynchronized) {
    writer.format("%lu", static_cast<unsigned long>(status.unixTime));
  } else {
    writer.append("Unavailable");
  }
  writer.append("</dd>");
  if (status.mdnsAvailable) {
    writer.append("<dt>Local name</dt><dd><a href=\"");
    writer.append(kMdnsUrl);
    writer.append("\">");
    writer.append(kMdnsUrl);
    writer.append("</a></dd>");
  }
  writer.append("</dl></details>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/wifi/setup\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><button type=\"submit\">Change Wi-Fi</button></form>");
  }
  writer.append("<p class=\"muted\">Wi-Fi setup opens a separate network named BrooklynMap-Avi. "
                "Your current Wi-Fi stays saved until a new network connects.</p></section>"
                "<details class=\"card wide\" data-view=\"wifi\"><summary>Advanced Wi-Fi settings</summary><h3>DNS configuration</h3><dl>"
                "<dt>Manual override</dt><dd>");
  writer.append(status.manualDnsEnabled ? "Enabled" : "Disabled");
  writer.append("</dd><dt>Override status</dt><dd>");
  writer.html(status.dnsConfigStatus);
  writer.append("</dd><dt>Effective DNS source</dt><dd>");
  writer.html(status.effectiveDnsSource);
  writer.append("</dd><dt>Effective DNS server</dt><dd>");
  writer.html(status.effectiveDnsServer[0] != '\0'
                  ? status.effectiveDnsServer
                  : "Unavailable");
  writer.append("</dd></dl>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/config/dns\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><input type=\"hidden\" name=\"dnsForm\" value=\"1\">"
                "<label><input type=\"checkbox\" name=\"manualDnsEnabled\" "
                "value=\"1\"");
  if (status.manualDnsEnabled) writer.append(" checked");
  writer.append("> Use manual DNS</label><br>"
                "<label for=\"dnsServer\">Manual DNS IPv4 address</label> "
                "<input id=\"dnsServer\" name=\"dnsServer\" type=\"text\" "
                "inputmode=\"decimal\" maxlength=\"15\" value=\"");
  writer.html(status.manualDnsServer);
    writer.append("\"><button type=\"submit\">Save DNS settings</button></form>");
  }
  writer.append("<p class=\"muted\">Disabled by default. This changes only "
                "the resolver; DHCP, Wi-Fi credentials, feeds, and TLS policy "
                "are unchanged.</p></details>"
                "<section class=\"wide\" data-view=\"overview\"><h2>Train data, colors &amp; beep tones</h2><details><summary>Train data details</summary><dl><dt>Last poll cycle</dt><dd data-status=\"feedCycleState\">");
  writer.append(status.hasCycle ? cycleStateName(status.cycleOutcome) : "Not run");
  writer.append("</dd><dt>Download</dt><dd data-status=\"downloadState\">");
  writer.append(status.fetchingFeeds?"Fetching train data":"Waiting for next update");
  writer.append("</dd><dt>Feeds succeeded / total</dt><dd data-status=\"feedSummary\">");
  if (status.hasCycle) {
    writer.format("%u / %u", status.feedsSucceeded, status.feedsTotal);
  } else {
    writer.append("Not available");
  }
  writer.append("</dd><dt>Operational display</dt><dd data-status=\"displayState\">");
  writer.append(displayStateName(status.displayState));
  writer.append("</dd><dt>Last usable update age</dt><dd data-status=\"dataAgeSeconds\">");
  if (status.hasDataAge) {
    writer.format("%lu seconds", static_cast<unsigned long>(status.dataAgeSeconds));
  } else {
    writer.append("Not available");
  }
  writer.append("</dd><dt>Last complete update age</dt><dd data-status=\"completeAgeSeconds\">");
  if (status.hasCompleteAge) {
    writer.format("%lu seconds", static_cast<unsigned long>(status.completeAgeSeconds));
  } else {
    writer.append("Not available");
  }
  writer.format("</dd><dt>Resolved trains</dt><dd data-status=\"resolvedTrains\">%lu</dd>"
                "<dt>Approaching trains (included above)</dt><dd data-status=\"approachingTrains\">%lu</dd>"
                "<dt>Occupied LEDs</dt><dd data-status=\"occupiedLeds\">%u</dd>"
                "<dt>Collision count</dt><dd data-status=\"collisionCount\">%u</dd></dl>"
                "</details><div class=\"legend\"><span><i class=\"swatch\" data-train-color=\"approaching\" style=\"background:#%02X%02X%02X\"></i>Approaching</span>"
                "<span><i class=\"swatch\" data-train-color=\"stopped\" style=\"background:#%02X%02X%02X\"></i>Stopped</span>"
                "<span><i class=\"swatch\" data-train-color=\"leaving\" style=\"background:#%02X%02X%02X\"></i>Leaving</span>"
                "<span><i class=\"swatch\" data-train-color=\"uncertain\" style=\"background:#%02X%02X%02X\"></i>Uncertain / last reported</span></div>"
                "<p class=\"hint\">Last sound: <span data-status=\"lastSound\">Loading...</span></p><p class=\"hint\">Choose these colors in Train lights. Faint markers show the last reported place when selected. No traffic lights blink.</p></section>"
                "<details class=\"card wide\" data-view=\"advanced\"><summary>Network diagnostics</summary><dl>"
                "<dt>Diagnostic run</dt><dd>%s</dd>"
                "<dt>NTP DNS</dt><dd>%s",
                static_cast<unsigned long>(status.resolvedTrains),
                static_cast<unsigned long>(status.approachingTrains),
                status.occupiedLeds, status.collisionCount,
                status.trainColors.approaching.red,status.trainColors.approaching.green,status.trainColors.approaching.blue,
                status.trainColors.stopped.red,status.trainColors.stopped.green,status.trainColors.stopped.blue,
                status.trainColors.leaving.red,status.trainColors.leaving.green,status.trainColors.leaving.blue,
                status.trainColors.uncertain.red,status.trainColors.uncertain.green,status.trainColors.uncertain.blue,
                network::diagnostics::runStateName(
                    status.networkDiagnostics.state),
                network::diagnostics::checkStateName(
                    status.networkDiagnostics.ntpDns.state));
  if (status.networkDiagnostics.ntpDns.address[0] != '\0') {
    writer.append(" (");
    writer.html(status.networkDiagnostics.ntpDns.address);
    writer.append(")");
  }
  writer.append("</dd><dt>MTA DNS</dt><dd>");
  writer.append(network::diagnostics::checkStateName(
      status.networkDiagnostics.mtaDns.state));
  if (status.networkDiagnostics.mtaDns.address[0] != '\0') {
    writer.append(" (");
    writer.html(status.networkDiagnostics.mtaDns.address);
    writer.append(")");
  }
  writer.append("</dd><dt>MTA TCP 443</dt><dd>");
  writer.append(network::diagnostics::checkStateName(
      status.networkDiagnostics.tcp443.state));
  writer.append("</dd><dt>Direct DNS (UDP/53)</dt><dd>");
  for (size_t i = 0; i < network::diagnostics::kDirectDnsCheckCount; ++i) {
    const network::diagnostics::DirectDnsResult &result =
        status.networkDiagnostics.directDns[i];
    if (i != 0) writer.append("<br>");
    writer.html(result.resolver);
    writer.append(" -> ");
    writer.html(result.hostname);
    writer.append(": ");
    writer.append(network::diagnostics::checkStateName(result.state));
    writer.append(result.responseReceived ? ", response" : ", no response");
    if (result.hasRcode) {
      writer.format(", RCODE %u", result.rcode);
    }
    if (result.addressCount != 0) {
      writer.append(", ");
      for (uint8_t address = 0; address < result.addressCount; ++address) {
        if (address != 0) writer.append(", ");
        writer.html(result.addresses[address]);
      }
    }
    writer.format(", %lu ms",
                  static_cast<unsigned long>(result.elapsedMs));
  }
  writer.append("</dd><dt>Secure-connect failures</dt><dd>");
  writer.format("%lu</dd><dt>HTTP failures</dt><dd>%lu</dd>"
                "<dt>Transport timeouts</dt><dd>%lu</dd>"
                "<dt>Complete-body bytes</dt><dd>%llu</dd>",
                static_cast<unsigned long>(
                    status.transportCounters.secureConnectFailures),
                static_cast<unsigned long>(status.transportCounters.httpFailures),
                static_cast<unsigned long>(status.transportCounters.timeouts),
                static_cast<unsigned long long>(
                    status.transportCounters.bytesDownloaded));
  if (status.hasLastFeedFailure) {
    writer.append("<dt>Last feed failure</dt><dd>");
    writer.html(status.lastFeedFailureName);
    writer.append(": ");
    writer.html(status.lastFeedFailureCategory);
    writer.format(", %lu bytes, %lu ms",
                  static_cast<unsigned long>(status.lastFeedFailureBytes),
                  static_cast<unsigned long>(
                      status.lastFeedFailureElapsedMs));
  } else {
    writer.append("<dt>Last feed failure</dt><dd>None recorded</dd>");
  }
  writer.append("</dd></dl>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/network-diagnostics/run\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><button type=\"submit\">Run Network Diagnostics</button>"
                  "</form>");
  }
  writer.append("<p class=\"muted\">Read-only checks. TCP success does not verify HTTPS.</p></details>"
                "");
  writer.append("<section class=\"train-colors wide\" data-view=\"lights\" id=\"lights\"><h2>Train colors</h2>"
                "<p>Pick a color for each state. Defaults: approaching yellow, stopped red, leaving green, uncertain blue.</p>");
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/colors\"><input type=\"hidden\" name=\"csrf\" value=\""); writer.html(csrfToken);
    writer.append("\"><div class=\"color-options\">");
    const char *names[]={"approaching","stopped","leaving","uncertain"};
    const char *labels[]={"Approaching","Stopped","Leaving","Uncertain"};
    const led::Rgb values[]={status.trainColors.approaching,status.trainColors.stopped,status.trainColors.leaving,status.trainColors.uncertain};
    const char *defaultHex[]={"#FFFF00","#FF0000","#00FF00","#0000FF"};
    for (unsigned i=0;i<4;++i) writer.format("<label>%s<input type=\"color\" name=\"%s\" value=\"#%02X%02X%02X\" data-default=\"%s\" required></label>",labels[i],names[i],values[i].red,values[i].green,values[i].blue,defaultHex[i]);
    writer.append("</div><div class=\"actions\"><button type=\"submit\">Save colors</button>"
                  "<button type=\"button\" class=\"secondary\" onclick=\"this.form.querySelectorAll('input[data-default]').forEach(e=>e.value=e.dataset.default)\">Use default colors</button></div></form>");
  } else writer.append("<p class=\"hint\">Sign in to change colors.</p>");
  writer.append("<p class=\"hint\">Leaving and last-reported lights still fade. Brightness limits still apply. Black hides that state.</p></section>"
                "<section class=\"map-brightness\" data-view=\"lights\"><h2>Map brightness</h2>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/config/brightness\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format(
        "\"><label for=\"brightness\">Brightness: <output id=\"map-level\">%u</output> / 255</label>"
        "<input id=\"brightness\" name=\"brightness\" type=\"range\" "
        "min=\"0\" max=\"255\" value=\"%u\" "
        "oninput=\"document.getElementById('brightness-value').value=this.value;document.getElementById('map-level').textContent=this.value\">"
        "<input id=\"brightness-value\" aria-label=\"Brightness value\" "
        "type=\"number\" min=\"0\" max=\"255\" value=\"%u\" "
        "oninput=\"document.getElementById('brightness').value=this.value;document.getElementById('map-level').textContent=this.value\">"
        "<button type=\"submit\">Save brightness</button></form>",
        brightness, brightness, brightness);
  } else {
    writer.format("<p>Current setting: %u / 255 (%u%%).</p>", brightness,
                  static_cast<unsigned>(brightness) * 100 / 255);
  }
  writer.append("<p class=\"muted\">Live output starts automatically with fresh data. POWER cycles LEDs and screen; hold 3 seconds to reboot; "
                "Your map keeps a safe maximum brightness, even if you choose a higher value."
                "</p></section>");
  writer.append("<p class=\"notice wide\" data-view=\"overview\" data-status=\"uncertaintyNotice\">");
  if (status.blueReport.uncertainLeds) writer.append("Some train movements are uncertain.");
  writer.append("</p>");
  {
    char summary[3072];
    const size_t length = renderBlueSummary(summary, sizeof(summary), status.blueReport);
    writer.append("<details class=\"card wide\" data-view=\"advanced\"><summary>Train report details</summary><pre data-status=\"blueSummary\" style=\"overflow:auto\">");
    writer.html(length ? summary : "Diagnostic report unavailable");
    writer.append("</pre></details>");
  }
  const auto uncertainMode=semantic::unknownTimingMode(status.unknownTiming);
  writer.append("<section class=\"uncertain-lights\" data-view=\"lights\"><h2>Uncertain train movement</h2><p>When train reports are unclear, choose what to show. "
                "A faint light marks its last reported place, then fades away. It does not mean the train is still there. "
                "Other train lights keep their normal brightness.</p>");
  if (!status.adminSessionActive) writer.format("<p>Current choice: %s. Fade time: %u seconds. Brightness limit: %u / 255.</p>",
      uncertainMode==semantic::UnknownTimingMode::LastReported?"Last reported location":
      uncertainMode==semantic::UnknownTimingMode::Orange?"Steady color":"Hide",
      semantic::lastReportedFadeSeconds(status.unknownTiming),status.unknownTiming&255);
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/unknown-timing\"><input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format("\"><label>Uncertain trains <select name=\"mode\">"
                  "<option value=\"hide\"%s>Hide</option><option value=\"last\"%s>Show last reported location</option>"
                  "<option value=\"orange\"%s>Steady color</option></select></label> "
                  "<label>Dim light brightness <input type=\"number\" name=\"brightness\" min=\"0\" max=\"255\" value=\"%u\" required></label> "
                  "<label>Fade out after (seconds) <input type=\"number\" name=\"fade\" min=\"1\" max=\"300\" value=\"%u\" required></label>"
                  "<button>Save train display settings</button></form>",
                  uncertainMode==semantic::UnknownTimingMode::Hidden?" selected":"",
                  uncertainMode==semantic::UnknownTimingMode::LastReported?" selected":"",
                  uncertainMode==semantic::UnknownTimingMode::Orange?" selected":"",
                  status.unknownTiming&255,semantic::lastReportedFadeSeconds(status.unknownTiming));
  }
  writer.append("<p>Faint lights stay faint. Fade time applies to the fading markers. Brightness 0 hides uncertain lights.</p></section>");
  writer.append("<section class=\"blue-hold\" data-view=\"lights\"><h2>When does the stopped color change?</h2>"
                "<p>The stopped color stays on while fresh, consistent reports support a train at the station. "
                "Verified departure evidence changes the indication to movement. Old, missing or contradictory evidence becomes uncertain. "
                "There is no fixed 20-second cutoff. A repeated old report does not count as fresh evidence.</p></section>");
  writer.format("<details class=\"card wide\" data-view=\"advanced\"><summary>Train update timing</summary><p>Wait after each completed download cycle: %lu seconds. "
                "Downloads add to the total interval. MTA feeds are typically generated about every 30 seconds; "
                "faster checks can return identical data.</p>", static_cast<unsigned long>(status.pollSeconds));
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/poll\"><input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format("\"><label>Seconds <input type=\"number\" name=\"seconds\" min=\"15\" max=\"120\" value=\"%lu\" required></label>"
                  "<button>Save update interval</button></form>", static_cast<unsigned long>(status.pollSeconds));
  }
  writer.append("</details>");
  writer.format("<section data-view=\"screen\" id=\"screen\"><h2>Screen brightness</h2><p>Brightness: %u / 255. This changes only the small screen. "
                "0 is the dimmest setting; it does not turn the screen off.</p>", status.oledContrast);
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/oled-dimmer\"><input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format("\"><label>Screen brightness <output id=\"screen-level\">%u</output> / 255<input type=\"range\" name=\"contrast\" min=\"0\" max=\"255\" value=\"%u\" oninput=\"document.getElementById('screen-level').textContent=this.value\"></label>"
                  "<button>Save screen brightness</button></form>", status.oledContrast, status.oledContrast);
  }
  writer.append("</section>");
  writer.append("<section data-view=\"screen\"><h2>Screen browsing</h2><p>Turn the knob to choose a screen. On Lines, press to browse subway lines; press again to go back. ROUTE opens Lines and toggles Auto/Paused. Hold the knob 10 seconds for Wi-Fi setup.</p>");
  if(status.adminSessionActive && csrfToken && *csrfToken) {
    writer.append("<form method=\"post\" action=\"/config/oled-return\"><input type=\"hidden\" name=\"csrf\" value=\"");writer.html(csrfToken);
    writer.append("\"><label>Return to the clock after no button or knob use<select name=\"seconds\">");
    const unsigned seconds[]={0,30,60,120,300};const char *labels[]={"Stay on this screen","30 seconds","1 minute","2 minutes","5 minutes"};
    for(unsigned i=0;i<5;++i) writer.format("<option value=\"%u\"%s>%s</option>",seconds[i],status.oledReturnSeconds==seconds[i]?" selected":"",labels[i]);
    writer.append("</select></label><button>Save screen return</button></form>");
    writer.append("<p><a href=\"/screen-options\">Quiet hours and startup greeting</a></p>");
  }
  writer.append("</section>");
  writer.format("<details class=\"card\" data-view=\"screen\"><summary>Screen connection details</summary><p>Saved I2C address: 0x%02X. Default: 0x38. Reboot after changing.</p>", status.oledAddress);
  {
    char text[160]; renderOledStatus(text, sizeof(text), status.oledStatus);
    writer.append("<p data-status=\"oledStatus\">"); writer.html(text); writer.append("</p>");
    writer.append("<p class=\"muted\">Tries saved address, then 0x38/0x3C/0x3D. Active 0x00: no screen found.</p>");
  }
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/oled\"><input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format("\"><label>I2C address <input name=\"address\" maxlength=\"4\" pattern=\"0[xX][0-9a-fA-F]{2}\" value=\"0x%02X\" required></label>"
                  "<button>Save OLED address</button></form>", status.oledAddress);
  }
  writer.append("</details>");
  uint8_t visibleRoutes = 0;
  for (uint8_t route = 1; route <= semantic::kRouteCount; ++route) {
    if ((routeVisibilityMask & (uint32_t{1} << (route - 1))) != 0) {
      ++visibleRoutes;
    }
  }
  writer.format("<section class=\"wide\" data-view=\"lights\"><h2>Choose subway lines</h2><p>%u of %u lines shown. "
                "Choose the subway lines you want to see. Hidden lines still receive updates.</p>",
                visibleRoutes, semantic::kRouteCount);
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form id=\"route-settings\" method=\"post\" "
                  "action=\"/config/routes\"><input type=\"hidden\" "
                  "name=\"routeForm\" value=\"1\"><input type=\"hidden\" "
                  "name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><div class=\"route-controls\">"
                "<button type=\"button\" onclick=\"document.querySelectorAll("
                "'#route-settings input[name=route]').forEach(function(x){x.checked=true})\">"
                "Select all</button><button type=\"button\" onclick=\"document."
                "querySelectorAll('#route-settings input[name=route]').forEach("
                "function(x){x.checked=false})\">Clear all</button></div>"
                "<div class=\"route-list\">");
    for (uint8_t route = 1; route <= semantic::kRouteCount; ++route) {
      const char *name = semantic::kRouteNames[route];
      writer.append("<label><input type=\"checkbox\" name=\"route\" value=\"");
      writer.html(name);
      writer.append("\"");
      if ((routeVisibilityMask & (uint32_t{1} << (route - 1))) != 0) {
        writer.append(" checked");
      }
      writer.append("> <span class=\"route-badge r-"); writer.html(name); writer.append("\">");
      writer.html(name);
      writer.append("</span></label>");
    }
    writer.append("</div><button type=\"submit\">Save subway lines</button></form>");
  }
  writer.append("<p class=\"muted\">You can hide every line. The small status lights keep working.</p></section>");
  writer.append("<details class=\"card wide\" data-view=\"advanced\"><summary>Memory diagnostics</summary><dl>"
                "<dt>Free internal heap</dt><dd>");
  writer.format("%lu B", static_cast<unsigned long>(status.freeInternalHeap));
  writer.append("</dd><dt>Largest free internal block</dt><dd data-status=\"largestInternalBlock\">");
  writer.format("%lu B",static_cast<unsigned long>(status.largestInternalBlock));
  writer.append("</dd><dt>Minimum internal heap</dt><dd>");
  writer.format("%lu B", static_cast<unsigned long>(status.minimumInternalHeap));
  writer.append("</dd><dt>Free PSRAM</dt><dd>");
  if (status.psramPresent) {
    writer.format("%lu B", static_cast<unsigned long>(status.freePsram));
  } else {
    writer.append("Unavailable");
  }
  writer.append("</dd><dt>Minimum free PSRAM</dt><dd>");
  if (status.psramPresent) {
    writer.format("%lu B", static_cast<unsigned long>(status.minimumFreePsram));
  } else {
    writer.append("Unavailable");
  }
  writer.append("</dd><dt>PSRAM feed buffer</dt><dd>");
  writer.append(status.psramFeedBufferPresent ? "Present" : "Unavailable");
  writer.append("</dd></dl><p><a href=\"/monitor#runtime\">Connection and restart details</a></p></details><section class=\"wide\" data-view=\"advanced\"><h2>Restart the map</h2>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/reboot\" "
                  "onsubmit=\"return confirm('Reboot the map? Settings are retained.')\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><button type=\"submit\">Restart map</button></form>");
  }
  writer.append("<p class=\"muted\">Restart keeps settings. <a href=\"/defaults\">Reset to Firmware Defaults</a></p></section>"
                "</main><p id=\"live-refresh\" class=\"footer\" role=\"status\">Live values refresh automatically.</p>"
                "<script>(()=>{const note=document.getElementById('live-refresh');"
                "async function refresh(){let timer;try{if(!document.hidden){"
                "const controller=new AbortController();timer=setTimeout(()=>controller.abort(),15000);"
                "const r=await fetch('/api/status',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error();"
                "const s=await r.json();if(s.trainColors)document.querySelectorAll('[data-train-color]').forEach(e=>{const c=s.trainColors[e.dataset.trainColor];if(/^#[0-9a-f]{6}$/i.test(c))e.style.backgroundColor=c});s.feedSummary=s.feedsSucceeded+' / '+s.feedsTotal;s.connectionLabel=s.networkConnected?'Wi-Fi connected':'Wi-Fi disconnected';"
                "s.downloadState=s.fetchingFeeds?'Fetching train data':'Waiting for next update';s.mapPower=s.startupIndicatorsActive?'Starting: status lights':s.quietHoursActive&&s.quietMapBrightness===0?'Quiet hours: lights off':s.physicalLedOutputEnabled?'Lights on':s.physicalPowerPaused?'Lights paused':'Lights off';if(s.routeFocusActive)s.mapPower+=' - Line '+s.routeFocusRoute+' only';"
                "s.friendlyState=s.displayState==='Fresh'?'Train data is up to date':s.displayState==='WaitingForData'?'Waiting for the first train update':s.displayState==='NoData'?'No current train data':'Some train updates are delayed';"
                "document.querySelectorAll('[data-status]').forEach(e=>{const k=e.dataset.status;"
                "if(!(k in s))return;const v=s[k];e.textContent=v===null?'Not available':"
                "k.endsWith('Seconds')?v+' seconds':String(v);});"
                "note.textContent='Live values updated '+new Date().toLocaleTimeString();"
                "}}catch(e){note.textContent='Live refresh delayed; retrying. Displayed values may be old.';}"
                "finally{clearTimeout(timer);setTimeout(refresh,2000);}}setTimeout(refresh,1000);})();</script>"
                "</body></html>");
  return writer.size();
}

// Only fixed local GET pages are allowed; never redirect to submitted URLs or actions.
const char *loginReturnPath(const char *target) {
  if (!target || !target[0]) return "/";
  if (strcmp(target,"screen-options")==0) return "/screen-options";
  if (strcmp(target,"update")==0) return "/update";
  if (strcmp(target,"hardware-test")==0) return "/hardware-test";
  if (strcmp(target,"wifi-setup")==0) return "/wifi/setup";
  if (strcmp(target,"defaults")==0) return "/defaults";
  if (strcmp(target,"lights")==0) return "/#lights";
  if (strcmp(target,"screen")==0) return "/#screen";
  if (strcmp(target,"wifi")==0) return "/#wifi";
  if (strcmp(target,"advanced")==0) return "/#advanced";
  if (strcmp(target,"overview")==0) return "/#overview";
  return nullptr;
}
const char *loginTargetForRequest(const char *path) {
  if (!path) return "";
  if (strcmp(path,"/defaults")==0) return "defaults";
  if (strcmp(path,"/screen-options")==0 || strcmp(path,"/config/screen-options")==0) return "screen-options";
  if (strcmp(path,"/update")==0 || strncmp(path,"/update/",8)==0) return "update";
  if (strcmp(path,"/hardware-test")==0 || strcmp(path,"/hardware-test/run")==0) return "hardware-test";
  if (strcmp(path,"/wifi/setup")==0 || strcmp(path,"/wifi/setup/confirm")==0) return "wifi-setup";
  if (strcmp(path,"/config/oled")==0 || strcmp(path,"/config/oled-dimmer")==0 || strcmp(path,"/config/oled-return")==0) return "screen";
  if (strcmp(path,"/config/dns")==0) return "wifi";
  if (strcmp(path,"/config/poll")==0) return "advanced";
  if (strncmp(path,"/config/",8)==0) return "lights";
  if (strcmp(path,"/network-diagnostics/run")==0 || strcmp(path,"/admin/password")==0 || strcmp(path,"/reboot")==0) return "advanced";
  return "overview";
}

size_t renderHelpPage(char *out,size_t capacity,const char *version,const char *target) {
  Writer w(out,capacity);
  w.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Help - Brooklyn Subway</title><link rel=\"stylesheet\" href=\"/ui.css?v=087\"></head><body><header><div class=\"brand\"><span class=\"brand-mark\" aria-hidden=\"true\">BK</span><div><h1>Brooklyn Subway</h1><p class=\"subtitle\">Specially Designed for Avi Yao</p></div></div></header><nav class=\"nav\" aria-label=\"Map settings\"><a href=\"/#overview\">Overview</a><a href=\"/#lights\">Train lights</a><a href=\"/#screen\">Screen</a><a href=\"/#wifi\">Wi-Fi</a><a href=\"/#advanced\">Advanced</a><a href=\"/help\" aria-current=\"page\">Help</a></nav><main><section class=\"hero\"><p class=\"kicker\">Help &amp; instructions</p><h2>Get to know your map</h2><p>Read the guide, visit the project, or send Joel a report if something looks wrong.</p><div class=\"actions\"><a class=\"button\" href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/docs/AVI_USER_GUIDE.md\" target=\"_blank\" rel=\"noopener noreferrer\">Read the guide</a><a class=\"button secondary\" href=\"https://github.com/jgoldwein/BrooklynSubway\" target=\"_blank\" rel=\"noopener noreferrer\">Project on GitHub</a></div><p class=\"hint\">GitHub opens in a new tab and needs internet access. This Help page works on your local network.</p></section><section id=\"sounds\"><h2>Sounds and what they mean</h2><p>Startup readiness uses one low SYSTEM beep, two middle Wi-Fi beeps and three high MTA beeps, once per boot. Optional service alerts use three descending notes for loss and three ascending notes for return. Overview shows the last sound and its meaning.</p><p><a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/docs/AVI_USER_GUIDE.md#sounds-and-what-they-mean\" target=\"_blank\" rel=\"noopener noreferrer\">Read the sound guide</a></p><p class=\"hint\">The guide explains startup, readiness, save and update sounds, knob feedback, test beeps and muting.</p></section><section><h2>Ask Joel for help</h2><p>Make a support report, review it, and send it with a description of what happened. Nothing is sent automatically.</p><a class=\"button\" href=\"/support\">Make a support report</a></section><section id=\"about\"><h2>About</h2><p>Brooklyn Subway is a connected subway map specially designed for Avi Yao, from the Goldweins.</p><p>It uses MTA train reports to estimate where trains are. Reports can be delayed or uncertain. This is an independent project, not an official MTA product.</p><dl><dt>Firmware version</dt><dd>");
  w.html(version);w.append("</dd><dt>Device</dt><dd>");w.html(target);
  w.append("</dd></dl><p>Copyright 2026 Joel W. Goldwein for original project material.</p></section><section class=\"wide\" id=\"licenses\"><h2>Licenses</h2><p>The original project material uses these licenses:</p><ul><li>Firmware and software: <a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/LICENSES/Apache-2.0.txt\" target=\"_blank\" rel=\"noopener noreferrer\">Apache License 2.0</a>.</li><li>Hardware designs: <a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/LICENSES/CERN-OHL-P-2.0.txt\" target=\"_blank\" rel=\"noopener noreferrer\">CERN Open Hardware Licence, permissive version 2</a>.</li><li>Documentation: <a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/LICENSES/CC-BY-4.0.txt\" target=\"_blank\" rel=\"noopener noreferrer\">Creative Commons Attribution 4.0</a>.</li></ul><p>Third-party software, fonts, MTA data, artwork and trademarks keep their own terms. Adafruit NeoPixel uses LGPL-3.0-or-later. U8g2 code uses BSD-2-Clause; its fonts have separate terms.</p><p><a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/THIRD_PARTY_NOTICES.md\" target=\"_blank\" rel=\"noopener noreferrer\">Third-party notices</a> &middot; <a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/LICENSE\" target=\"_blank\" rel=\"noopener noreferrer\">Project license overview</a> &middot; <a href=\"https://github.com/olikraus/u8g2/blob/2.36.18/LICENSE\" target=\"_blank\" rel=\"noopener noreferrer\">U8g2 and font license notices</a></p><p class=\"hint\">Full license documents open online. This summary does not replace their terms.</p></section></main></body></html>");
  return w.size();
}

size_t renderDefaultsPage(char *out,size_t capacity,const char *csrf) {
  if (!csrf || !*csrf) return 0;
  Writer w(out,capacity);
  w.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Firmware defaults - Brooklyn Subway</title><link rel=\"stylesheet\" href=\"/ui.css\"></head><body><main><section class=\"card\"><h1>Return to firmware defaults</h1><p>Start over or prepare the map for a new owner.</p><p><strong>This erases:</strong> saved Wi-Fi and its password, the administrator password, and all customized settings, including colors, brightness, quiet hours and automatic update preferences.</p><p>The installed firmware stays. After restarting, connect to <strong>BrooklynMap-Avi</strong> and open <strong>192.168.4.1</strong>. Create a new administrator password and choose your Wi-Fi.</p><p>You cannot undo this reset. Keep power connected until the setup network appears.</p><form method=\"post\" action=\"/defaults\"><input type=\"hidden\" name=\"csrf\" value=\"");
  w.html(csrf);
  w.append("\"><p><label><input type=\"checkbox\" name=\"confirm\" value=\"erase\" required> I understand that Wi-Fi, the administrator password and saved settings will be erased.</label></p><button type=\"submit\">Erase settings and restart</button></form><p><a href=\"/#advanced\">Cancel and keep settings</a></p></section></main></body></html>");
  return w.size();
}

size_t renderScreenOptionsPage(char *out,size_t capacity,uint64_t preferences,bool quietNow,const char *csrf) {
  if(!config::validScreenPreferences(preferences) || !csrf || !*csrf) return 0;
  const auto p=config::unpackScreenPreferences(preferences);Writer w(out,capacity);
  w.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Brooklyn Subway</title><link rel=\"stylesheet\" href=\"/ui.css?v=038\"></head><body><header><div class=\"brand\"><span class=\"brand-mark\">BK</span><div><h1>Brooklyn Subway</h1><p class=\"subtitle\">Specially Designed for Avi Yao</p></div></div></header><main class=\"login-shell\"><section><h2>Quiet hours and startup greeting</h2>");
  w.append(quietNow?"<p>Quiet hours are active. Beeps, OLED and map LEDs follow the settings below.</p>":"<p>Quiet hours are not active.</p>");
  w.append("<form method=\"post\" action=\"/config/screen-options\"><input type=\"hidden\" name=\"csrf\" value=\"");w.html(csrf);
  w.format("\"><label><input type=\"checkbox\" name=\"startup\" value=\"1\"%s>Show the dedication at startup</label>",p.startupDedication?" checked":"");
  w.format("<label><input type=\"checkbox\" name=\"waitWifi\" value=\"1\"%s>Keep it until Wi-Fi connects</label><p>The greeting stays for at least 5 seconds. Uncheck the Wi-Fi option for a brief greeting. Turn the knob to skip; Wi-Fi setup and POWER take priority.</p>",p.startupUntilWifi?" checked":"");
  w.format("<h3>Service sounds</h3><label><input type=\"checkbox\" name=\"serviceSounds\" value=\"1\"%s>Play service lost and restored sounds</label><p>Off by default. After service has worked, three descending notes announce a continuous 60-second outage. Three ascending notes announce its return, only if a loss sound played. Brief feed problems stay quiet. Quiet hours mute both; no missed sounds play later.</p>",p.serviceSounds?" checked":"");
  w.format("<h3>Quiet hours</h3><label><input type=\"checkbox\" name=\"quiet\" value=\"1\"%s>Enable quiet hours</label>",p.quietEnabled?" checked":"");
  w.format("<label>Start <input name=\"start\" type=\"time\" value=\"%02u:%02u\" required></label><label>End <input name=\"end\" type=\"time\" value=\"%02u:%02u\" required></label>",p.startMinute/60,p.startMinute%60,p.endMinute/60,p.endMinute%60);
  w.format("<label>Quiet OLED brightness (0-255)<input name=\"contrast\" type=\"number\" min=\"0\" max=\"255\" value=\"%u\" required></label>",p.quietContrast);
  w.append("<label>Map LEDs during quiet hours<select name=\"mapMode\">");
  w.format("<option value=\"off\"%s>Off</option><option value=\"dim\"%s>Dim to a brightness limit</option><option value=\"normal\"%s>Keep normal brightness</option></select></label>",p.quietMapBrightness==0?" selected":"",p.quietMapBrightness>0 && p.quietMapBrightness<255?" selected":"",p.quietMapBrightness==255?" selected":"");
  w.format("<label>Map brightness limit when Dim is selected (1-16)<input name=\"mapBrightness\" type=\"number\" min=\"1\" max=\"16\" value=\"%u\" required></label>",p.quietMapBrightness>0 && p.quietMapBrightness<255?p.quietMapBrightness:1);
  w.append("<p>Times use New York local time, including daylight saving. Overnight schedules are supported. Quiet hours mute all beeps and cap OLED brightness; map LEDs can be off, dimmed or unchanged. Normal brightness is preserved; POWER-off stays off. 0 is the dimmest OLED setting, not off. Quiet hours begin following the schedule once the clock synchronizes. Until then, normal brightness and sounds apply.</p><button>Save settings</button></form><p><a href=\"/#screen\">Back to Screen</a></p></section></main></body></html>");
  return w.size();
}

size_t renderLoginPage(char *out, size_t capacity, const char *csrfToken, const char *target) {
  if (!loginReturnPath(target)) return 0;
  if (csrfToken == nullptr || csrfToken[0] == '\0') return 0;
  Writer writer(out, capacity);
  writer.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<meta name=\"referrer\" content=\"same-origin\"><title>Brooklyn Subway</title>"
                "<link rel=\"stylesheet\" href=\"/ui.css?v=038\"></head><body>"
                "<header><div class=\"brand\"><span class=\"brand-mark\" aria-hidden=\"true\">BK</span>"
                "<div><h1>Brooklyn Subway</h1><p class=\"subtitle\">Specially Designed for Avi Yao</p></div></div><a href=\"/\">Back to map</a></header>"
                "<main class=\"login-shell\"><section class=\"wide\"><p class=\"kicker\">Settings access</p><h2>Sign in</h2>"
                "<p>Use your map's administrator password to change its settings.</p>"
                "<form method=\"post\" action=\"/login\"><input type=\"hidden\" name=\"csrf\" value=\"");
  writer.html(csrfToken);
  writer.append("\"><label>Password <input type=\"password\" name=\"password\" "
                "autocomplete=\"current-password\" required></label>");
  writer.append(kPasswordVisibilityControl);
  writer.append("<input type=\"hidden\" name=\"next\" value=\"");
  writer.html(target ? target : "");
  writer.append("\"><button type=\"submit\">Sign in</button></form>"
                "<p class=\"hint\">Use only on Wi-Fi you trust. Passwords travel over unencrypted HTTP.</p>"
                "</section></main></body></html>");
  return writer.size();
}


namespace {
void monitorString(Writer &w,const char *value) { w.append("\"");w.json(value);w.append("\""); }
uint32_t monitorLatestCount(const StatusSnapshot &s) {
  uint32_t count=0;for(uint8_t route=1;route<=semantic::kRouteCount;++route)count+=s.blueReport.monitoring[route].mapped;return count;
}
uint16_t monitorFeedMask(const char *name) {
  for (uint8_t i=0;i<network::kFeedCount;++i) if(strcmp(network::kFeeds[i].name,name)==0) return uint16_t(1u<<i);
  return 0;
}
const char *monitorFeedState(const StatusSnapshot &s,semantic::Route route) {
  if(!s.hasCycle)return "Waiting";
  using R=semantic::Route;uint16_t required=0;
  switch(route) {
    case R::Route2: case R::Route3: case R::Route4: case R::Route5: required=monitorFeedMask("1-7,S");break;
    case R::RouteA: case R::RouteC: required=monitorFeedMask("ACE");break;
    case R::RouteB: case R::RouteD: case R::RouteF: case R::RouteM: required=monitorFeedMask("BDFM");break;
    case R::RouteG: required=monitorFeedMask("G");break;
    case R::RouteJ: case R::RouteZ: required=monitorFeedMask("JZ");break;
    case R::RouteL: required=monitorFeedMask("L");break;
    case R::RouteN: case R::RouteQ: case R::RouteR: required=monitorFeedMask("NQRW");break;
    case R::RouteS: required=monitorFeedMask("ACE")|monitorFeedMask("G");break;
    default: return "Unknown";
  }
  const uint16_t received=s.processedFeedMask & required;
  return received==required?"Received":received?"Partial":"Missing";
}
const char *monitorHealth(const StatusSnapshot &s) {
  if (s.fetchingFeeds) return "Fetching train data. The map continues showing the last update.";
  if (!s.blueReport.timestamp) return "Waiting for the first train poll.";
  if (s.hasCycle && s.feedsSucceeded<s.feedsTotal) return "Some feeds are missing or unreadable. The map may hold an older update.";
  if (!s.unixTime) return "Waiting for clock sync; source age is unavailable.";
  if (s.blueReport.newestFeedTimestamp>s.unixTime && s.blueReport.newestFeedTimestamp-s.unixTime>30)
    return "A source timestamp is ahead of the clock. Source age needs review.";
  if (s.blueReport.capacityDrops) return "Some reports could not be processed. Review the counts below.";
  if (s.unixTime && s.blueReport.oldestFeedTimestamp && s.unixTime>s.blueReport.oldestFeedTimestamp && s.unixTime-s.blueReport.oldestFeedTimestamp>90)
    return "A source feed is over 90 seconds old, even if its download succeeded.";
  return "Latest poll received. Estimated and uncertain reports are listed separately.";
}
void monitorAge(Writer &w,uint32_t now,uint32_t then) {
  if (!now || !then || (then>now && then-now>30)) w.append("null");
  else w.format("%lu",static_cast<unsigned long>(now>=then?now-then:0));
}
void monitorRows(Writer &w,const StatusSnapshot &s) {
  for(uint8_t route=1;route<=semantic::kRouteCount;++route) {
    const auto id=static_cast<semantic::Route>(route);const auto &r=s.blueReport.monitoring[route];
    w.append("<tr><th scope=\"row\">");w.html(semantic::routeName(id));
    if(!semantic::routeVisible(effectiveRouteMask(s),id))w.append(" (hidden)");
    w.format("</th><td>%u</td><td>%u</td><td>%u</td><td>%u</td><td>%s</td></tr>",r.mapped,r.moving,r.estimated,r.uncertain,monitorFeedState(s,id));
  }
}
}
namespace {
constexpr const char *kRuntimeSection=R"HTML(<section id="runtime"><h2>Connection and restart details</h2><p id="runtime-state" role="status">Loading...</p><dl><dt>Board ID</dt><dd id="runtime-device"></dd><dt>Boot ID</dt><dd id="runtime-boot"></dd><dt>Free internal memory / largest block</dt><dd id="runtime-memory"></dd><dt>Minimum free task stacks: feeds / web / lights</dt><dd id="runtime-stacks"></dd><dt>Previous run checkpoint</dt><dd id="runtime-previous"></dd></dl><p class="hint">Restart history may be cleared when power is removed. A checkpoint records the last saved uptime and memory, not a crash backtrace. A missing TLS error does not prove TLS succeeded.</p><h3>Recent feed failures</h3><ul id="runtime-failures"></ul><p id="runtime-refresh" class="hint"></p><a href="/api/runtime-diagnostics" download="brooklyn-runtime-diagnostics.json">Download connection and restart report</a></section><script>(()=>{const by=id=>document.getElementById(id),set=(id,v)=>by(id).textContent=v;async function refresh(){let timer;try{if(!document.hidden){const c=new AbortController();timer=setTimeout(()=>c.abort(),8000);const r=await fetch('/api/runtime-diagnostics',{cache:'no-store',signal:c.signal});if(!r.ok)throw Error();const s=await r.json();set('runtime-state','Last restart: '+s.resetReason+' | Unexpected warm resets: '+s.unexpectedWarmResets);set('runtime-device',s.deviceId);set('runtime-boot',s.bootId);set('runtime-memory',s.freeInternalHeap+' / '+s.largestInternalBlock+' bytes');set('runtime-stacks',s.workerStackMinimumBytes+' / '+s.loopStackMinimumBytes+' / '+s.liveStackMinimumBytes+' bytes');set('runtime-previous',s.previousCheckpointAvailable?Math.floor(s.previousUptimeMs/1000)+' seconds uptime; '+s.previousFreeHeap+' bytes free':'No retained checkpoint available');const items=document.createDocumentFragment();s.failures.forEach(e=>{const li=document.createElement('li');li.textContent=e.feed+': '+e.category+'; HTTP '+e.httpCode+'; TLS '+(e.tlsError===null?'not reported':e.tlsError)+'; before attempt '+e.heapBefore+' bytes free, largest '+e.largestBefore+'; before cleanup '+e.heapBeforeClose+' free, largest '+e.largestBeforeClose;items.append(li)});if(!s.failures.length){const li=document.createElement('li');li.textContent='No feed failures recorded since restart.';items.append(li)}by('runtime-failures').replaceChildren(items);set('runtime-refresh','Updated '+new Date().toLocaleTimeString());}}catch(e){set('runtime-refresh','Runtime details refresh delayed; retrying.')}finally{clearTimeout(timer);setTimeout(refresh,10000)}}setTimeout(refresh,750)})();</script>)HTML";
}

namespace {
constexpr const char *kLiveIndicationSection=R"HTML(<section aria-labelledby="lights-title"><h2 id="lights-title">What the map lights are doing</h2>
<p id="light-counts" role="status">Loading live light counts...</p>
<p class="hint">Counts follow your color settings: approaching, stopped, leaving and uncertain. These are nonzero colors successfully sent to map LEDs, not a physical light sensor. A shared light can represent several trains.</p>
<h3>Stopped lights now</h3><ul id="active-stops"><li>Loading...</li></ul><p id="active-note" class="hint"></p>
<h3>Why stopped lights changed</h3><p class="hint">Recent completed indications, newest first. A change is not proof a train departed. Times cover one continuous light, even if several trains share it. History is held in memory since restart; the last 64 endings are retained.</p>
<div style="overflow:auto"><table><thead><tr><th scope="col">Station</th><th scope="col">Lasted</th><th scope="col">Why it ended</th></tr></thead><tbody id="stop-history"></tbody></table></div>
<p id="light-refresh" class="hint">Loading indication history...</p><p><a href="/api/indications" download="brooklyn-light-history.json">Download light snapshot</a></p>
</section><script>(()=>{const by=id=>document.getElementById(id),seconds=ms=>(ms/1000).toFixed(1)+' s',location=e=>e.line+' '+e.station+' (LED'+e.led+')';async function update(){let timer;try{if(!document.hidden){const controller=new AbortController();timer=setTimeout(()=>controller.abort(),8000);const r=await fetch('/api/indications',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error();const s=await r.json(),c=s.counts;by('light-counts').textContent=!s.available?'Live light monitoring is unavailable.':!s.outputEnabled?'Map lights are off.':'Approaching: '+c.approaching+' | Stopped: '+c.stopped+' | Leaving: '+c.leaving+' | Uncertain: '+c.uncertain;const active=document.createDocumentFragment();s.active.forEach(e=>{const li=document.createElement('li');li.textContent=location(e)+' - '+seconds(e.durationMs);active.append(li)});if(!s.active.length){const li=document.createElement('li');li.textContent='No stopped lights currently shown.';active.append(li)}by('active-stops').replaceChildren(active);by('active-note').textContent=s.activeShown<s.activeTotal?'Showing '+s.activeShown+' of '+s.activeTotal+' stopped lights.':'';const rows=document.createDocumentFragment();[...s.events].reverse().forEach(e=>{const tr=document.createElement('tr');[location(e),seconds(e.durationMs),e.reason].forEach(v=>{const td=document.createElement('td');td.textContent=v;tr.append(td)});rows.append(tr)});if(!s.events.length){const tr=document.createElement('tr'),td=document.createElement('td');td.colSpan=3;td.textContent='No stopped-light endings recorded yet.';tr.append(td);rows.append(tr)}by('stop-history').replaceChildren(rows);by('light-refresh').textContent=s.sampleAgeMs>5000&&s.outputEnabled?'LED output sample is delayed.':'Updated '+new Date().toLocaleTimeString()+' - recent '+s.events.length+' endings.';}}catch(e){by('light-refresh').textContent='Light history refresh delayed; retrying.';}finally{clearTimeout(timer);setTimeout(update,2000)}}setTimeout(update,500)})();</script>)HTML";
}

bool parseIndicationCursor(const char *text,uint32_t &after) {
  if(!text || !*text)return false;
  uint32_t value=0;
  for(const char *p=text;*p;++p) {
    if(*p<'0'||*p>'9')return false;
    const uint32_t digit=*p-'0';if(value>(UINT32_MAX-digit)/10)return false;
    value=value*10+digit;
  }
  after=value;return true;
}
namespace {
void indicationCounts(Writer &w,const display::IndicationCounts &c) {
  w.format("{\"approaching\":%u,\"stopped\":%u,\"leaving\":%u,\"uncertain\":%u}",c.approaching,c.stopped,c.leaving,c.uncertain);
}
void indicationLocation(Writer &w,uint16_t led) {
  const auto *m=semantic::semanticForLed(led);
  w.format("\"index\":%u,\"led\":%u,\"line\":",led,m?m->designator:0);
  monitorString(w,m?semantic::routeName(m->route):"");w.append(",\"station\":");
  const char *name=m?semantic::stationName(m->stationId):nullptr;monitorString(w,name?name:"");
}
}
size_t renderRuntimeJson(char *out,size_t capacity,const observation::RuntimeReport &r) {
  Writer w(out,capacity);const auto &b=r.boot;
  w.format("{\"schemaVersion\":1,\"deviceId\":\"%08lx%08lx\",\"bootId\":\"%08lx%08lx\",\"resetReason\":",
      static_cast<unsigned long>(r.deviceId>>32),static_cast<unsigned long>(r.deviceId&0xffffffffu),static_cast<unsigned long>(b.idHigh),static_cast<unsigned long>(b.idLow));
  monitorString(w,r.resetReason);
  w.format(",\"uptimeMs\":%lu,\"warmSessionBoots\":%lu,\"unexpectedWarmResets\":%lu,\"previousBootAvailable\":%s,\"previousCheckpointAvailable\":%s,\"previousBootId\":\"%08lx%08lx\",\"previousUptimeMs\":%lu,\"previousFreeHeap\":%lu,\"previousMinimumHeap\":%lu,",
      static_cast<unsigned long>(r.uptimeMs),static_cast<unsigned long>(b.boots),static_cast<unsigned long>(b.unexpected),b.previousAvailable?"true":"false",b.previousCheckpointAvailable?"true":"false",static_cast<unsigned long>(b.previousHigh),static_cast<unsigned long>(b.previousLow),static_cast<unsigned long>(b.previousUptimeMs),static_cast<unsigned long>(b.previousFreeHeap),static_cast<unsigned long>(b.previousMinHeap));
  w.format("\"freeInternalHeap\":%lu,\"minimumInternalHeap\":%lu,\"largestInternalBlock\":%lu,\"workerStackMinimumBytes\":%lu,\"loopStackMinimumBytes\":%lu,\"liveStackMinimumBytes\":%lu,\"newestFailureSequence\":%lu,\"oldestFailureSequence\":%lu,\"failures\":[",
      static_cast<unsigned long>(r.freeHeap),static_cast<unsigned long>(r.minimumHeap),static_cast<unsigned long>(r.largestBlock),static_cast<unsigned long>(r.workerStackMinimumBytes),static_cast<unsigned long>(r.loopStackMinimumBytes),static_cast<unsigned long>(r.liveStackMinimumBytes),static_cast<unsigned long>(r.newestSequence),static_cast<unsigned long>(r.count?r.failures[(r.head+observation::kFailureHistory-r.count)%observation::kFailureHistory].sequence:0));
  for(uint8_t n=0;n<r.count;++n) {
    if(n)w.append(",");const auto &e=r.failures[(r.head+observation::kFailureHistory-r.count+n)%observation::kFailureHistory];
    const auto &f=e.observation.failure;const auto &d=e.observation.details;
    w.format("{\"sequence\":%lu,\"feed\":",static_cast<unsigned long>(e.sequence));monitorString(w,f.name?f.name:"");
    w.append(",\"category\":");monitorString(w,transport::downloadStatusName(f.status));
    w.format(",\"bytes\":%lu,\"elapsedMs\":%lu,\"captured\":%s,\"httpCode\":%ld,\"tlsError\":",
        static_cast<unsigned long>(f.bytesReceived),static_cast<unsigned long>(f.elapsedMs),d.captured?"true":"false",static_cast<long>(d.httpCode));
    if(d.tlsError<0)w.format("%ld",static_cast<long>(d.tlsError));else w.append("null");
    w.format(",\"startedAtUptimeMs\":%lu,\"unixTime\":%lu,\"heapBefore\":%lu,\"largestBefore\":%lu,\"heapBeforeClose\":%lu,\"largestBeforeClose\":%lu,\"minimumHeap\":%lu}",
        static_cast<unsigned long>(d.startedAtMs),static_cast<unsigned long>(d.unixTime),static_cast<unsigned long>(d.heapBefore),static_cast<unsigned long>(d.largestBefore),static_cast<unsigned long>(d.heapBeforeClose),static_cast<unsigned long>(d.largestBeforeClose),static_cast<unsigned long>(d.minimumHeap));
  }
  w.append("]}");return w.size();
}

size_t renderIndicationJson(char *out,size_t capacity,const display::IndicationReport &r) {
  Writer w(out,capacity);
  w.format("{\"schemaVersion\":1,\"stoppedPolicy\":\"fresh-evidence\",\"available\":%s,\"outputEnabled\":%s,\"uptimeMs\":%lu,\"unixTime\":%lu,\"sampleAgeMs\":%lu,\"counts\":",
      r.available?"true":"false",r.outputEnabled?"true":"false",static_cast<unsigned long>(r.nowMs),static_cast<unsigned long>(r.observedUnix),static_cast<unsigned long>(r.nowMs-r.observedAtMs));
  indicationCounts(w,r.counts);
  w.format(",\"newestSequence\":%lu,\"oldestSequence\":%lu,\"nextAfter\":%lu,\"historyGap\":%s,\"more\":%s,\"activeTotal\":%u,\"activeShown\":%u,\"active\":[",
      static_cast<unsigned long>(r.newestSequence),static_cast<unsigned long>(r.oldestSequence),static_cast<unsigned long>(r.nextAfter),r.gap?"true":"false",r.more?"true":"false",r.activeTotal,r.activeCount);
  for(uint8_t i=0;i<r.activeCount && i<display::kActiveIndicationPage;++i) {
    const auto &a=r.active[i];if(i)w.append(",");w.append("{");indicationLocation(w,a.led);
    w.format(",\"startedAtUptimeMs\":%lu,\"durationMs\":%lu}",static_cast<unsigned long>(a.startedAtMs),static_cast<unsigned long>(a.durationMs));
  }
  w.append("],\"events\":[");
  for(uint8_t i=0;i<r.eventCount && i<display::kIndicationHistoryPage;++i) {
    const auto &e=r.events[i];if(i)w.append(",");w.format("{\"sequence\":%lu,",static_cast<unsigned long>(e.sequence));indicationLocation(w,e.led);
    w.format(",\"startedAtUptimeMs\":%lu,\"endedAtUptimeMs\":%lu,\"durationMs\":%lu,\"endedUnix\":%lu,\"reasonCode\":%u,\"reason\":",
        static_cast<unsigned long>(e.startedAtMs),static_cast<unsigned long>(e.endedAtMs),static_cast<unsigned long>(uint32_t(e.endedAtMs-e.startedAtMs)),static_cast<unsigned long>(e.endedUnix),unsigned(e.reason));
    monitorString(w,display::indicationEndName(e.reason));w.append(",\"next\":");monitorString(w,display::indicationName(e.next));w.append("}");
  }
  w.append("]}");return w.size();
}

size_t renderTrainMonitorJson(char *out,size_t capacity,const StatusSnapshot &s) {
  Writer w(out,capacity);const auto &r=s.blueReport;
  w.format("{\"reportTimestamp\":%lu,\"mappedTrains\":%lu,\"occupiedMapLeds\":%u,\"visibleMapLeds\":%u,\"darkOccupiedMapLeds\":%u,\"outputEnabled\":%s,\"feedsSucceeded\":%u,\"feedsTotal\":%u,\"sourceOldestUnix\":%lu,\"sourceNewestUnix\":%lu,\"sourceAgeSeconds\":",
      static_cast<unsigned long>(r.timestamp),static_cast<unsigned long>(s.resolvedTrains),s.occupiedLeds,s.visibleMapLeds,s.darkOccupiedMapLeds,s.physicalLedOutputEnabled?"true":"false",s.feedsSucceeded,s.feedsTotal,static_cast<unsigned long>(r.oldestFeedTimestamp),static_cast<unsigned long>(r.newestFeedTimestamp));
  monitorAge(w,s.unixTime,r.oldestFeedTimestamp);
  w.format(",\"latestPollMappedTrains\":%lu",static_cast<unsigned long>(monitorLatestCount(s)));w.append(",\"reportAgeSeconds\":");monitorAge(w,s.unixTime,r.timestamp);
  w.append(",\"mapAgeSeconds\":");if(s.hasDataAge)w.format("%lu",static_cast<unsigned long>(s.dataAgeSeconds));else w.append("null");
  w.format(",\"fetchingFeeds\":%s",s.fetchingFeeds?"true":"false");
  w.append(",\"stoppedPolicy\":\"fresh-evidence\",\"stoppedTimeLimitEnabled\":false");
  w.append(",\"indications\":");indicationCounts(w,s.indications);
  w.append(",\"health\":");monitorString(w,monitorHealth(s));
  w.format(",\"staleVehicles\":%lu,\"unmappedPositions\":%lu,\"conflictedTrips\":%lu,\"predictionConflicts\":%lu,\"capacityDrops\":%lu,\"uncertainMode\":",
      static_cast<unsigned long>(r.staleVehicles),static_cast<unsigned long>(r.unmappedPositions),static_cast<unsigned long>(r.conflictedTrips),static_cast<unsigned long>(r.predictionConflicts),static_cast<unsigned long>(r.capacityDrops));
  const auto mode=semantic::unknownTimingMode(s.unknownTiming);
  monitorString(w,mode==semantic::UnknownTimingMode::LastReported?"Last reported":mode==semantic::UnknownTimingMode::Hidden?"Hidden":"Steady color");
  w.format(",\"fadeSeconds\":%u,\"uncertainBrightness\":%u,\"routes\":[",semantic::lastReportedFadeSeconds(s.unknownTiming),unsigned(s.unknownTiming&255));
  for(uint8_t route=1;route<=semantic::kRouteCount;++route) {
    if(route>1)w.append(",");const auto id=static_cast<semantic::Route>(route);const auto &q=r.monitoring[route];
    w.append("{\"line\":");monitorString(w,semantic::routeName(id));
    w.format(",\"shown\":%s,\"mapped\":%u,\"moving\":%u,\"estimated\":%u,\"uncertain\":%u,\"feedState\":\"%s\"}",semantic::routeVisible(effectiveRouteMask(s),id)?"true":"false",q.mapped,q.moving,q.estimated,q.uncertain,monitorFeedState(s,id));
  }
  w.format("],\"decodeFailures\":%lu,\"lastFeedFailure\":",static_cast<unsigned long>(r.decodeFailures));
  if(s.hasLastFeedFailure) {w.append("{\"feed\":");monitorString(w,s.lastFeedFailureName);w.append(",\"reason\":");monitorString(w,s.lastFeedFailureCategory);w.append("}");}else w.append("null");
  w.append("}");return w.size();
}
size_t renderTrainMonitorPage(char *out,size_t capacity,const StatusSnapshot &s) {
  Writer w(out,capacity);
  w.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Brooklyn Subway</title><link rel=\"stylesheet\" href=\"/ui.css?v=038\"><style>td,th{padding:6px;text-align:left;border-bottom:1px solid #dce4ef}table{width:100%;border-collapse:collapse}dl{max-width:42rem}@media(max-width:600px){td,th{padding:6px 2px;font-size:13px}}</style></head><body><header><div><h1>Brooklyn Subway</h1><p class=\"subtitle\">Specially Designed for Avi Yao</p></div><a href=\"/\">Back to map</a></header><main style=\"display:block\"><section><h2>Train monitor</h2><p id=\"health\" role=\"status\">");
  w.html(monitorHealth(s));
  w.format("</p><p><strong id=\"mapped\">%lu</strong> mapped trains &middot; <strong id=\"lit\">%u</strong> lit map LEDs</p>",static_cast<unsigned long>(s.resolvedTrains),s.visibleMapLeds);
  w.append("<p class=\"hint\">Several trains can share one light. Uncertain reports can fade or be hidden. Very faint lights may be hard to see. Lit counts nonzero colors sent to map LEDs, excluding status lights.</p><dl><dt>Occupied map positions</dt><dd id=\"occupied\">");
  w.format("%u</dd><dt>Occupied positions now dark</dt><dd id=\"dark\">%u</dd><dt>Feeds received</dt><dd id=\"feeds\">%u / %u</dd>",s.occupiedLeds,s.darkOccupiedMapLeds,s.feedsSucceeded,s.feedsTotal);
  w.append("<dt>Last usable map update</dt><dd id=\"mapAge\">");if(s.hasDataAge)w.format("%lu seconds",static_cast<unsigned long>(s.dataAgeSeconds));else w.append("Waiting");
  w.append("</dd><dt>Oldest source feed age</dt><dd id=\"sourceAge\">Refresh pending</dd><dt>Latest poll age</dt><dd id=\"reportAge\">Refresh pending</dd><dt>Uncertain-light setting</dt><dd id=\"policy\">");
  const auto mode=semantic::unknownTimingMode(s.unknownTiming);
  if(mode==semantic::UnknownTimingMode::LastReported)w.format("Last reported: fades after %u seconds, at most 2/255 brightness",semantic::lastReportedFadeSeconds(s.unknownTiming));
  else w.append(mode==semantic::UnknownTimingMode::Hidden?"Hidden":"Steady color");
  w.format("</dd><dt>Old vehicle reports ignored</dt><dd id=\"stale\">%lu</dd><dt>Positions outside map or unresolved</dt><dd id=\"unmapped\">%lu</dd><dt>Conflicting trips ignored</dt><dd id=\"conflicts\">%lu</dd><dt>Conflicting prediction sets</dt><dd id=\"predictions\">%lu</dd><dt>Processing capacity problems</dt><dd id=\"capacity\">%lu</dd><dt>Unreadable feeds</dt><dd id=\"decode\">%lu</dd><dt>Last recorded feed error</dt><dd id=\"failure\">",static_cast<unsigned long>(s.blueReport.staleVehicles),static_cast<unsigned long>(s.blueReport.unmappedPositions),static_cast<unsigned long>(s.blueReport.conflictedTrips),static_cast<unsigned long>(s.blueReport.predictionConflicts),static_cast<unsigned long>(s.blueReport.capacityDrops),static_cast<unsigned long>(s.blueReport.decodeFailures));
  if(s.hasLastFeedFailure){w.html(s.lastFeedFailureName);w.append(": ");w.html(s.lastFeedFailureCategory);}else w.append("None recorded");
  w.append("</dd></dl><p><a href=\"/#lights\">Change uncertain-light settings</a> &middot; <a href=\"/api/monitor\" download=\"brooklyn-train-monitor.json\">Download report</a></p><h3>Latest poll, by line</h3><p>Latest poll total: <strong id=\"pollCount\">");
  w.format("%lu</strong> mapped train reports.</p>",static_cast<unsigned long>(monitorLatestCount(s)));
  w.append("<p class=\"hint\">Counts are before fading and route filters. Est. means an estimated departure; Unsure means an unclear report. Columns overlap. The map may hold an older update. Zero does not prove no trains are running: check Feed. Missing or Partial means some data was unavailable.</p><div style=\"overflow:auto\"><table><thead><tr><th scope=\"col\">Line</th><th scope=\"col\">Trains</th><th scope=\"col\">Moving</th><th scope=\"col\"><abbr title=\"Estimated departures\">Est.</abbr></th><th scope=\"col\">Unsure</th><th scope=\"col\">Feed</th></tr></thead><tbody id=\"lines\">");
  monitorRows(w,s);
  w.append("</tbody></table></div><p id=\"refresh\" class=\"hint\">Live values refresh automatically.</p></section>");
  w.append(kLiveIndicationSection);
  w.append(kRuntimeSection);
  w.append("</main><script>(function(){const by=id=>document.getElementById(id);function text(id,v){by(id).textContent=v}const age=v=>v===null?'Not available':v+' seconds';async function refresh(){let timer;try{const controller=new AbortController();timer=setTimeout(()=>controller.abort(),15000);const r=await fetch('/api/monitor',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error();const s=await r.json();text('health',s.health);text('mapped',s.mappedTrains);text('pollCount',s.latestPollMappedTrains);text('lit',s.visibleMapLeds);text('occupied',s.occupiedMapLeds);text('dark',s.outputEnabled?s.darkOccupiedMapLeds:'Map lights are off');text('feeds',s.feedsSucceeded+' / '+s.feedsTotal);text('mapAge',age(s.mapAgeSeconds));text('sourceAge',age(s.sourceAgeSeconds));text('reportAge',age(s.reportAgeSeconds));text('policy',s.uncertainMode==='Last reported'?'Last reported: fades after '+s.fadeSeconds+' seconds, at most 2/255 brightness':s.uncertainMode+' (ceiling '+s.uncertainBrightness+'/255)');text('stale',s.staleVehicles);text('unmapped',s.unmappedPositions);text('conflicts',s.conflictedTrips);text('predictions',s.predictionConflicts);text('capacity',s.capacityDrops);text('decode',s.decodeFailures);text('failure',s.lastFeedFailure?s.lastFeedFailure.feed+': '+s.lastFeedFailure.reason:'None recorded');const rows=document.createDocumentFragment();s.routes.forEach(q=>{const tr=document.createElement('tr');[q.line+(q.shown?'':' (hidden)'),q.mapped,q.moving,q.estimated,q.uncertain,q.feedState].forEach((v,i)=>{const cell=document.createElement(i?'td':'th');if(!i)cell.scope='row';cell.textContent=v;tr.append(cell)});rows.append(tr)});by('lines').replaceChildren(rows);text('refresh','Updated '+new Date().toLocaleTimeString());}catch(e){text('refresh','Refresh delayed; values may be old. Retrying...')}finally{clearTimeout(timer);setTimeout(refresh,5000)}}setTimeout(refresh,500)})();</script></body></html>");
  return w.size();
}

size_t renderStatusJson(char *out, size_t capacity,
                        const StatusSnapshot &status, uint8_t brightness,
                        uint32_t routeVisibilityMask) {
  Writer writer(out, capacity);
  writer.append("{\"firmware\":\"");
  writer.append(kFirmwareId);
  writer.append("\",\"firmwareVersion\":\"");
  writer.json(status.firmwareVersion);
  writer.append("\",\"lastSound\":\"");writer.json(status.lastSound);
  writer.format("\",\"otaBootConfirmed\":%s", status.otaBootConfirmed ? "true" : "false");
  writer.append(",\"deviceName\":\"");
  writer.json(status.deviceName);
  writer.append("\",\"target\":\"");
  writer.json(status.target);
  writer.format("\",\"deviceId\":\"%08lx%08lx\",\"bootId\":\"%08lx%08lx\",\"largestInternalBlock\":%lu,\"unexpectedWarmResets\":%lu,\"indications\":",
      static_cast<unsigned long>(status.deviceId>>32),static_cast<unsigned long>(status.deviceId&0xffffffffu),static_cast<unsigned long>(status.boot.idHigh),static_cast<unsigned long>(status.boot.idLow),static_cast<unsigned long>(status.largestInternalBlock),static_cast<unsigned long>(status.boot.unexpected));
  indicationCounts(writer,status.indications);
  writer.append(",\"resetReason\":\"");
  writer.json(status.resetReason);
  writer.format("\",\"uptimeSeconds\":%lu,\"networkConnected\":%s,"
                "\"ssid\":\"",
                static_cast<unsigned long>(status.uptimeSeconds),
                status.networkConnected ? "true" : "false");
  writer.json(status.ssid);
  writer.format("\",\"fetchingFeeds\":%s,\"ip\":\"",status.fetchingFeeds?"true":"false");
  writer.json(status.ip);
  writer.append("\",\"subnetMask\":\"");
  writer.json(status.subnet);
  writer.append("\",\"gateway\":\"");
  writer.json(status.gateway);
  writer.append("\",\"dnsPrimary\":\"");
  writer.json(status.dnsPrimary);
  writer.append("\",\"dnsSecondary\":\"");
  writer.json(status.dnsSecondary);
  writer.append("\",\"dhcpDns1\":\"");
  writer.json(status.dnsPrimary);
  writer.append("\",\"dhcpDns2\":\"");
  writer.json(status.dnsSecondary);
  writer.append("\",\"manualDnsEnabled\":");
  writer.append(status.manualDnsEnabled ? "true" : "false");
  writer.append(",\"manualDnsApplied\":");
  writer.append(status.manualDnsApplied ? "true" : "false");
  writer.append(",\"effectiveDnsSource\":\"");
  writer.json(status.effectiveDnsSource);
  writer.append("\",\"manualDnsServer\":\"");
  writer.json(status.manualDnsServer);
  writer.append("\",\"effectiveDnsServer\":\"");
  writer.json(status.effectiveDnsServer);
  writer.append("\",\"dnsConfigStatus\":\"");
  writer.json(status.dnsConfigStatus);
  writer.format("\",\"adminConfigured\":%s,\"adminSessionActive\":%s,"
                "\"rssi\":%d,\"hostname\":\"%s\","
                "\"mdnsAvailable\":%s,\"timeSynchronized\":%s,"
                "\"displayState\":\"%s\",\"feedCycleState\":\"",
                status.adminConfigured ? "true" : "false",
                status.adminSessionActive ? "true" : "false",
                static_cast<int>(status.rssi), kHostname,
                status.mdnsAvailable ? "true" : "false",
                status.timeSynchronized ? "true" : "false",
                displayStateName(status.displayState));
  writer.json(status.hasCycle ? cycleStateName(status.cycleOutcome) : "NOT RUN");
  writer.format("\",\"unixTime\":%s",
                status.timeSynchronized ? "" : "null");
  if (status.timeSynchronized) {
    writer.format("%lu", static_cast<unsigned long>(status.unixTime));
  }
  writer.format(",\"feedsSucceeded\":%u,\"feedsTotal\":%u,"
                "\"dataAgeSeconds\":",
                status.feedsSucceeded, status.feedsTotal);
  appendNullableAge(writer, status.hasDataAge, status.dataAgeSeconds);
  writer.append(",\"completeAgeSeconds\":");
  appendNullableAge(writer, status.hasCompleteAge, status.completeAgeSeconds);
  writer.format(",\"approachingTrains\":%lu", static_cast<unsigned long>(status.approachingTrains));
  writer.format(",\"oledAddress\":%u", status.oledAddress);
  {
    char text[160]; renderOledStatus(text, sizeof(text), status.oledStatus);
    writer.append(",\"oledStatus\":\""); writer.json(text); writer.append("\"");
    writer.format(",\"oledActiveAddress\":%u,\"oledI2cError\":%u", status.oledStatus.activeAddress, status.oledStatus.error);
  }
  writer.format(",\"uncertaintyNotice\":\"%s\"",status.blueReport.uncertainLeds?"Some train movements are uncertain.":"");
  writer.format(",\"trainColors\":{\"approaching\":\"#%02X%02X%02X\",\"stopped\":\"#%02X%02X%02X\",\"leaving\":\"#%02X%02X%02X\",\"uncertain\":\"#%02X%02X%02X\"}",
      status.trainColors.approaching.red,status.trainColors.approaching.green,status.trainColors.approaching.blue,
      status.trainColors.stopped.red,status.trainColors.stopped.green,status.trainColors.stopped.blue,
      status.trainColors.leaving.red,status.trainColors.leaving.green,status.trainColors.leaving.blue,
      status.trainColors.uncertain.red,status.trainColors.uncertain.green,status.trainColors.uncertain.blue);
  writer.format(",\"unknownTimingEnabled\":%s,\"unknownTimingBrightness\":%u,\"unknownTimingMode\":\"%s\",\"lastReportedFadeSeconds\":%u",
      semantic::unknownTimingMode(status.unknownTiming)!=semantic::UnknownTimingMode::Hidden?"true":"false",status.unknownTiming&255,
      semantic::unknownTimingModeName(semantic::unknownTimingMode(status.unknownTiming)),semantic::lastReportedFadeSeconds(status.unknownTiming));
  writer.format(",\"oledContrast\":%u,\"oledReturnSeconds\":%u", status.oledContrast, status.oledReturnSeconds);
  const auto preferences=config::unpackScreenPreferences(status.screenPreferences);
  writer.format(",\"startupDedicationUntilWifi\":%s",preferences.startupUntilWifi?"true":"false");
  writer.format(",\"serviceSoundsEnabled\":%s",preferences.serviceSounds?"true":"false");
  writer.format(",\"quietHoursEnabled\":%s,\"quietHoursActive\":%s,\"startupDedicationEnabled\":%s,\"quietMapBrightness\":%u",preferences.quietEnabled?"true":"false",status.quietHoursActive?"true":"false",preferences.startupDedication?"true":"false",unsigned(preferences.quietMapBrightness));
  writer.format(",\"routeFocusActive\":%s,\"routeFocusRoute\":\"%s\",\"effectiveRouteMask\":%lu",
      status.routeFocusActive?"true":"false",status.routeFocusActive?semantic::routeName(status.focusedRoute):"",
      static_cast<unsigned long>(status.routeFocusActive?semantic::routeVisibilityBit(status.focusedRoute):(routeVisibilityMask & semantic::kAllRoutesVisibleMask)));
  writer.format(",\"startupIndicatorsActive\":%s",status.startupIndicatorsActive?"true":"false");
  writer.append(",\"stoppedPolicy\":\"fresh-evidence\",\"stoppedTimeLimitEnabled\":false");
  writer.format(",\"blueHoldSeconds\":%lu", static_cast<unsigned long>(status.blueHoldSeconds)); // legacy stored value, inactive
  writer.format(",\"pollSeconds\":%lu", static_cast<unsigned long>(status.pollSeconds));
  writer.format(",\"physicalFramesWritten\":%lu,\"physicalMapChanges\":%lu,"
                "\"physicalBlinkingLeds\":%u,\"physicalPowerPaused\":%s",
                static_cast<unsigned long>(status.physicalFramesWritten),
                static_cast<unsigned long>(status.physicalMapChanges),
                status.physicalBlinkingLeds, status.physicalPowerPaused ? "true" : "false");
  writer.format(",\"visibleMapLeds\":%u,\"darkOccupiedMapLeds\":%u",status.visibleMapLeds,status.darkOccupiedMapLeds);
  writer.format(",\"resolvedTrains\":%lu,\"occupiedLeds\":%u,"
                "\"collisionCount\":%u,\"brightness\":%u,"
                "\"routeVisibilityMask\":%lu,\"routeCount\":%u,"
                "\"physicalLedOutputEnabled\":%s,"
                "\"secureConnectFailures\":%lu,\"httpFailures\":%lu,"
                "\"transportTimeouts\":%lu,\"bytesDownloaded\":%llu,"
                "\"freeInternalHeap\":%lu,\"minimumInternalHeap\":%lu,"
                "\"freePsram\":",
                static_cast<unsigned long>(status.resolvedTrains),
                status.occupiedLeds, status.collisionCount, brightness,
                static_cast<unsigned long>(routeVisibilityMask &
                                           semantic::kAllRoutesVisibleMask),
                semantic::kRouteCount,
                status.physicalLedOutputEnabled ? "true" : "false",
                static_cast<unsigned long>(
                    status.transportCounters.secureConnectFailures),
                static_cast<unsigned long>(
                    status.transportCounters.httpFailures),
                static_cast<unsigned long>(status.transportCounters.timeouts),
                static_cast<unsigned long long>(
                    status.transportCounters.bytesDownloaded),
                static_cast<unsigned long>(status.freeInternalHeap),
                static_cast<unsigned long>(status.minimumInternalHeap));
  if (status.psramPresent) {
    writer.format("%lu,\"minimumFreePsram\":%lu",
                  static_cast<unsigned long>(status.freePsram),
                  static_cast<unsigned long>(status.minimumFreePsram));
  } else {
    writer.append("null,\"minimumFreePsram\":null");
  }
  writer.format(",\"psramPresent\":%s,\"psramFeedBufferPresent\":%s",
                status.psramPresent ? "true" : "false",
                status.psramFeedBufferPresent ? "true" : "false");
  if (status.hasLastFeedFailure) {
    writer.append(",\"lastFeedFailure\":{\"feed\":\"");
    writer.json(status.lastFeedFailureName);
    writer.append("\",\"category\":\"");
    writer.json(status.lastFeedFailureCategory);
    writer.format("\",\"bytes\":%lu,\"elapsedMs\":%lu}",
                  static_cast<unsigned long>(status.lastFeedFailureBytes),
                  static_cast<unsigned long>(
                      status.lastFeedFailureElapsedMs));
  } else {
    writer.append(",\"lastFeedFailure\":null");
  }
  {
    char summary[3072];
    const size_t length = renderBlueSummary(summary, sizeof(summary), status.blueReport);
    writer.append(",\"blueSummary\":\"");
    writer.json(length ? summary : "Diagnostic report unavailable");
    writer.append("\"");
  }
  writer.append("}");
  return writer.size();
}

size_t renderNetworkDiagnosticsJson(
    char *out, size_t capacity,
    const network::diagnostics::Snapshot &diagnostics) {
  Writer writer(out, capacity);
  writer.append("{\"state\":\"");
  writer.append(network::diagnostics::runStateName(diagnostics.state));
  writer.format("\",\"wifiAssociated\":%s,\"ssid\":\"",
                diagnostics.wifiAssociated ? "true" : "false");
  writer.json(diagnostics.ssid);
  writer.append("\",\"ip\":\"");
  writer.json(diagnostics.ip);
  writer.append("\",\"subnetMask\":\"");
  writer.json(diagnostics.subnet);
  writer.append("\",\"gateway\":\"");
  writer.json(diagnostics.gateway);
  writer.append("\",\"dnsPrimary\":\"");
  writer.json(diagnostics.dnsPrimary);
  writer.append("\",\"dnsSecondary\":\"");
  writer.json(diagnostics.dnsSecondary);
  writer.append("\",\"effectiveDns1\":\"");
  writer.json(diagnostics.dnsPrimary);
  writer.append("\",\"effectiveDns2\":\"");
  writer.json(diagnostics.dnsSecondary);
  writer.append("\",\"unixTime\":");
  if (diagnostics.hasUnixTime) {
    writer.format("%lu", static_cast<unsigned long>(diagnostics.unixTime));
  } else {
    writer.append("null");
  }
  writer.append(",\"ntpDns\":");
  appendCheckJson(writer, diagnostics.ntpDns);
  writer.append(",\"mtaDns\":");
  appendCheckJson(writer, diagnostics.mtaDns);
  writer.append(",\"mtaTcp443\":");
  appendCheckJson(writer, diagnostics.tcp443);
  writer.append(",\"directDns\":[");
  for (size_t i = 0; i < network::diagnostics::kDirectDnsCheckCount; ++i) {
    if (i != 0) writer.append(",");
    const network::diagnostics::DirectDnsResult &result =
        diagnostics.directDns[i];
    writer.append("{\"resolver\":\"");
    writer.json(result.resolver);
    writer.append("\",\"hostname\":\"");
    writer.json(result.hostname);
    writer.append("\",\"state\":\"");
    writer.append(network::diagnostics::checkStateName(result.state));
    writer.format("\",\"responseReceived\":%s,\"rcode\":",
                  result.responseReceived ? "true" : "false");
    if (result.hasRcode) {
      writer.format("%u", result.rcode);
    } else {
      writer.append("null");
    }
    writer.append(",\"addresses\":[");
    for (uint8_t address = 0; address < result.addressCount; ++address) {
      if (address != 0) writer.append(",");
      writer.append("\"");
      writer.json(result.addresses[address]);
      writer.append("\"");
    }
    writer.format("],\"elapsedMs\":%lu}",
                  static_cast<unsigned long>(result.elapsedMs));
  }
  writer.append("]");
  writer.append("}");
  return writer.size();
}

}  // namespace brooklyn::web
