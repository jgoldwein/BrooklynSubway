// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "weather.h"
#include <string.h>
namespace brooklyn::weather {
const char *description(uint8_t code) {
  switch(code) {
    case 0:return "Clear";case 1:return "Mainly clear";case 2:return "Partly cloudy";case 3:return "Overcast";
    case 45:case 48:return "Fog";
    case 51:case 53:case 55:return "Drizzle";case 56:case 57:return "Freezing drizzle";
    case 61:case 63:case 65:return "Rain";case 66:case 67:return "Freezing rain";
    case 71:case 73:case 75:case 77:return "Snow";
    case 80:case 81:case 82:return "Rain showers";case 85:case 86:return "Snow showers";
    case 95:case 96:case 99:return "Thunderstorms";default:return "Unknown conditions";
  }
}
namespace {
bool digits(const char *&p,const char *end,uint32_t &value) {
  value=0;const char *start=p;
  while(p<end && *p>='0' && *p<='9') {
    if(value>(UINT32_MAX-uint32_t(*p-'0'))/10) return false;
    value=value*10+(*p++-'0');
  }
  return p!=start;
}
}
bool parseCsv(const uint8_t *data,size_t length,Snapshot &out) {
  if(!data || !length || length>1024) return false;
  const char *p=reinterpret_cast<const char *>(data),*end=p+length;
  const char *values=nullptr;
  while(p<end) {
    const char *line=p;while(p<end && *p!='\n') {if(!*p) return false;++p;}
    constexpr char prefix[]="time,temperature_2m (";
    if(size_t(p-line)>sizeof(prefix)-1 && memcmp(line,prefix,sizeof(prefix)-1)==0) {
      // Reject changed column order/units rather than silently labeling C as F.
      const char *q=line+sizeof(prefix)-1;
      while(q<p && static_cast<unsigned char>(*q)>=128) ++q;
      const char suffix[]="F),weather_code (wmo code)";
      const size_t rest=p-q;
      if(rest!=sizeof(suffix)-1 && !(rest==sizeof(suffix) && p[-1]=='\r')) return false;
      if(memcmp(q,suffix,sizeof(suffix)-1)!=0 || values) return false;
      values=p<end?p+1:end;break;
    }
    if(p<end) ++p;
  }
  if(!values) return false;
  p=values;Snapshot next;uint32_t stamp=0,integer=0,fraction=0,code=0;
  if(!digits(p,end,stamp) || stamp<1700000000 || p==end || *p++!=',') return false;
  bool negative=p<end && *p=='-';if(negative) ++p;
  if(!digits(p,end,integer) || integer>200) return false;
  if(p<end && *p=='.') {++p;if(p==end || *p<'0'||*p>'9') return false;fraction=*p++-'0';}
  if(p==end || *p++!=',' || !digits(p,end,code) || code>99) return false;
  if(p==end || (*p!='\r' && *p!='\n')) return false;
  while(p<end && (*p=='\r'||*p=='\n')) ++p;
  if(p!=end) return false;
  const int32_t temperature=(negative?-1:1)*int32_t(integer*10+fraction);
  if(temperature<-1500 || temperature>1500) return false;
  next.sourceUnix=stamp;next.fahrenheitTenths=temperature;next.code=code;
  next.available=next.attempted=true;out=next;return true;
}
bool due(bool attempted,uint32_t lastAttempt,uint32_t now,uint32_t cycleFinished) {
  return uint32_t(now-cycleFinished)<1000 && (!attempted || uint32_t(now-lastAttempt)>=kPollMs);
}
}
