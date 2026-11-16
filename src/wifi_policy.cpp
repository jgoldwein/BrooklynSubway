// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "wifi_policy.h"
#include "password_visibility.h"

#include <string.h>

namespace brooklyn::wifi {

namespace {

// Bounded appender; always leaves out NUL-terminated.
class Out {
 public:
  Out(char *buf, size_t cap) : buf_(buf), cap_(cap), len_(0),full_(false) {
    if (cap_ != 0) buf_[0] = 0;
  }
  void put(const char *s) {
    while (*s != 0) putc(*s++);
  }
  void putc(char c) {
    if (cap_ != 0 && len_ + 1 < cap_) {
      buf_[len_++] = c;
      buf_[len_] = 0;
    } else full_=true;
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
  void jsonEscaped(const char *text) {
    const char hex[]="0123456789abcdef";
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text);*p;++p) {
      if(*p=='"')put("\\\"");else if(*p=='\\')put("\\\\");
      else if(*p<32) {put("\\u00");putc(hex[*p>>4]);putc(hex[*p&15]);}
      else putc(static_cast<char>(*p));
    }
  }
  size_t length() const { return len_; }
  size_t completeLength() {if(full_){if(cap_)buf_[0]=0;return 0;}return len_;}

 private:
  char *buf_;
  size_t cap_;
  size_t len_;
  bool full_;
};

bool startsWith(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

void appendOnlineHelp(Out &o,bool connected) {
  o.put("<section><h2>Help &amp; instructions</h2><p><a href=\"https://github.com/jgoldwein/BrooklynSubway/blob/main/docs/AVI_USER_GUIDE.md\" target=\"_blank\" rel=\"noopener noreferrer\">Read the guide</a> &middot; <a href=\"https://github.com/jgoldwein/BrooklynSubway\" target=\"_blank\" rel=\"noopener noreferrer\">Project on GitHub</a></p><p class=\"hint\">");
  o.put(connected?"After your phone rejoins your home Wi-Fi, these links open online help in a new tab.":"These links need internet access. Finish setup, then reconnect your phone to your home Wi-Fi to open them.");
  o.put("</p></section>");
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
  if (strcmp(path,"/setup/networks")==0)return PortalRoute::Networks;
  if (strcmp(path,"/ui.css")==0 || strcmp(path,"/setup.css")==0 || strcmp(path,"/setup.js")==0)return PortalRoute::Asset;
  if (strcmp(path, "/generate_204") == 0 || strcmp(path, "/gen_204") == 0 ||
      strcmp(path, "/hotspot-detect.html") == 0 || strcmp(path, "/library/test/success.html") == 0 ||
      strcmp(path, "/connecttest.txt") == 0 || strcmp(path, "/ncsi.txt") == 0 ||
      strcmp(path, "/redirect") == 0 || strcmp(path, "/fwlink") == 0 ||
      startsWith(path, "/fwlink")) {
    return PortalRoute::Probe;
  }
  return PortalRoute::Redirect;
}

size_t renderScanJson(char *out,size_t capacity,const ScanEntry *entries,size_t count,bool pending,bool busy) {
  Out o(out,capacity);o.put("{\"pending\":");o.put(pending?"true":"false");o.put(",\"busy\":");o.put(busy?"true":"false");o.put(",\"networks\":[");
  for(size_t i=0;entries && i<count && i<kMaxScanEntries;++i) {
    if(i)o.put(",");o.put("{\"ssid\":\"");o.jsonEscaped(entries[i].ssid);o.put("\",\"rssi\":");o.putInt(entries[i].rssi);
    o.put(",\"encrypted\":");o.put(entries[i].encrypted?"true":"false");o.put("}");
  }
  o.put("]}");return o.completeLength();
}

size_t renderPortalPage(char *out, size_t capacity, const ScanEntry *entries, size_t count,
                        PortalStatus status, bool hasStored, const char *connectedSsid,
                        const char *connectedIp, bool adminConfigured, bool editAdmin) {
  Out o(out,capacity);
  o.put("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">");
  if(status==PortalStatus::Connecting)o.put("<meta http-equiv=\"refresh\" content=\"2\">");
  o.put("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Brooklyn Subway - Wi-Fi setup</title>"
        "<link rel=\"stylesheet\" href=\"/ui.css?v=038\"><link rel=\"stylesheet\" href=\"/setup.css?v=050\">"
        "<script defer src=\"/setup.js?v=050\"></script></head><body class=\"setup-shell\">"
        "<header><div class=\"brand\"><span class=\"brand-mark\" aria-hidden=\"true\">BK</span><div>"
        "<h1>Brooklyn Subway</h1><p class=\"subtitle\">Specially Designed for Avi Yao</p></div></div></header>"
        "<main><section class=\"hero\"><p class=\"kicker\">Wi-Fi setup mode</p><h2>Let's connect your map</h2>"
        );
  if(status==PortalStatus::Saved)
    o.put("<p>Your map has joined your home Wi-Fi. Now connect your phone to that same Wi-Fi to open the map.</p>");
  else
    o.put("<p>On your phone, open Wi-Fi settings and join <strong>BrooklynMap-Avi</strong>. Choose &ldquo;Stay connected&rdquo; or &ldquo;Use without internet&rdquo; if asked.</p>"
          "<p>Keep this setup page open while you enter your details. No app or account is needed.</p>");
  o.put("<ol class=\"steps\" aria-label=\"Setup progress\"><li");
  if((!adminConfigured || editAdmin) && status!=PortalStatus::Saved && status!=PortalStatus::Connecting)o.put(" aria-current=\"step\"");
  o.put(">1. Map password</li><li");if(status==PortalStatus::Connecting || (adminConfigured && !editAdmin && status!=PortalStatus::Saved))o.put(" aria-current=\"step\"");
  o.put(">2. Wi-Fi</li><li");if(status==PortalStatus::Saved)o.put(" aria-current=\"step\"");o.put(">3. Open your map</li></ol></section>");
  if(status==PortalStatus::Connecting) {
    o.put("<section><h2>Connecting to Wi-Fi...</h2><progress aria-label=\"Connecting to Wi-Fi\"></progress>"
          "<p>Please keep this page open. This can take up to 20 seconds.</p><p class=\"hint\">Your saved Wi-Fi settings are kept until a new network connects.</p></section>");
    appendOnlineHelp(o,false);o.put("</main></body></html>");
    return o.completeLength();
  }
  if(status==PortalStatus::Saved || status==PortalStatus::SaveFailed) {
    const bool saved=status==PortalStatus::Saved;
    o.put(saved?"<section><div class=\"alert success\" role=\"status\"><h2>You're connected!</h2>":"<section><div class=\"alert warning\" role=\"alert\"><h2>Connected, but not saved</h2>");
    o.put("<p>Successfully connected to Wi-Fi");
    if(connectedSsid && *connectedSsid){o.put(": <b>");o.escaped(connectedSsid);o.put("</b>");}
    o.put(saved?". Settings saved.</p>":", but settings could not be saved. Keep this page open and try again.</p>");
    o.put("</div>");
    bool ipSafe=connectedIp && *connectedIp;
    for(const char *c=connectedIp;ipSafe && *c;++c)ipSafe=(*c>='0'&&*c<='9')||*c=='.';
    if(ipSafe) {
      o.put("<h3>Open your map</h3><ol><li>Reconnect your phone to the same Wi-Fi as the map using your phone's Wi-Fi settings.</li><li>Wait about 10 seconds for your phone to rejoin that Wi-Fi.</li><li>Close the setup pop-up and open Safari, Chrome, or your usual browser.</li><li>Type this address in the address bar:</li></ol>"
            "<p class=\"ip-link\"><a href=\"http://");o.put(connectedIp);o.put("/\">");o.put(connectedIp);o.put("</a></p>"
            "<a class=\"button\" href=\"http://");o.put(connectedIp);o.put("/\">Open your map</a>"
            "<p class=\"hint\">If you have two maps, use this board's individual IP address. If it does not open yet, wait a few more seconds and try again. Bookmark this page for next time. You do not need to join BrooklynMap-Avi again for everyday use.</p>");
    }
    if(!adminConfigured)o.put("<p class=\"alert warning\">No administrator password is set. The map's settings will stay locked until you set one through setup Wi-Fi.</p>");
    o.put("<p class=\"hint\">To return while setup Wi-Fi is available, go to <a href=\"http://192.168.4.1/\">192.168.4.1</a>. ");
    if(saved)o.put("The setup network closes shortly. This is normal; the map keeps using your home Wi-Fi.");o.put("</p></section>");
    if(saved) {
      appendOnlineHelp(o,true);
      if(ipSafe) {
        o.put("<script>var ip=\"");o.put(connectedIp);
        o.put("\";function probe(){var c=new AbortController(),k=setTimeout(function(){c.abort()},2500);fetch(\"http://\"+ip+\"/api/status\",{mode:\"no-cors\",cache:\"no-store\",signal:c.signal}).then(function(){clearTimeout(k);location.replace(\"http://\"+ip+\"/\")}).catch(function(){clearTimeout(k);setTimeout(probe,2000)})}setTimeout(probe,10000)</script>");
      }
      o.put("</main></body></html>");return o.completeLength();
    }
  }
  if(status==PortalStatus::ConnectFailed)o.put("<section class=\"alert error\" role=\"alert\"><h2>Let's try that again</h2><p>Could not connect. Check the name and password; previously saved settings were not changed.</p><p>Make sure your network supports 2.4 GHz.</p></section>");
  if(status==PortalStatus::InvalidInput)o.put("<section class=\"alert error\" role=\"alert\"><h2>Check your Wi-Fi details</h2><p>Choose or enter a Wi-Fi name. Use 8 to 63 password characters, or leave it empty for an open network.</p></section>");
  if(!adminConfigured || editAdmin) {
  o.put("<section><h2 class=\"step-title\"><span class=\"step-number\">1</span>Protect your map settings</h2>");
  if(adminConfigured)o.put("<p>An administrator password is configured. Resetting it replaces the map password and signs out existing administrator sessions.</p>");
  else o.put("<p>Set the initial administrator password so you can change map settings later. This is just for your map &mdash; it can be different from your Wi-Fi password. Save it somewhere safe; you will use it to sign in when changing settings.</p>");
  o.put("<form method=\"POST\" action=\"/admin/setup\" id=\"admin-form\"><label for=\"admin-password\">New map password (at least 8 characters)</label>"
        "<input id=\"admin-password\" name=\"password\" type=\"password\" minlength=\"8\" maxlength=\"128\" autocomplete=\"new-password\" required>"
        "<label for=\"admin-confirm\">Type the map password again</label><input id=\"admin-confirm\" name=\"confirmation\" type=\"password\" minlength=\"8\" maxlength=\"128\" autocomplete=\"new-password\" required>");
  o.put(web::kPasswordVisibilityControl);o.put("<button type=\"submit\" class=\"submit\">");o.put(adminConfigured?"Save password and continue":"Save password and continue to Wi-Fi");o.put("</button></form>");
  if(adminConfigured)o.put("<p><a href=\"/\">Cancel and keep the current password</a></p>");
  o.put("<p class=\"hint\">Save this password first. The next page lets you choose your Wi-Fi.</p></section>");
  appendOnlineHelp(o,status==PortalStatus::SaveFailed);
  o.put("</main><p class=\"footer\">Setup address: 192.168.4.1 &middot; Keep your map powered while connecting.</p></body></html>");
  return o.completeLength();
  }
  o.put("<section><p class=\"pill\">Administrator password is set</p><p>Step 1 is complete. Now connect the map to your Wi-Fi.</p><p><a href=\"/admin/setup\">Change the map password</a></p></section>");
  o.put("<section><h2 class=\"step-title\"><span class=\"step-number\">2</span>Choose your Wi-Fi</h2>"
        "<p>The map uses <strong>2.4 GHz</strong> Wi-Fi. A shared 2.4/5 GHz name is fine; a 5 GHz-only network will not work.</p>");
  if(hasStored)o.put("<p class=\"hint\">Saved Wi-Fi settings stay in place until a new network connects.</p>");
  o.put("<div class=\"network-list\" id=\"setup-networks\" aria-label=\"Nearby Wi-Fi networks\">");
  for(size_t i=0;entries && i<count && i<3;++i) {
    o.put("<button type=\"button\" class=\"network\" aria-pressed=\"false\"><span class=\"ssid\">");o.escaped(entries[i].ssid);o.put("</span><small>");
    o.put(entries[i].rssi>=-60?"Strong signal":entries[i].rssi>=-75?"Good signal":"Weak signal");o.put(entries[i].encrypted?" - Password needed":" - Open network");o.put("</small></button>");
  }
  o.put("</div><div class=\"actions\"><button type=\"button\" class=\"secondary\" id=\"scan-again\">Scan again</button><p id=\"scan-state\" class=\"hint\" role=\"status\">Loading nearby Wi-Fi...</p></div>"
        "<noscript><p>Type the Wi-Fi name below. <a href=\"/scan\">Rescan and reload this page</a> if needed.</p></noscript>"
        "<form method=\"POST\" action=\"/connect\" id=\"wifi-form\"><label for=\"wifi-name\">Wi-Fi name</label>"
        "<input id=\"wifi-name\" name=\"pick\" type=\"text\" maxlength=\"32\" placeholder=\"Choose above, or type a network name\" autocomplete=\"off\" autocapitalize=\"off\" autocorrect=\"off\" spellcheck=\"false\" required>"
        "<label for=\"wifi-password\">Wi-Fi password</label><input id=\"wifi-password\" name=\"pass\" type=\"password\" minlength=\"8\" maxlength=\"63\" autocomplete=\"off\" autocapitalize=\"off\" autocorrect=\"off\" spellcheck=\"false\">"
        "<p class=\"hint\">Leave the password empty only for an open network.</p>");
  o.put(web::kPasswordVisibilityControl);o.put("<button type=\"submit\" class=\"submit\">Connect to Wi-Fi</button></form></section>");
  appendOnlineHelp(o,status==PortalStatus::SaveFailed);
  o.put("</main><p class=\"footer\">Setup address: 192.168.4.1 &middot; Keep your map powered while connecting.</p></body></html>");
  return o.completeLength();
}

}  // namespace brooklyn::wifi
