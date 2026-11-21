// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif
namespace brooklyn { namespace personal {
struct Settings {
  char title[33]="Brooklyn Subway";
  char recipient[33]="Avi Yao";
  char shortName[22]="Avi";
  char greeting[22]="Happy Bar Mitzvah!";
  char from[22]="From the Goldweins";
  char names[129]="Joel, Marlene, Marc, Kim, Juliana, Levi, Robbie, Eric";
  char date[11]="2026-09-26";
  char creator[49]="Joel W. Goldwein";
  char support[33]="Joel";
  char email[81]="goldwein@gmail.com";
};
struct Field { const char *key; const char *label; size_t offset,size; bool optional; };
inline const Field *fields() {
  static const Field list[]={
#define F(key,label,optional) {#key,label,offsetof(Settings,key),sizeof(Settings::key),optional}
    F(title,"Map title",false),F(recipient,"Recipient name",false),F(shortName,"Name on OLED greeting",false),
    F(greeting,"Greeting",false),F(from,"From line",true),F(names,"Scrolling names",true),
    F(date,"Celebration date (YYYY-MM-DD)",true),F(creator,"Made by",true),
    F(support,"Support contact name",false),F(email,"Support email (optional)",true)
#undef F
  };return list;
}
constexpr unsigned kFieldCount=10;
inline const char *value(const Settings &s,unsigned i) {return reinterpret_cast<const char *>(&s)+fields()[i].offset;}
inline char *value(Settings &s,unsigned i) {return reinterpret_cast<char *>(&s)+fields()[i].offset;}
inline bool dateParts(const char *s,int &year,int &month,int &day) {
  if(strlen(s)!=10 || s[4]!='-' || s[7]!='-') return false;
  for(unsigned i=0;i<10;++i)if(i!=4 && i!=7 && (s[i]<'0'||s[i]>'9'))return false;
  year=(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+s[3]-'0';
  month=(s[5]-'0')*10+s[6]-'0';day=(s[8]-'0')*10+s[9]-'0';
  const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  return year>=1900 && year<=2199 && month>=1 && month<=12 && day>=1 && day<=days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
}
inline bool valid(const Settings &s) {
  for(unsigned i=0;i<kFieldCount;++i) {
    const char *v=value(s,i);const Field &f=fields()[i];size_t n=0;
    for(;n<f.size && v[n];++n) if(static_cast<unsigned char>(v[n])<32 || static_cast<unsigned char>(v[n])>126)return false;
    if(n==f.size || (!n && !f.optional))return false;
    if(n && (v[0]==' ' || v[n-1]==' '))return false;
  }
  int y,m,d;if(s.date[0] && !dateParts(s.date,y,m,d))return false;
  if(!s.email[0])return true; // Copy/Save reports can be used without email.
  const char *at=strchr(s.email,'@');
  if(!at || at==s.email || strchr(at+1,'@') || !strchr(at+1,'.') || !at[1])return false;
  for(const char *p=s.email;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("._+-@",*p)))return false;
  return true;
}
inline Settings &storage() {static Settings s;return s;}
#if defined(ARDUINO_ARCH_ESP32)
inline portMUX_TYPE &mutex() {static portMUX_TYPE m=portMUX_INITIALIZER_UNLOCKED;return m;}
#endif
inline Settings snapshot() {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mutex());
#endif
  const Settings copy=storage();
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mutex());
#endif
  return copy;
}
inline void apply(const Settings &s) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mutex());
#endif
  storage()=s;
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mutex());
#endif
}

// Individual PCB label, independent of the greeting and monitoring credentials.
// Empty internal value means use the older monitoring label (upgrade compatibility).
struct MapName {char text[33]{};};
inline bool validMapName(const MapName &s) {
  size_t n=0;
  for(;n<sizeof(s.text) && s.text[n];++n)
    if(static_cast<unsigned char>(s.text[n])<32 || static_cast<unsigned char>(s.text[n])>126)return false;
  return n>0 && n<sizeof(s.text) && s.text[0]!=' ' && s.text[n-1]!=' ';
}
inline MapName &mapNameStorage() {static MapName s;return s;}
inline void applyMapName(const MapName &s) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mutex());
#endif
  mapNameStorage()=s;
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mutex());
#endif
}
inline MapName mapName(const char *fallback="Brooklyn map") {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mutex());
#endif
  MapName copy=mapNameStorage();
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mutex());
#endif
  if(!copy.text[0])snprintf(copy.text,sizeof(copy.text),"%s",fallback);
  return copy;
}
bool loadMapName();
bool saveMapName(const MapName &s);

