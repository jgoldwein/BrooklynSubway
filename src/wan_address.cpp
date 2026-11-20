// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "wan_address.h"
#include <string.h>
namespace brooklyn::wan {
namespace {
bool space(uint8_t c) { return c==' ' || c=='\r' || c=='\n' || c=='\t'; }
}
bool parseIpv4(const uint8_t *data, size_t length, char out[kAddressSize], bool publicOnly) {
  if(!data || !out || !length || length>kMaxBody)return false;
  size_t start=0,end=length;
  while(start<end && space(data[start]))++start;
  while(end>start && space(data[end-1]))--end;
  if(end-start<7 || end-start>=kAddressSize)return false;
  uint16_t octets[4]{};size_t pos=start;
  for(unsigned n=0;n<4;++n) {
    const size_t first=pos;
    while(pos<end && data[pos]>='0' && data[pos]<='9') {
      if(pos-first==3)return false;
      octets[n]=uint16_t(octets[n]*10+data[pos++]-'0');
    }
    if(pos==first || octets[n]>255 || (pos-first>1 && data[first]=='0'))return false;
    if(n<3 && (pos==end || data[pos++]!='.'))return false;
  }
  if(pos!=end || octets[0]==0 || octets[0]==127 || octets[0]>=224)return false;
  const auto a=octets[0],b=octets[1],c=octets[2];
  if(publicOnly && (a==10 || (a==100 && b>=64 && b<=127) || (a==169 && b==254) ||
      (a==172 && b>=16 && b<=31) || (a==192 && (b==168 || (b==0 && (c==0 || c==2)))) ||
      (a==198 && (b==18 || b==19 || (b==51 && c==100))) || (a==203 && b==0 && c==113)))return false;
  memcpy(out,data+start,end-start);out[end-start]=0;return true;
}
void Cache::observe(bool online) {
  if(online_==online)return;
  online_=online;++epoch_;attempted_=false;valid_=false;address_[0]=0;
}
bool Cache::due(uint32_t now) const {
  return online_ && (!attempted_ || uint32_t(now-attemptAt_)>=(valid_?kRefreshMs:kRetryMs));
}
void Cache::started(uint32_t now) {attempted_=true;attemptAt_=now;}
bool Cache::finish(const uint8_t *body,size_t length,bool downloaded,uint32_t now,uint32_t epoch) {
  if(!online_ || epoch!=epoch_)return false;
  attempted_=true;attemptAt_=now;valid_=false;address_[0]=0;
  if(downloaded && parseIpv4(body,length,address_,true)) {valid_=true;checkedAt_=now;}
  return valid_;
}
const char *Cache::value(uint32_t now) const {
  return online_ && valid_ && uint32_t(now-checkedAt_)<kRefreshMs?address_:"";
}
}
