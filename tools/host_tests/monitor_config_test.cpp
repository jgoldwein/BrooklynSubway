// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "monitor_config.h"
#include "monitor_seed.h"
#include "telemetry_core.h"
#include "admin_auth.h"
#include "lan_web_policy.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
using namespace brooklyn;
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
struct Fake {monitor::Settings settings;bool present=false,readOk=true,writeOk=true;};
bool read(void *p,monitor::Settings &s,bool &present){auto &f=*static_cast<Fake *>(p);s=f.settings;present=f.present;return f.readOk;}
bool write(void *p,const monitor::Settings &s){auto &f=*static_cast<Fake *>(p);if(!f.writeOk)return false;f.settings=s;f.present=true;return true;}
bool erase(void *p){auto &f=*static_cast<Fake *>(p);f.settings={};f.present=false;return true;}
struct Marker {bool done=false,readOk=true,writeOk=true;int reads=0;};
bool handled(void *p,const char *,bool &done){auto &m=*static_cast<Marker *>(p);++m.reads;done=m.done;return m.readOk;}
bool mark(void *p,const char *){auto &m=*static_cast<Marker *>(p);if(!m.writeOk)return false;m.done=true;return true;}
int main(){
  monitor::Settings blank,saved,next;
  CHECK(blank.enabled==1 && blank.port==8884 && !monitor::valid(blank));
  monitor::Param params[]={{"host","mqtt.example.com"},{"port","8884"},{"deviceId","11111111-1111-4111-8111-111111111111"},{"password","synthetic-monitor-key-123456789"},{"name","Avi map"},{"enabled","1"}};
  CHECK(monitor::parseSettings(params,6,blank,saved) && monitor::valid(saved) && saved.enabled);
  CHECK(!monitor::parseSettings(params,5,blank,next));
  params[5]={"name","1"};CHECK(!monitor::parseSettings(params,6,blank,next));params[5]={"enabled","1"};
  params[3].value="";CHECK(monitor::parseSettings(params,6,saved,next) && !strcmp(next.password,saved.password));
  CHECK(!monitor::parseSettings(params,6,blank,next));
  params[0].value="other.example.com";CHECK(!monitor::parseSettings(params,6,saved,next));params[0].value="mqtt.example.com";
  params[2].value="22222222-2222-4222-8222-222222222222";CHECK(!monitor::parseSettings(params,6,saved,next));params[2].value=saved.deviceId;
  params[3].value="short";CHECK(!monitor::parseSettings(params,6,saved,next));params[3].value=saved.password;
  const char *badHosts[]={"mqtt://example.com","bad/path","bad:8884","-bad.com","bad-.com","a..com","bad.com.","bad\n.com"};
  for(const char *bad:badHosts) {
    params[0].value=bad;CHECK(!monitor::parseSettings(params,6,saved,next));
  }
  params[0].value=saved.host;
  const char *badPorts[]={"0","65536","8884x","-1","","999999"};
  for(const char *bad:badPorts){params[1].value=bad;CHECK(!monitor::parseSettings(params,6,saved,next));}params[1].value="8884";
  const char *badIds[]={"","11111111-1111-1111-8111-111111111111","11111111-1111-4111-7111-111111111111","AAAAAAAA-aaaa-4aaa-8aaa-aaaaaaaaaaaa","11111111-1111-4111-8111-111111111111x"};
  for(const char *bad:badIds)CHECK(!monitor::validUuid(bad));
  params[5].value="0";CHECK(monitor::parseSettings(params,6,saved,next) && !next.enabled);
  Fake f;monitor::Store store{&f,read,write,erase};bool configured=true;
  CHECK(monitor::load(store,next,configured) && !configured);
  CHECK(store.write(store.context,saved));CHECK(monitor::load(store,next,configured) && configured);
  f.settings.enabled=2;CHECK(!monitor::load(store,next,configured) && !configured && !monitor::valid(next));
  f.settings=saved;memset(f.settings.deviceId,'a',sizeof(f.settings.deviceId));CHECK(!monitor::load(store,next,configured) && !configured);
  f.settings=saved;f.settings.version=99;CHECK(!monitor::load(store,next,configured) && !configured);
  f.readOk=false;CHECK(!monitor::load(store,next,configured) && !configured);f.readOk=true;
  CHECK(store.erase(store.context));CHECK(monitor::load(store,next,configured) && !configured);
  Marker marker;monitor::SeedStore markers{&marker,handled,mark};
  CHECK(monitor::seedOnce(saved,1,2,store,markers)==monitor::SeedResult::WrongDevice && !f.present && !marker.reads);
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::Applied && f.present && marker.done);
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::AlreadyHandled);
  // Reset/credential removal must not silently re-enable this same private seed.
  store.erase(store.context);
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::AlreadyHandled && !f.present);
  marker=Marker{};f.settings=saved;f.settings.enabled=0;f.present=true;
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::PreservedExisting && !f.settings.enabled);
  marker=Marker{};f=Fake{};marker.writeOk=false;
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::Failed && f.present && !marker.done);
  marker.writeOk=true;
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::PreservedExisting && marker.done);
  marker=Marker{};f=Fake{};f.writeOk=false;
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::Failed && !f.present && !marker.done);
  f.writeOk=true;f.readOk=false;
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::Failed && !f.present && !marker.done);
  f.readOk=true;marker.readOk=false;
  CHECK(monitor::seedOnce(saved,1,1,store,markers)==monitor::SeedResult::Failed && !f.present);
  monitor::ConnectionHistory h;
  CHECK(!h.observeWifi(false,1) && !h.wifiLosses);
  CHECK(!h.observeWifi(true,100));CHECK(h.observeWifi(false,200));CHECK(!h.observeWifi(false,300));
  CHECK(h.wifiLosses==1 && h.lastWifiLossMs==200);
  h.observeWifi(true,400);h.observeWifi(false,500);h.mqttLost(600);CHECK(h.wifiLosses==2 && h.mqttLosses==1 && h.lastMqttLossMs==600);
  h.wifiLosses=2147483647;h.observeWifi(true,700);h.observeWifi(false,800);CHECK(h.wifiLosses==2147483647);
  CHECK(monitor::resourcesAllow(false,48000,8192));CHECK(!monitor::resourcesAllow(false,47999,8192));
  CHECK(monitor::resourcesAllow(true,24000,4096));CHECK(!monitor::resourcesAllow(true,24000,4095));
  monitor::ResourceGate resources;
  CHECK(resources.allow(false,60000,10000,0));
  CHECK(!resources.allow(true,23999,10000,10));
  CHECK(!resources.allow(false,60000,10000,60009));
  CHECK(resources.allow(false,60000,10000,60010));
  CHECK(admin::isProtectedEndpoint("GET","/monitoring"));CHECK(admin::isProtectedEndpoint("GET","/api/monitoring"));
  CHECK(admin::isMutationEndpoint("POST","/monitoring"));CHECK(admin::isMutationEndpoint("POST","/monitoring/clear"));
  CHECK(!strcmp(web::loginReturnPath("monitoring"),"/monitoring"));CHECK(!strcmp(web::loginTargetForRequest("/api/monitoring"),"monitoring"));
  static char page[web::kStatusPageCapacity];monitor::Runtime runtime;
  runtime.uptimeMs=9000100;runtime.wifiDisconnects=2;runtime.mqttDisconnects=1;
  const auto n=web::renderMonitoringPage(page,sizeof(page),saved,runtime,"test-csrf");CHECK(n>0 && n<sizeof(page));
  CHECK(!strstr(page,saved.password));CHECK(strstr(page,"value=\"\"") && strstr(page,"Show key"));
  CHECK(strstr(page,"/api/monitoring") && strstr(page,"no remote control"));
  CHECK(strstr(page,"LAN and cached public WAN IPv4") && !strstr(page,"exclude Wi-Fi names, network addresses"));
  CHECK(!web::renderMonitoringPage(page,100,saved,runtime,"test-csrf"));
  CHECK(web::renderMonitoringJson(page,sizeof(page),runtime)>0);CHECK(strstr(page,"\"ackAgeSeconds\":null"));
  CHECK(!strstr(page,saved.deviceId) && !strstr(page,saved.password) && !strstr(page,saved.host));
  runtime.hasStatusAck=true;runtime.lastStatusAckMs=9000000;CHECK(web::renderMonitoringJson(page,sizeof(page),runtime)>0);CHECK(strstr(page,"\"ackAgeSeconds\":0"));
  telemetry::StatusInput input;
  input.deviceId=saved.deviceId;input.bootId="22222222-2222-4222-8222-222222222222";input.deviceName="Avi map";
  input.sequence=1;input.uptimeMs=9000100;input.unixTime=1800000000;input.firmwareTarget="brooklyn-s3";input.firmwareVersion="0.2.103";input.buildId="0.2.103";input.buildTypeBench=false;
  input.hasConnectionHistory=true;input.wifiDisconnects=2;input.mqttDisconnects=1;input.lastWifiLossMs=1001000;input.lastMqttLossMs=2001000;
  CHECK(telemetry::buildStatusJson(page,sizeof(page),input)>0);
  printf("MONITOR_SAMPLE %s\n",page);
  CHECK(strstr(page,"\"x_wifi_disconnects\":2") && strstr(page,"\"x_mqtt_loss_up_s\":2001"));
  monitor::wipe(&saved,sizeof(saved));CHECK(saved.password[0]==0);
  printf("Production monitoring configuration, privacy, disconnects and gates: %d checks PASS\n",checks);
}
