// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "station_alerts.h"
#include <string.h>
namespace brooklyn { namespace alerts {
namespace {
struct Reader {
  const uint8_t *p,*end;bool ok=true;
  Reader(const uint8_t *start,const uint8_t *finish):p(start),end(finish) {}
  bool varint(uint64_t &v) {
    v=0;for(unsigned shift=0;shift<64;shift+=7) {
      if(p==end) {ok=false;return false;}const uint8_t b=*p++;
      if(shift==63 && (b&254)) {ok=false;return false;}
      v|=uint64_t(b&127)<<shift;if(!(b&128))return true;
    }ok=false;return false;
  }
  bool next(uint32_t &number,uint8_t &wire,uint64_t &value,Reader &bytes) {
    if(p==end)return false;uint64_t tag;
    if(!varint(tag) || !(tag>>3) || (tag>>3)>0x1fffffff) {ok=false;return false;}
    number=tag>>3;wire=tag&7;value=0;bytes={p,p};
    if(wire==0)return varint(value);
    if(wire==2) {uint64_t n;if(!varint(n) || n>uint64_t(end-p)) {ok=false;return false;}bytes={p,p+n};p+=n;return true;}
    const unsigned n=wire==1?8:wire==5?4:0;
    if(!n || size_t(end-p)<n) {ok=false;return false;}p+=n;return true;
  }
};
bool text(Reader r,char *out,size_t capacity,bool &shortened) {
  size_t used=0;
  while(r.p<r.end) {
    uint8_t c=*r.p++;if(!c)return false;
    if(c>=128) {while(r.p<r.end && (*r.p&192)==128)++r.p;c='?';}
    if(c<32 && c!='\n' && c!='\t' && c!='\r')c=' ';
    if(used+1<capacity)out[used++]=char(c);else shortened=true;
  }out[used]=0;return true;
}
bool translated(Reader r,char *out,size_t capacity,bool &shortened) {
  uint32_t n;uint8_t w;uint64_t v;Reader b{nullptr,nullptr};bool selected=false;
  while(r.next(n,w,v,b)) {
    if(n!=1 || w!=2)continue;
    Reader t=b,content{nullptr,nullptr};char language[16]{};bool haveText=false,haveLanguage=false;
    while(t.next(n,w,v,b)) {
      if(n==1 && w==2) {if(haveText)return false;content=b;haveText=true;}
      if(n==2 && w==2) {if(haveLanguage)return false;bool cut=false;if(!text(b,language,sizeof(language),cut) || cut)return false;haveLanguage=true;}
    }
    if(!t.ok)return false;
    if(haveText && (!language[0] || strcmp(language,"en")==0) && !selected) {
      if(!text(content,out,capacity,shortened))return false;selected=true;
    }
  }return r.ok;
}
uint8_t stationBit(const char *s) {
  char id[16]{};const size_t length=strlen(s);if(length>=sizeof(id))return 0;
  memcpy(id,s,length+1);if(length && (id[length-1]=='N'||id[length-1]=='S'))id[length-1]=0;
  return strcmp(id,"F18")==0?1:strcmp(id,"A40")==0?2:(strcmp(id,"A41")==0||strcmp(id,"R29")==0)?4:0;
}
bool selectedRoute(const char *route,uint8_t stations) {
  if(stations&1)if(strcmp(route,"F")==0 || strcmp(route,"FX")==0)return true;
  if(stations&2)if(strcmp(route,"A")==0 || strcmp(route,"C")==0)return true;
  if(stations&4)if(strcmp(route,"A")==0 || strcmp(route,"C")==0 || strcmp(route,"F")==0 || strcmp(route,"FX")==0 || strcmp(route,"R")==0)return true;
  return false;
}
bool selector(Reader r,const Preferences &prefs,Item &item) {
  uint32_t n;uint8_t w;uint64_t v;Reader b{nullptr,nullptr};char agency[24]{},route[16]{},stop[16]{};bool cut=false;
  while(r.next(n,w,v,b)) {
    if(w!=2)continue;
    if(n==1 && !text(b,agency,sizeof(agency),cut))return false;
    if(n==2 && !text(b,route,sizeof(route),cut))return false;
    if(n==5 && !text(b,stop,sizeof(stop),cut))return false;
  }
  if(!r.ok || cut)return false;
  if(agency[0] && strcmp(agency,"MTASBWY")!=0)return true;
  item.stations|=stationBit(stop)&prefs.stations;
  if(!stop[0] && prefs.lineWide && selectedRoute(route,prefs.stations))item.lineWide=true;
  return true;
}
bool period(Reader r,uint32_t now,Item &item,bool &haveWindow) {
  uint32_t n;uint8_t w;uint64_t v;Reader b{nullptr,nullptr};uint32_t start=0,end=0;
  while(r.next(n,w,v,b))if((n==1 || n==2) && w==0) {
    if(v>UINT32_MAX)return false;if(n==1)start=uint32_t(v);else end=uint32_t(v);
  }
  if(!r.ok || (end && end<start))return false;
  if(end && now>=end)return true;
  if(start>now && start-now>kUpcomingSeconds)return true;
  const bool active=start<=now;
  if(!haveWindow || (active && item.start>now) || (active==(item.start<=now) && start<item.start)) {
    item.start=start;item.end=end;haveWindow=true;
  }return true;
}
bool alert(Reader r,const Preferences &prefs,uint32_t now,Item &item,bool &keep) {
  uint32_t n;uint8_t w;uint64_t v;Reader b{nullptr,nullptr};bool hadPeriod=false,window=false,head=false,desc=false;
  while(r.next(n,w,v,b)) {
    if(w!=2)continue;
    if(n==1) {hadPeriod=true;if(!period(b,now,item,window))return false;}
    else if(n==5) {if(!selector(b,prefs,item))return false;}
    else if(n==10) {if(head || !translated(b,item.headline,sizeof(item.headline),item.shortened))return false;head=true;}
    else if(n==11) {if(desc || !translated(b,item.description,sizeof(item.description),item.shortened))return false;desc=true;}
  }
  if(!r.ok)return false;
  keep=(item.stations || item.lineWide) && (!hadPeriod || window);
  if(keep && !item.headline[0])return false;
  return true;
}
unsigned rank(const Item &i,uint32_t now) {return (i.start>now?2:0)+(i.stations?0:1);}
void insert(Snapshot &out,const Item &item,uint32_t now) {
  ++out.matched;
  for(uint8_t i=0;i<out.count;++i)if(strcmp(out.items[i].id,item.id)==0)return;
  uint8_t at=0;
  while(at<out.count && (rank(out.items[at],now)<rank(item,now) ||
      (rank(out.items[at],now)==rank(item,now) && out.items[at].start<=item.start)))++at;
  if(out.count==kMaxItems)out.overflow=true;else ++out.count;
  if(at>=kMaxItems)return;
  for(uint8_t i=out.count-1;i>at;--i)out.items[i]=out.items[i-1];out.items[at]=item;
}
bool header(Reader r,uint32_t now,uint32_t &stamp) {
  uint32_t n;uint8_t w;uint64_t v;Reader b{nullptr,nullptr};char version[8]{};bool cut=false;
  while(r.next(n,w,v,b)) {
    if(n==1 && w==2 && !text(b,version,sizeof(version),cut))return false;
    if(n==2 && w==0 && v!=0)return false; // Only a full dataset may replace the cache.
    if(n==3 && w==0) {if(v>UINT32_MAX)return false;stamp=uint32_t(v);}
  }
  return r.ok && !cut && (strcmp(version,"2.0")==0 || strcmp(version,"1.0")==0) && stamp>=1700000000 && (stamp<=now || stamp-now<=300);
}
}
bool decode(const uint8_t *data,size_t length,const Preferences &prefs,uint32_t now,Snapshot &out) {
  out=Snapshot{};
  if(!data || !length || length>kMaxBody || now<1700000000 || !validPreferences(prefs))return false;
  Reader r{data,data+length},b{nullptr,nullptr};uint32_t n;uint8_t w;uint64_t v;bool haveHeader=false;uint16_t entities=0;
  while(r.next(n,w,v,b)) {
    if(n==1 && w==2) {if(haveHeader || !header(b,now,out.sourceUnix))return false;haveHeader=true;}
    if(n==2 && w==2) {
      if(++entities>2048)return false;Reader e=b,bytes{nullptr,nullptr};Item item;bool haveAlert=false,keep=false;
      while(e.next(n,w,v,bytes)) {
        if(n==1 && w==2) {bool cut=false;if(!text(bytes,item.id,sizeof(item.id),cut)||cut)return false;}
        if(n==2 && w==0 && v)return false;
        if(n==5 && w==2) {if(haveAlert || !alert(bytes,prefs,now,item,keep))return false;haveAlert=true;}
      }
      if(!e.ok)return false;if(keep) {if(!item.id[0])return false;insert(out,item,now);}
    }
  }
  if(!r.ok || !haveHeader)return false;
  out.checkedUnix=now;out.available=true;return true;
}
}}
