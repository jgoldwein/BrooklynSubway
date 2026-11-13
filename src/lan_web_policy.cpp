// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"
#include "password_visibility.h"
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
  w.format("Latest poll: %lu (Unix). Station LEDs, visible routes; held/paused display may differ.\n",
           static_cast<unsigned long>(report.timestamp));
  w.format("Station indications: %u recent matching stop LEDs (blue), %u uncertain LEDs (steady dim amber).\n", report.steadyLeds, report.uncertainLeds);
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
           "Shared: LEDs with multiple trains; stopped reports override blinking.\n"
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
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  writer.append("<title>Brooklyn Active Subway Map</title><style>"
                "body{font:16px system-ui,sans-serif;line-height:1.45;max-width:52rem;"
                "margin:1rem auto;padding:0 1rem;color:#17212b}"
                "h1{font-size:1.65rem}.status{padding:.7rem;background:#eef3f6}"
                "section{margin:1.3rem 0}dl{display:grid;grid-template-columns:"
                "minmax(9rem,1fr) 2fr;gap:.35rem}dt{font-weight:600}"
                "dd{margin:0;overflow-wrap:anywhere}input,button{font:inherit;"
                "padding:.5rem}input[type=range]{width:100%}form{margin:.6rem 0}"
                "button{cursor:pointer}.muted{color:#52616b}details{margin-top:1rem}"
                ".route-list{display:grid;grid-template-columns:repeat(auto-fit,"
                "minmax(7rem,1fr));gap:.35rem}.route-controls{display:flex;gap:.5rem;"
                "flex-wrap:wrap}"
                "</style></head><body><h1>Brooklyn Active Subway Map</h1>"
                "<p class=\"status\">");
  writer.html(status.deviceName);
  writer.append(" - ");
  writer.append(status.networkConnected ? "Connected to LAN" : "LAN disconnected");
  writer.append(" | Display: ");
  writer.append(displayStateName(status.displayState));
  writer.append("</p><section><h2>Device</h2><dl><dt>Firmware</dt><dd>");
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
  writer.append("</dd></dl></section><section><h2>Administrator</h2><p>");
  if (!status.adminConfigured) {
    writer.append("Administrator setup is required through the local "
                  "BrooklynMap-Avi provisioning network.");
  } else if (status.adminSessionActive) {
    writer.append("Signed in. Normal-LAN settings are unlocked.");
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
                  "<h3>Change administrator password</h3>"
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
    writer.append("<button type=\"submit\">Change password</button></form>");
  }
  writer.append("<p class=\"muted\">Normal-LAN access uses HTTP; passwords "
                "and page contents are not encrypted in transit. Do not use "
                "this interface on an untrusted network.</p></section>"
                "<section><h2>Network</h2><dl><dt>Connection</dt><dd>");
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
  writer.append("</dl>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/wifi/setup\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><button type=\"submit\">Change Wi-Fi</button></form>");
  }
  writer.append("<p class=\"muted\">Setup uses the separate BrooklynMap-Avi "
                "provisioning network and keeps existing credentials unless "
                "a replacement successfully connects.</p></section>"
                "<section><h2>DNS configuration</h2><dl>"
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
                "are unchanged.</p></section>"
                "<section><h2>MTA / data</h2><dl><dt>Last poll cycle</dt><dd data-status=\"feedCycleState\">");
  writer.append(status.hasCycle ? cycleStateName(status.cycleOutcome) : "Not run");
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
                "<p>Recent matching station report: steady blue. Uncertain station report: steady dim amber. Approaching: orange transitioning to cyan during the final 30 predicted seconds; "
                "blinking speeds up (about 0.5 to 3 blinks/second). Departing: blue transitioning to magenta while fading out; "
                "the trail slows and clears within 30 seconds. These are estimated movement cues, not measured distance. "
                "Missing or stalled predictions use orange with a fixed blink. A stopped train sharing a light "
                "takes priority; extra indicators do not add trains or brightness.</p></section>"
                "<section><h2>Network diagnostics</h2><dl>"
                "<dt>Diagnostic run</dt><dd>%s</dd>"
                "<dt>NTP DNS</dt><dd>%s",
                static_cast<unsigned long>(status.resolvedTrains),
                static_cast<unsigned long>(status.approachingTrains),
                status.occupiedLeds, status.collisionCount,
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
  writer.append("<p class=\"muted\">Read-only connectivity checks; raw TCP "
                "success does not verify TLS or HTTPS.</p></section>"
                "<section><h2>Display brightness</h2>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/config/brightness\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format(
        "\"><label for=\"brightness\">Brightness: %u / 255 (%u%%)</label>"
        "<input id=\"brightness\" name=\"brightness\" type=\"range\" "
        "min=\"0\" max=\"255\" value=\"%u\" "
        "oninput=\"document.getElementById('brightness-value').value=this.value\">"
        "<input id=\"brightness-value\" aria-label=\"Brightness value\" "
        "type=\"number\" min=\"0\" max=\"255\" value=\"%u\" "
        "oninput=\"document.getElementById('brightness').value=this.value\">"
        "<button type=\"submit\">Save brightness</button></form>",
        brightness, static_cast<unsigned>(brightness) * 100 / 255, brightness,
        brightness);
  } else {
    writer.format("<p>Current setting: %u / 255 (%u%%).</p>", brightness,
                  static_cast<unsigned>(brightness) * 100 / 255);
  }
  writer.append("<p class=\"muted\">Live output starts automatically with fresh data. POWER toggles off/on; "
                "the physical brightness cap remains 16/255."
                "</p></section>");
  {
    char summary[3072];
    const size_t length = renderBlueSummary(summary, sizeof(summary), status.blueReport);
    writer.append("<details><summary>Why blue?</summary><pre data-status=\"blueSummary\" style=\"overflow:auto\">");
    writer.html(length ? summary : "Diagnostic report unavailable");
    writer.append("</pre></details>");
  }
  writer.format("<section><h2>MTA update interval</h2><p>Wait after each completed download cycle: %lu seconds. "
                "Downloads add to the total interval. MTA feeds are typically generated about every 30 seconds; "
                "faster checks can return identical data.</p>", static_cast<unsigned long>(status.pollSeconds));
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/poll\"><input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format("\"><label>Seconds <input type=\"number\" name=\"seconds\" min=\"15\" max=\"120\" value=\"%lu\" required></label>"
                  "<button>Save update interval</button></form>", static_cast<unsigned long>(status.pollSeconds));
  }
  writer.append("</section>");
  writer.format("<section><h2>OLED</h2><p>Saved I2C address: 0x%02X. Default: 0x38. Reboot after changing.</p>", status.oledAddress);
  {
    char text[160]; renderOledStatus(text, sizeof(text), status.oledStatus);
    writer.append("<p data-status=\"oledStatus\">"); writer.html(text); writer.append("</p>");
    writer.append("<p class=\"muted\">Startup tries the saved address first, then 0x38/0x3C/0x3D. Active 0x00 means none. I2C: 0=OK, 1=short write, 2=NACK, 4=other, 5=timeout.</p>");
  }
  if (status.adminSessionActive && csrfToken && csrfToken[0]) {
    writer.append("<form method=\"post\" action=\"/config/oled\"><input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.format("\"><label>I2C address <input name=\"address\" maxlength=\"4\" pattern=\"0[xX][0-9a-fA-F]{2}\" value=\"0x%02X\" required></label>"
                  "<button>Save OLED address</button></form>", status.oledAddress);
  }
  writer.append("</section>");
  uint8_t visibleRoutes = 0;
  for (uint8_t route = 1; route <= semantic::kRouteCount; ++route) {
    if ((routeVisibilityMask & (uint32_t{1} << (route - 1))) != 0) {
      ++visibleRoutes;
    }
  }
  writer.format("<section><h2>Routes to display</h2><p>%u of %u routes enabled. "
                "This affects map rendering only; feeds and status indicators "
                "continue normally.</p>",
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
      writer.append("> ");
      writer.html(name);
      writer.append("</label>");
    }
    writer.append("</div><button type=\"submit\">Save routes</button></form>");
  }
  writer.append("<p class=\"muted\">All routes may be hidden. Status indicators "
                "remain independent.</p></section>");
  writer.append("<details><summary>Memory diagnostics</summary><dl>"
                "<dt>Free internal heap</dt><dd>");
  writer.format("%lu B", static_cast<unsigned long>(status.freeInternalHeap));
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
  writer.append("</dd></dl></details><section><h2>Device actions</h2>");
  if (status.adminSessionActive && csrfToken != nullptr &&
      csrfToken[0] != '\0') {
    writer.append("<form method=\"post\" action=\"/reboot\" "
                  "onsubmit=\"return confirm('Reboot the map? Settings are retained.')\">"
                  "<input type=\"hidden\" name=\"csrf\" value=\"");
    writer.html(csrfToken);
    writer.append("\"><button type=\"submit\">Reboot Device</button></form>");
  }
  writer.append("<p class=\"muted\">A reboot retains Wi-Fi credentials and "
                "configuration.</p></section>"
                "<p id=\"live-refresh\" class=\"muted\" role=\"status\">Live values refresh automatically.</p>"
                "<script>(()=>{const note=document.getElementById('live-refresh');"
                "async function refresh(){let timer;try{if(!document.hidden){"
                "const controller=new AbortController();timer=setTimeout(()=>controller.abort(),15000);"
                "const r=await fetch('/api/status',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error();"
                "const s=await r.json();s.feedSummary=s.feedsSucceeded+' / '+s.feedsTotal;"
                "document.querySelectorAll('[data-status]').forEach(e=>{const k=e.dataset.status;"
                "if(!(k in s))return;const v=s[k];e.textContent=v===null?'Not available':"
                "k.endsWith('Seconds')?v+' seconds':String(v);});"
                "note.textContent='Live values updated '+new Date().toLocaleTimeString();"
                "}}catch(e){note.textContent='Live refresh delayed; retrying. Displayed values may be old.';}"
                "finally{clearTimeout(timer);setTimeout(refresh,5000);}}setTimeout(refresh,1000);})();</script>"
                "</body></html>");
  return writer.size();
}

