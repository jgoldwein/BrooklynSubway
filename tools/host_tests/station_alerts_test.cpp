// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "station_alerts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace brooklyn::alerts;
// Small synthetic protobuf writer; no copied MTA alert text in fixtures.
struct Bytes {
 unsigned char data[8192]{};size_t n=0;
 void var(uint64_t v){do{data[n++]=(v&127)|(v>127?128:0);v>>=7;}while(v);}
 void integer(unsigned field,uint64_t v){var(field*8);var(v);}
 void blob(unsigned field,const unsigned char *p,size_t length){var(field*8+2);var(length);memcpy(data+n,p,length);n+=length;}
 void str(unsigned field,const char *s){blob(field,reinterpret_cast<const unsigned char *>(s),strlen(s));}
 void msg(unsigned field,const Bytes &b){blob(field,b.data,b.n);}
};
constexpr uint32_t now=1794960000;
Bytes translated(const char *message,const char *language="en") {Bytes t,a;t.str(1,message);t.str(2,language);a.msg(1,t);return a;}
Bytes entity(const char *id,const char *stop,const char *route,uint32_t start,uint32_t end,const char *headline="Test station notice",const char *description="Plain details <script>& test") {
 Bytes a,s,e,period;s.str(1,"MTASBWY");s.str(2,route);if(stop[0])s.str(5,stop);a.msg(5,s);
 period.integer(1,start);if(end)period.integer(2,end);a.msg(1,period);
 a.msg(10,translated(headline));a.msg(11,translated(description));e.str(1,id);e.msg(5,a);return e;
}
Bytes feed(const Bytes &e,unsigned incremental=0) {Bytes h,f;h.str(1,"2.0");h.integer(2,incremental);h.integer(3,now);f.msg(1,h);f.msg(2,e);return f;}
int main(int argc,char **argv) {
 static Snapshot out;Preferences p;
 if(argc==3) {
  FILE *file=fopen(argv[1],"rb");assert(file);fseek(file,0,SEEK_END);long size=ftell(file);rewind(file);
  auto *data=static_cast<unsigned char *>(malloc(size));assert(data);assert(fread(data,1,size,file)==size_t(size));fclose(file);
  assert(decode(data,size,p,static_cast<uint32_t>(strtoul(argv[2],nullptr,10)),out));free(data);
  printf("Live alert decode: source %lu; matching %u; retained %u; overflow %u\n",(unsigned long)out.sourceUnix,out.matched,out.count,out.overflow);
  for(unsigned i=0;i<out.count;++i)printf("%s | station %u | linewide %u | start %lu end %lu\n",out.items[i].id,out.items[i].stations,out.items[i].lineWide,(unsigned long)out.items[i].start,(unsigned long)out.items[i].end);
  return 0;
 }
 auto f=feed(entity("one","F18","F",now-1,now+100));assert(decode(f.data,f.n,p,now,out));
 assert(out.count==1 && out.items[0].stations==1 && current(out.items[0],now));
 f=feed(entity("other","A40","A",now-1,now+100));assert(decode(f.data,f.n,p,now,out)&&out.count==0);
 p.stations=3;assert(decode(f.data,f.n,p,now,out)&&out.count==1);p=Preferences{};
 f=feed(entity("suffix","F18N","F",now+100,now+200));assert(decode(f.data,f.n,p,now,out)&&out.count==1&&upcoming(out.items[0],now));
 f=feed(entity("expired","F18","F",now-200,now));assert(decode(f.data,f.n,p,now,out)&&out.count==0);
 f=feed(entity("far","F18","F",now+kUpcomingSeconds+1,0));assert(decode(f.data,f.n,p,now,out)&&out.count==0);
 f=feed(entity("line","","F",now-1,0));assert(decode(f.data,f.n,p,now,out)&&out.count==1&&out.items[0].lineWide);
 p.lineWide=false;assert(decode(f.data,f.n,p,now,out)&&out.count==0);p=Preferences{};
 f=feed(entity("full","F18","F",now-1,now+100),1);assert(!decode(f.data,f.n,p,now,out));
 f=feed(entity("full","F18","F",now-1,now+100));assert(!decode(f.data,f.n-1,p,now,out));
 const unsigned char malformed[]={0x0a,0xff,0xff,0xff};assert(!decode(malformed,sizeof(malformed),p,now,out));
 assert(!decode(nullptr,0,p,now,out));p.stations=0;assert(!decode(f.data,f.n,p,now,out));p=Preferences{};
 Bytes longFeed=feed(entity("one","F18","F",now+10,now+50));
 for(unsigned i=0;i<10;++i) {char id[16];snprintf(id,sizeof(id),"item%u",i);longFeed.msg(2,entity(id,"F18","F",now+100+i,0));}
 assert(decode(longFeed.data,longFeed.n,p,now,out)&&out.count==kMaxItems&&out.overflow&&out.matched==11);
 configure(p);state().snapshot=out;state().attempted=true;
 assert(summary(now).soonCount==kMaxItems && summary(now).available);
 state().lastFailed=true;assert(!summary(now).available);state().lastFailed=false;
 assert(!summary(now+kStaleSeconds+1).available);
 assert(due(300000,300000));state().attemptedAtMs=300000;assert(!due(300001,300000));assert(due(600000,600000));
 p.enabled=false;configure(p);assert(!due(900000,900000));
 Summary badge;badge.enabled=badge.available=true;badge.nowCount=1;badge.checkedUnix=now;
 assert(mainIndicator(badge,now));assert(!mainIndicator(badge,now+kStaleSeconds+1));
 badge.nowCount=0;badge.soonCount=2;assert(!mainIndicator(badge,now));badge.nowCount=1;
 badge.available=false;assert(!mainIndicator(badge,now));badge.available=true;badge.enabled=false;assert(!mainIndicator(badge,now));
 badge.enabled=true;assert(!mainIndicator(badge,0));assert(!mainIndicator(badge,now-1));
 puts("Station alerts: station/route matching, suffixes, current/upcoming/expiry, bounded cache, failure/stale/disabled, full-dataset and malformed protobuf PASS");
}
