// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "personalization.h"
#include "Preferences.h"
#include "personal_stubs/Arduino.h"
#include <assert.h>
#include <stdio.h>
PersonalSerial Serial;
int main() {
  using namespace brooklyn::personal;
  assert(load());assert(strcmp(snapshot().recipient,"Avi Yao")==0);
  Settings p;strcpy(p.recipient,"Sam");strcpy(p.email,"sam@example.com");
  assert(save(p));assert(strcmp(snapshot().recipient,"Sam")==0);
  apply(Settings{});assert(load());assert(strcmp(snapshot().recipient,"Sam")==0);
  const auto saved=fakeNvs::bytes();
  fakeNvs::failWrite()=true;strcpy(p.recipient,"Unsaved");assert(!save(p));
  assert(strcmp(snapshot().recipient,"Sam")==0 && fakeNvs::bytes()==saved);
  fakeNvs::failWrite()=false;p.greeting[0]='\n';assert(!save(p));assert(fakeNvs::bytes()==saved);
  fakeNvs::bytes()[8]^=1;assert(!load());assert(strcmp(snapshot().recipient,"Avi Yao")==0);
  fakeNvs::bytes()=saved;fakeNvs::bytes().pop_back();assert(!load());
  fakeNvs::bytes()=saved;fakeNvs::bytes()[0]=99;assert(!load());
  fakeNvs::bytes().clear();assert(load());assert(strcmp(snapshot().recipient,"Avi Yao")==0);
  fakeNvs::failOpen()=true;assert(!save(Settings{}));assert(load());
  puts("Personalization NVS: default, save, reload, failed-write retention, corruption/version/size fallback and cleared-namespace defaults PASS");
}

