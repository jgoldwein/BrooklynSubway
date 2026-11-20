// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "wan_address.h"
#include <stdio.h>
#include <string.h>
using namespace brooklyn::wan;
int checks=0,failures=0;
#define CHECK(x) do {++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
bool parse(const char *text,char *out,bool global=true) {
  return parseIpv4(reinterpret_cast<const uint8_t *>(text),strlen(text),out,global);
}
int main() {
  char address[kAddressSize]="unchanged";
  CHECK(parse("8.8.8.8",address) && !strcmp(address,"8.8.8.8"));
  CHECK(parse(" \r\n1.1.1.1\t",address) && !strcmp(address,"1.1.1.1"));
  CHECK(parse("223.255.255.254",address));
  const char *bad[]={"", "8.8.8", "8.8.8.8.8", "8.8.8.256", "08.8.8.8", "8. 8.8.8",
    "8.8.8.8<script>", "::1", "2001:4860:4860::8888", "0.0.0.0", "127.0.0.1",
    "10.1.1.27", "100.64.0.1", "100.127.255.254", "169.254.1.2", "172.16.0.1",
    "172.31.255.254", "192.168.1.1", "192.0.2.1", "198.18.1.2", "198.19.1.2",
    "198.51.100.10", "203.0.113.1", "224.0.0.1", "255.255.255.255"};
  for(const char *value:bad) {
    strcpy(address,"unchanged");CHECK(!parse(value,address));CHECK(!strcmp(address,"unchanged"));
  }
  CHECK(parse("10.1.1.27",address,false) && !strcmp(address,"10.1.1.27"));
  CHECK(parse("192.168.4.1",address,false));
  CHECK(!parse("0.0.0.0",address,false));
  CHECK(!parseIpv4(nullptr,1,address,true));
  CHECK(!parseIpv4(reinterpret_cast<const uint8_t *>("8.8.8.8"),7,nullptr,true));
  const uint8_t embedded[]={'8','.','8','.','8','.','8',0,'8'};
  CHECK(!parseIpv4(embedded,sizeof(embedded),address,true));
  uint8_t huge[kMaxBody+1]{};CHECK(!parseIpv4(huge,sizeof(huge),address,true));

  Cache cache;CHECK(!cache.due(0) && !cache.value(0)[0]);
  cache.observe(true);const auto firstEpoch=cache.epoch();CHECK(cache.due(100));
  cache.started(100);CHECK(!cache.due(101));
  const uint8_t body[]="8.8.8.8";
  CHECK(cache.finish(body,7,true,200,firstEpoch));
  CHECK(!strcmp(cache.value(200),"8.8.8.8"));
  CHECK(!cache.due(200+kRefreshMs-1) && cache.value(200+kRefreshMs-1)[0]);
  CHECK(cache.due(200+kRefreshMs) && !cache.value(200+kRefreshMs)[0]);
  cache.started(200+kRefreshMs);
  CHECK(!cache.finish(body,7,false,300+kRefreshMs,firstEpoch));
  CHECK(!cache.value(300+kRefreshMs)[0] && !cache.due(300+kRefreshMs+kRetryMs-1));
  CHECK(cache.due(300+kRefreshMs+kRetryMs));
  cache.observe(false);CHECK(!cache.due(0));
  cache.observe(true);CHECK(cache.epoch()!=firstEpoch && cache.due(0));
  CHECK(!cache.finish(body,7,true,0,firstEpoch) && !cache.value(0)[0]);
  CHECK(cache.finish(body,7,true,UINT32_MAX-100,cache.epoch()));
  CHECK(!strcmp(cache.value(50),"8.8.8.8") && !cache.due(50));
  cache.observe(false);CHECK(!cache.value(51)[0]);
  CHECK(!cache.finish(body,7,true,52,cache.epoch()));
  printf("WAN address checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