struct ExtraSettings {
  uint8_t enabledMask=15; // Celebration, anniversary, family credits, Rangers.
  char celebrationNote[22]="Let's celebrate!";
  char anniversaryNote[22]="A day to remember!";
  char creditsTitle[22]=""; // Empty: For <recipient>.
  char teamTitle[22]="New York Rangers";
  char teamNote[22]=""; // Empty: <short name>'s favorite team.
  char teamCheer[22]="GO RANGERS!";
};
constexpr unsigned kExtraFieldCount=6;
inline const Field *extraFields() {
  static const Field list[]={
#define E(key,label,optional) {#key,label,offsetof(ExtraSettings,key),sizeof(ExtraSettings::key),optional}
    E(celebrationNote,"Celebration message",true),E(anniversaryNote,"Anniversary message",true),
    E(creditsTitle,"Family credits heading (blank = For recipient)",true),
    E(teamTitle,"Team heading (sound and colors stay Rangers)",false),
    E(teamNote,"Team caption (blank = name's favorite team)",true),
    E(teamCheer,"Flashing team cheer",true)
#undef E
  };return list;
}
inline const char *extraValue(const ExtraSettings &s,unsigned i) {return reinterpret_cast<const char *>(&s)+extraFields()[i].offset;}
inline char *extraValue(ExtraSettings &s,unsigned i) {return reinterpret_cast<char *>(&s)+extraFields()[i].offset;}
inline bool validExtras(const ExtraSettings &s) {
  if(s.enabledMask&~15u)return false;
  for(unsigned i=0;i<kExtraFieldCount;++i) {
    const auto &f=extraFields()[i];const char *v=extraValue(s,i);size_t n=0;
    for(;n<f.size && v[n];++n)if(static_cast<unsigned char>(v[n])<32 || static_cast<unsigned char>(v[n])>126)return false;
    if(n==f.size || (!n && !f.optional) || (n && (v[0]==' ' || v[n-1]==' ')))return false;
  }return true;
}
inline ExtraSettings &extraStorage() {static ExtraSettings s;return s;}
inline ExtraSettings extraSnapshot() {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mutex());
#endif
  const ExtraSettings s=extraStorage();
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mutex());
#endif
  return s;
}
inline void applyExtras(const ExtraSettings &s) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mutex());
#endif
  extraStorage()=s;
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mutex());
#endif
}
bool loadExtras();
bool saveExtras(const ExtraSettings &s);
inline void dateText(const Settings &s,char (&out)[22]) {
  int y,m,d;out[0]=0;if(!dateParts(s.date,y,m,d))return;
  const char *months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
  snprintf(out,sizeof(out),"%s %d, %d",months[m-1],d,y);
}
// Expand explicit tokens in trusted templates; escaped values are never reparsed.
template<class Emit> bool htmlTemplate(const char *text,const Settings &s,Emit emit) {
  while(*text) {
    bool found=false;
    if(text[0]=='{' && text[1]=='{')for(unsigned i=0;i<kFieldCount;++i) {
      const size_t n=strlen(fields()[i].key);
      if(strncmp(text+2,fields()[i].key,n)==0 && text[n+2]=='}' && text[n+3]=='}') {
        for(const char *v=value(s,i);*v;++v) {
          const char *e=*v=='&'?"&amp;":*v=='<'?"&lt;":*v=='>'?"&gt;":*v=='\"'?"&quot;":*v=='\''?"&#39;":nullptr;
          if(e) {while(*e)if(!emit(*e++))return false;}else if(!emit(*v))return false;
        }
        text+=n+4;found=true;break;
      }
    }
    if(!found && !emit(*text++))return false;
  }return true;
}
bool load();
bool save(const Settings &s);
}}
