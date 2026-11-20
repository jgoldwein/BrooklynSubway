// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "monitor_config.h"
#include <string.h>

namespace brooklyn::monitor {
void wipe(void *data,size_t size) {volatile uint8_t *p=static_cast<volatile uint8_t *>(data);while(size--)*p++=0;}
namespace {
bool plain(const char *s,size_t cap,size_t min) {
  size_t n=0;for(;n<cap && s[n];++n)if(s[n]<32 || s[n]>126)return false;
  return n>=min && n<cap && (!n || (s[0]!=' ' && s[n-1]!=' '));
}
bool hostValid(const char *s,size_t cap) {
  size_t n=0,label=0;for(;n<cap && s[n];++n) {
    const char c=s[n];
    if(c=='.') {if(!label || s[n-1]=='-')return false;label=0;continue;}
    if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-') || (!label && c=='-') || ++label>63)return false;
  }
  return n>0 && n<cap && label>0 && s[n-1]!='-';
}
bool copy(char *out,size_t cap,const char *in) {
  if(!in)return false;
  size_t n=0;while(n<cap && in[n])++n;if(n==cap)return false;
  memcpy(out,in,n+1);return true;
}
void increment(uint32_t &n) {if(n<2147483647u)++n;}
}
bool validUuid(const char *s) {
  if(!s)return false;
  for(unsigned n=0;n<36;++n) {
    const char c=s[n];if(!c)return false;
    if(n==8 || n==13 || n==18 || n==23) {if(c!='-')return false;}
    else if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
  }
  return s[36]==0 && s[14]=='4' && (s[19]=='8' || s[19]=='9' || s[19]=='a' || s[19]=='b');
}
bool valid(const Settings &s) {
  return s.version==kRecordVersion && s.enabled<=1 && hostValid(s.host,sizeof(s.host)) && s.port &&
    memchr(s.deviceId,0,sizeof(s.deviceId)) && validUuid(s.deviceId) &&
    plain(s.password,sizeof(s.password),24) && plain(s.name,sizeof(s.name),1);
}
bool parseSettings(const Param *params,size_t count,const Settings &previous,Settings &out) {
  if(!params || count!=6)return false;
  Settings next;unsigned seen=0;
  const char *keys[]={"host","port","deviceId","password","name","enabled"};
  bool ok=true;
  for(size_t n=0;n<count && ok;++n) {
    if(!params[n].name || !params[n].value) {ok=false;break;}
    unsigned i=0;for(;i<6 && strcmp(keys[i],params[n].name);++i){}
    if(i==6 || (seen&(1u<<i))) {ok=false;break;}seen|=1u<<i;
    const char *v=params[n].value;
    switch(i) {
      case 0:ok=copy(next.host,sizeof(next.host),v);break;
      case 1: {
        uint32_t p=0;size_t j=0;
        for(;j<6 && v[j];++j) {if(v[j]<'0' || v[j]>'9')break;p=p*10+v[j]-'0';}
        ok=j>0 && j<6 && !v[j] && p>0 && p<=65535;next.port=static_cast<uint16_t>(p);break;
      }
      case 2:ok=copy(next.deviceId,sizeof(next.deviceId),v);break;
      case 3:ok=copy(next.password,sizeof(next.password),v);break;
      case 4:ok=copy(next.name,sizeof(next.name),v);break;
      case 5:ok=!strcmp(v,"0") || !strcmp(v,"1");next.enabled=!strcmp(v,"1");break;
    }
  }
  if(ok && !next.password[0] && valid(previous) && next.port==previous.port &&
      !strcmp(next.host,previous.host) && !strcmp(next.deviceId,previous.deviceId))
    memcpy(next.password,previous.password,sizeof(next.password));
  ok=ok && seen==63 && valid(next);
  if(ok)out=next;
  wipe(&next,sizeof(next));return ok;
}
bool load(const Store &store,Settings &out,bool &configured) {
  Settings next;bool present=false;configured=false;
  const bool read=store.read && store.read(store.context,next,present);
  const bool ok=read && (!present || valid(next));
  if(ok && present) {out=next;configured=true;}else out=Settings{};
  wipe(&next,sizeof(next));return ok;
}
bool ConnectionHistory::observeWifi(bool up,uint64_t now) {
  const bool lost=wifiWasUp && !up;
  if(lost) {increment(wifiLosses);lastWifiLossMs=now;wifiOutage=true;}
  if(up && wifiOutage) {wifiOutage=false;wifiRecoveryMs=now;wifiOutageMs=now-lastWifiLossMs;}
  wifiWasUp=up;return lost;
}
void ConnectionHistory::mqttLost(uint64_t now) {if(mqttOutage)return;increment(mqttLosses);lastMqttLossMs=now;mqttOutage=true;}
void ConnectionHistory::mqttRecovered(uint64_t now) {
  if(!mqttOutage)return;mqttOutage=false;mqttRecoveryMs=now;mqttOutageMs=now-lastMqttLossMs;
}
bool resourcesAllow(bool active,uint32_t freeBytes,uint32_t largestBytes) {
  return freeBytes>=(active?kStopHeapBytes:kStartHeapBytes) && largestBytes>=(active?kStopLargestBytes:kStartLargestBytes);
}
}
