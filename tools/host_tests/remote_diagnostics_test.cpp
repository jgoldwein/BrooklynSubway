// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "remote_diagnostics.h"
#include "telemetry_core.h"
#include "monitor_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace brooklyn::telemetry;
int checks=0;
#define CHECK(c) do {++checks;if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
int main(){
  RestartRecord r{};
  CHECK(takeRestart(r,true)==RestartIntent::None);
  for(unsigned i=1;i<=4;++i){rememberRestart(r,static_cast<RestartIntent>(i));CHECK(takeRestart(r,true)==static_cast<RestartIntent>(i));CHECK(takeRestart(r,true)==RestartIntent::None);}
  rememberRestart(r,RestartIntent::Web);CHECK(takeRestart(r,false)==RestartIntent::None);
  rememberRestart(r,RestartIntent::Web);r.inverse=0;CHECK(takeRestart(r,true)==RestartIntent::None);
  brooklyn::monitor::ConnectionHistory h;
  CHECK(!h.observeWifi(false,10));CHECK(!h.observeWifi(true,20));CHECK(h.observeWifi(false,30));
  CHECK(h.wifiOutage && h.wifiLosses==1);h.observeWifi(false,40);CHECK(h.wifiLosses==1);
  h.observeWifi(true,55);CHECK(!h.wifiOutage && h.wifiOutageMs==25 && h.wifiRecoveryMs==55);
  h.mqttRecovered(30);CHECK(h.mqttRecoveryMs==0);h.mqttLost(100);h.mqttLost(150);
  CHECK(h.mqttLosses==1 && h.lastMqttLossMs==100);h.mqttRecovered(330);CHECK(h.mqttOutageMs==230 && !h.mqttOutage);
  Core core;core.begin("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",nullptr,nullptr);
  Inputs in;in.networkReady=in.wifiUp=in.timeValid=in.credentials=in.diagnostics=true;
  CHECK(core.next(0,in).type==ActionType::None);auto a=core.next(500,in);CHECK(a.type==ActionType::StartClient);
  core.onClientStarted(true,500);core.onConnected(501);a=core.next(502,in);CHECK(a.type==ActionType::PublishOnline);
  core.publishResult(a,PublishResult::Queued,1,502);core.onPublished(1,503);
  a=core.next(504,in);CHECK(a.type==ActionType::PublishStatus);core.publishResult(a,PublishResult::Queued,2,504);core.onPublished(2,505);
  a=core.next(506,in);CHECK(a.type==ActionType::PublishDiagnostics);const auto seq=a.sequence;
  core.publishResult(a,PublishResult::Queued,3,506);core.requestDiagnostics();CHECK(core.next(507,in).type==ActionType::None);
  core.onPublished(3,508);CHECK(core.next(509,in).type==ActionType::None);
  a=core.next(5506,in);CHECK(a.type==ActionType::PublishDiagnostics && a.sequence>seq);
  core.publishResult(a,PublishResult::TooLarge,0,5506);CHECK(core.next(5507,in).type==ActionType::None);
  core.requestDiagnostics(true);a=core.next(5508,in);CHECK(a.type==ActionType::PublishDiagnostics);
  core.publishResult(a,PublishResult::Queued,4,5508);core.onPublished(4,5509);
  in.diagnostics=false;core.requestDiagnostics();CHECK(core.next(11000,in).type==ActionType::None);
  Diagnostics d;CHECK(!strcmp(ledOffReason(d),"brightness_zero"));d.quiet=true;d.quietMapCap=0;CHECK(!strcmp(ledOffReason(d),"quiet_hours"));
  d.powerMode=1;CHECK(!strcmp(ledOffReason(d),"power_button"));d.maintenance=true;CHECK(!strcmp(ledOffReason(d),"test_or_recovery"));
  d.systemHealthy=false;CHECK(!strcmp(ledOffReason(d),"system_fault"));d.ledsOn=true;
  CHECK(!strcmp(ledOffReason(d),"brightness_zero"));d.brightness=5;
  CHECK(!strcmp(ledOffReason(d),"routes_hidden"));d.routeMask=1;
  CHECK(!strcmp(ledOffReason(d),"quiet_hours_dimmed"));d.quiet=false;
  CHECK(!strcmp(ledOffReason(d),"on"));
  char out[4097];const char *id="11111111-1111-4111-8111-111111111111",*boot="aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
  CHECK(buildDiagnosticsJson(out,sizeof(out),id,boot,3,10000,1800000000,d)>0);
  CHECK(strstr(out,"\"last_button_age_s\":null"));printf("DIAGNOSTICS %s\n",out);
  d.hasButton=d.hasInput=true;d.lastButtonMs=2000;d.lastInputMs=9000;d.restart=RestartIntent::FirmwareUpdate;d.imageConfirmed=true;
  d.settingsRevision=1;strcpy(d.settingsGroup,"Brightness\"\\\n");d.wifiOutage=true;d.wifiLossMs=1000;d.hasError=true;d.errorTls=-9984;
  CHECK(buildDiagnosticsJson(out,sizeof(out),id,boot,4,10000,1800000000,d)>0);
  CHECK(strstr(out,"\"last_button_age_s\":8") && strstr(out,"\"last_control_age_s\":1"));
  CHECK(strstr(out,"\"wifi_outage_duration_s\":9") && strstr(out,"\"last_update_outcome\":\"confirmed\""));printf("DIAGNOSTICS %s\n",out);
  const size_t needed=buildDiagnosticsJson(out,sizeof(out),id,boot,4,10000,0,d);
  for(size_t i=0;i<=needed;++i)CHECK(buildDiagnosticsJson(out,i,id,boot,4,10000,0,d)==0);
  CHECK(buildDiagnosticsJson(nullptr,4097,id,boot,4,10000,0,d)==0);
  d.lastButtonMs=11000;CHECK(buildDiagnosticsJson(out,sizeof(out),id,boot,5,10000,0,d)>0);CHECK(strstr(out,"\"last_button_age_s\":null"));printf("DIAGNOSTICS %s\n",out);
  printf("Remote diagnostics %d checks PASS; record %u bytes\n",checks,unsigned(sizeof(Diagnostics)));
}
