// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "station_alerts.h"
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <stdlib.h>
PersonalSerial Serial;
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main() {
 using namespace brooklyn::alerts;
 const unsigned char old[]={1,2,3};fakeNvs::bytes().assign(old,old+3);
 CHECK(loadPreferences() && settingsWord(state().preferences)==25);
 brooklyn::alerts::Preferences p;p.stations=6;p.lineWide=false;
 CHECK(savePreferences(p));configure(brooklyn::alerts::Preferences{});CHECK(loadPreferences());CHECK(settingsWord(state().preferences)==14);
 CHECK(fakeNvs::bytes().size()==3 && fakeNvs::bytes()[0]==1);
 p.stations=0;CHECK(!savePreferences(p));CHECK(state().preferences.stations==6);
 p.stations=2;fakeNvs::failWrite()=true;CHECK(!savePreferences(p));CHECK(state().preferences.stations==6);fakeNvs::failWrite()=false;
 fakeNvs::alertWord()=0x1234;CHECK(!loadPreferences());CHECK(settingsWord(state().preferences)==25);
 p.enabled=false;p.stations=0;CHECK(savePreferences(p));CHECK(loadPreferences() && !state().preferences.enabled);
 puts("Station alert preferences: defaults, persistence, validation, failed writes and original personal record preserved PASS");
}
