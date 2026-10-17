// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "lan_web_policy.h"

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

size_t renderHomePage(char *out, size_t capacity,
                      const StatusSnapshot &status, uint8_t brightness) {
  Writer writer(out, capacity);
  writer.append("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<meta http-equiv=\"refresh\" content=\"20\">"
                "<title>Brooklyn Active Subway Map</title><style>"
                "body{font:16px system-ui,sans-serif;line-height:1.45;max-width:52rem;"
                "margin:1rem auto;padding:0 1rem;color:#17212b}"
                "h1{font-size:1.65rem}.status{padding:.7rem;background:#eef3f6}"
                "section{margin:1.3rem 0}dl{display:grid;grid-template-columns:"
                "minmax(9rem,1fr) 2fr;gap:.35rem}dt{font-weight:600}"
                "dd{margin:0;overflow-wrap:anywhere}input,button{font:inherit;"
                "padding:.5rem}input[type=range]{width:100%}form{margin:.6rem 0}"
                "button{cursor:pointer}.muted{color:#52616b}details{margin-top:1rem}"
                "</style></head><body><h1>Brooklyn Active Subway Map</h1>"
                "<p class=\"status\">");
  writer.html(status.deviceName);
  writer.append(" - ");
  writer.append(status.networkConnected ? "Connected to LAN" : "LAN disconnected");
  writer.append(" | Display: ");
  writer.append(displayStateName(status.displayState));
  writer.append("</p><section><h2>Device</h2><dl><dt>Firmware</dt><dd>");
  writer.append(kFirmwareId);
  writer.append("</dd><dt>Target</dt><dd>");
  writer.html(status.target);
  writer.append("</dd><dt>Uptime</dt><dd>");
  writer.format("%lu seconds", static_cast<unsigned long>(status.uptimeSeconds));
  writer.append("</dd><dt>Reset reason</dt><dd>");
  writer.html(status.resetReason);
  writer.append("</dd><dt>Physical LED output</dt><dd>DISABLED</dd></dl></section>"
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
  writer.append("</dd><dt>Hostname</dt><dd>");
  writer.append(kHostname);
  writer.append("</dd><dt>Time synchronization</dt><dd>");
  writer.append(status.timeSynchronized ? "Synchronized" : "Waiting");
  writer.append("</dd>");
  if (status.mdnsAvailable) {
    writer.append("<dt>Local name</dt><dd><a href=\"");
    writer.append(kMdnsUrl);
    writer.append("\">");
    writer.append(kMdnsUrl);
    writer.append("</a></dd>");
  }
  writer.append("</dl><form method=\"post\" action=\"/wifi/setup\">"
                "<button type=\"submit\">Change Wi-Fi</button></form>"
                "<p class=\"muted\">Setup uses the separate BrooklynMap-Avi "
                "provisioning network and keeps existing credentials unless "
                "a replacement successfully connects.</p></section>"
                "<section><h2>MTA / data</h2><dl><dt>Last poll cycle</dt><dd>");
  writer.append(status.hasCycle ? cycleStateName(status.cycleOutcome) : "Not run");
  writer.append("</dd><dt>Feeds succeeded / total</dt><dd>");
  if (status.hasCycle) {
    writer.format("%u / %u", status.feedsSucceeded, status.feedsTotal);
  } else {
    writer.append("Not available");
  }
  writer.append("</dd><dt>Operational display</dt><dd>");
  writer.append(displayStateName(status.displayState));
  writer.append("</dd><dt>Last usable update age</dt><dd>");
  if (status.hasDataAge) {
    writer.format("%lu seconds", static_cast<unsigned long>(status.dataAgeSeconds));
  } else {
    writer.append("Not available");
  }
  writer.append("</dd><dt>Last complete update age</dt><dd>");
  if (status.hasCompleteAge) {
    writer.format("%lu seconds", static_cast<unsigned long>(status.completeAgeSeconds));
  } else {
    writer.append("Not available");
  }
  writer.format("</dd><dt>Resolved trains</dt><dd>%lu</dd>"
                "<dt>Occupied LEDs</dt><dd>%u</dd>"
                "<dt>Collision count</dt><dd>%u</dd></dl></section>"
                "<section><h2>Display brightness</h2>"
                "<form method=\"post\" action=\"/config/brightness\">"
                "<label for=\"brightness\">Brightness: %u / 255 (%u%%)</label>"
                "<input id=\"brightness\" name=\"brightness\" type=\"range\" "
                "min=\"0\" max=\"255\" value=\"%u\" "
                "oninput=\"document.getElementById('brightness-value').value=this.value\">"
                "<input id=\"brightness-value\" aria-label=\"Brightness value\" "
                "type=\"number\" min=\"0\" max=\"255\" value=\"%u\" "
                "oninput=\"document.getElementById('brightness').value=this.value\">"
                "<button type=\"submit\">Save brightness</button></form>"
                "<p class=\"muted\">Logical display setting. Physical output "
                "remains disabled; the production power cap is separate.</p></section>",
                static_cast<unsigned long>(status.resolvedTrains),
                status.occupiedLeds, status.collisionCount, brightness,
                static_cast<unsigned>(brightness) * 100 / 255, brightness,
                brightness);
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
  writer.append("</dd></dl></details><section><h2>Device actions</h2>"
                "<form method=\"post\" action=\"/reboot\" "
                "onsubmit=\"return confirm('Reboot the map? Settings are retained.')\">"
                "<button type=\"submit\">Reboot Device</button></form>"
                "<p class=\"muted\">A reboot retains Wi-Fi credentials and "
                "configuration.</p></section></body></html>");
  return writer.size();
}

size_t renderStatusJson(char *out, size_t capacity,
                        const StatusSnapshot &status, uint8_t brightness) {
  Writer writer(out, capacity);
  writer.append("{\"firmware\":\"");
  writer.append(kFirmwareId);
  writer.append("\",\"deviceName\":\"");
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
  writer.format("\",\"rssi\":%d,\"hostname\":\"%s\","
                "\"mdnsAvailable\":%s,\"timeSynchronized\":%s,"
                "\"displayState\":\"%s\",\"feedCycleState\":\"",
                static_cast<int>(status.rssi), kHostname,
                status.mdnsAvailable ? "true" : "false",
                status.timeSynchronized ? "true" : "false",
                displayStateName(status.displayState));
  writer.json(status.hasCycle ? cycleStateName(status.cycleOutcome) : "NOT RUN");
  writer.format("\",\"feedsSucceeded\":%u,\"feedsTotal\":%u,"
                "\"dataAgeSeconds\":",
                status.feedsSucceeded, status.feedsTotal);
  appendNullableAge(writer, status.hasDataAge, status.dataAgeSeconds);
  writer.append(",\"completeAgeSeconds\":");
  appendNullableAge(writer, status.hasCompleteAge, status.completeAgeSeconds);
  writer.format(",\"resolvedTrains\":%lu,\"occupiedLeds\":%u,"
                "\"collisionCount\":%u,\"brightness\":%u,"
                "\"physicalLedOutputEnabled\":false,"
                "\"freeInternalHeap\":%lu,\"minimumInternalHeap\":%lu,"
                "\"freePsram\":",
                static_cast<unsigned long>(status.resolvedTrains),
                status.occupiedLeds, status.collisionCount, brightness,
                static_cast<unsigned long>(status.freeInternalHeap),
                static_cast<unsigned long>(status.minimumInternalHeap));
  if (status.psramPresent) {
    writer.format("%lu,\"minimumFreePsram\":%lu",
                  static_cast<unsigned long>(status.freePsram),
                  static_cast<unsigned long>(status.minimumFreePsram));
  } else {
    writer.append("null,\"minimumFreePsram\":null");
  }
  writer.format(",\"psramPresent\":%s,\"psramFeedBufferPresent\":%s}",
                status.psramPresent ? "true" : "false",
                status.psramFeedBufferPresent ? "true" : "false");
  return writer.size();
}

}  // namespace brooklyn::web
