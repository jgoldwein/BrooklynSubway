// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "personalization.h"
#include "Preferences.h"
#include "personal_stubs/Arduino.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
PersonalSerial Serial;
int main() {
  using namespace brooklyn::personal;
  assert(load());assert(strcmp(snapshot().recipient,"Avi Yao")==0);
  assert(valid(Settings{}) && strcmp(snapshot().support,"Joel")==0);
  {Settings noMail;noMail.email[0]=0;assert(valid(noMail));}
  Settings p;strcpy(p.recipient,"Sam");strcpy(p.email,"sam@example.com");
  assert(save(p));assert(strcmp(snapshot().recipient,"Sam")==0);
  apply(Settings{});assert(load());assert(strcmp(snapshot().recipient,"Sam")==0);
  const auto saved=fakeNvs::bytes();
  {ExtraSettings e;e.enabledMask=7;assert(saveExtras(e));assert(fakeNvs::bytes()==saved);fakeNvs::extraBytes().clear();applyExtras(ExtraSettings{});}
  fakeNvs::failWrite()=true;strcpy(p.recipient,"Unsaved");assert(!save(p));
  assert(strcmp(snapshot().recipient,"Sam")==0 && fakeNvs::bytes()==saved);
  fakeNvs::failWrite()=false;p.greeting[0]='\n';assert(!save(p));assert(fakeNvs::bytes()==saved);
  fakeNvs::bytes()[8]^=1;assert(!load());assert(strcmp(snapshot().recipient,"Avi Yao")==0);
  fakeNvs::bytes()=saved;fakeNvs::bytes().pop_back();assert(!load());
  fakeNvs::bytes()=saved;fakeNvs::bytes()[0]=99;assert(!load());
  fakeNvs::bytes().clear();assert(load());assert(strcmp(snapshot().recipient,"Avi Yao")==0);
  fakeNvs::failOpen()=true;assert(!save(Settings{}));assert(load());
  fakeNvs::failOpen()=false;ExtraSettings extras;extras.enabledMask=8;strcpy(extras.teamNote,"Sam's favorite team");
  assert(saveExtras(extras));applyExtras(ExtraSettings{});assert(loadExtras());assert(extraSnapshot().enabledMask==8);
  assert(strcmp(extraSnapshot().teamNote,"Sam's favorite team")==0);
  assert(snapshot().recipient[0]);const auto extraSaved=fakeNvs::extraBytes();
  fakeNvs::failWrite()=true;extras.enabledMask=0;assert(!saveExtras(extras));assert(extraSnapshot().enabledMask==8);
  assert(fakeNvs::extraBytes()==extraSaved);fakeNvs::failWrite()=false;
  fakeNvs::extraBytes()[7]^=1;assert(!loadExtras());assert(extraSnapshot().enabledMask==15);
  fakeNvs::extraBytes().clear();assert(loadExtras());extras.enabledMask=16;assert(!saveExtras(extras));
  assert(extraSnapshot().enabledMask==15);
  {Settings noMail=snapshot();noMail.email[0]=0;assert(save(noMail));assert(load());assert(snapshot().email[0]==0);}
  puts("Personalization NVS: default, save, reload, failed-write retention, corruption/version/size fallback and cleared-namespace defaults PASS");
  // New names must not change the existing personalization blob or depend on MQTT.
  const auto oldPersonal=fakeNvs::bytes();const auto oldExtras=fakeNvs::extraBytes();
  assert(strcmp(mapName("Legacy monitoring name").text,"Legacy monitoring name")==0);
  MapName name;strcpy(name.text,"Living room map");assert(saveMapName(name));
  assert(fakeNvs::bytes()==oldPersonal && fakeNvs::extraBytes()==oldExtras);
  applyMapName(MapName{});assert(loadMapName());assert(strcmp(mapName().text,"Living room map")==0);
  const auto savedName=fakeNvs::nameBytes();
  fakeNvs::failWrite()=true;strcpy(name.text,"Unsaved name");assert(!saveMapName(name));
  assert(strcmp(mapName().text,"Living room map")==0 && fakeNvs::nameBytes()==savedName);fakeNvs::failWrite()=false;
  fakeNvs::nameBytes()[8]^=1;assert(!loadMapName());assert(strcmp(mapName("Legacy name").text,"Legacy name")==0);
  fakeNvs::nameBytes()=savedName;fakeNvs::nameBytes().pop_back();assert(!loadMapName());
  fakeNvs::nameBytes()=savedName;fakeNvs::nameBytes()[0]=99;assert(!loadMapName());
  fakeNvs::nameBytes().clear();assert(loadMapName());assert(strcmp(mapName().text,"Brooklyn map")==0);
  const char *badNames[]={""," Leading","Trailing ","Bad\nname"};
  for(const char *bad:badNames) {MapName n;strcpy(n.text,bad);assert(!saveMapName(n));}
  MapName full;memset(full.text,'a',32);assert(validMapName(full));full.text[32]='a';assert(!validMapName(full));
  assert(fakeNvs::bytes()==oldPersonal && fakeNvs::extraBytes()==oldExtras);
  puts("Map name NVS: legacy fallback, independent save, reload, write failure, corruption, reset and validation PASS");
}