size_t renderLoginPage(char *out, size_t capacity, const char *csrfToken) {
  if (csrfToken == nullptr || csrfToken[0] == '\0') return 0;
  Writer writer(out, capacity);
  writer.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<meta name=\"referrer\" content=\"same-origin\">"
                "<title>Administrator sign in</title></head><body>"
                "<h1>Administrator sign in</h1>"
                "<form method=\"post\" action=\"/login\">"
                "<input type=\"hidden\" name=\"csrf\" value=\"");
  writer.html(csrfToken);
  writer.append("\"><label>Password <input type=\"password\" name=\"password\" "
                "autocomplete=\"current-password\" required></label> ");
  writer.append(kPasswordVisibilityControl);
  writer.append("<button type=\"submit\">Sign in</button></form>"
                "<p><a href=\"/\">Return to status</a></p>"
                "<p>Normal-LAN access uses unencrypted HTTP.</p>"
                "</body></html>");
  return writer.size();
}

size_t renderStatusJson(char *out, size_t capacity,
                        const StatusSnapshot &status, uint8_t brightness,
                        uint32_t routeVisibilityMask) {
  Writer writer(out, capacity);
  writer.append("{\"firmware\":\"");
  writer.append(kFirmwareId);
  writer.append("\",\"firmwareVersion\":\"");
  writer.json(status.firmwareVersion);
  writer.format("\",\"otaBootConfirmed\":%s", status.otaBootConfirmed ? "true" : "false");
  writer.append(",\"deviceName\":\"");
  writer.json(status.deviceName);
  writer.append("\",\"target\":\"");
  writer.json(status.target);
  writer.append("\",\"resetReason\":\"");
  writer.json(status.resetReason);
  writer.format("\",\"uptimeSeconds\":%lu,\"networkConnected\":%s,"
                "\"ssid\":\"",
                static_cast<unsigned long>(status.uptimeSeconds),
                status.networkConnected ? "true" : "false");
  writer.json(status.ssid);
  writer.append("\",\"ip\":\"");
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
  writer.format(",\"pollSeconds\":%lu", static_cast<unsigned long>(status.pollSeconds));
  writer.format(",\"physicalFramesWritten\":%lu,\"physicalMapChanges\":%lu,"
                "\"physicalBlinkingLeds\":%u,\"physicalPowerPaused\":%s",
                static_cast<unsigned long>(status.physicalFramesWritten),
                static_cast<unsigned long>(status.physicalMapChanges),
                status.physicalBlinkingLeds, status.physicalPowerPaused ? "true" : "false");
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
