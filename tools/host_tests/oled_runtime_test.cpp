#include "oled_clock.h"
#include <Arduino.h>
#include <Wire.h>
#include "check.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
jmp_buf iterationEnd;
FakeSerial Serial; FakeWire Wire;
uint32_t fakeNowMs=120000;
uint32_t millis() { return fakeNowMs; }
char lastScreenTitle[24]{};
int borderDraws=0;
extern int scenario;
extern "C" time_t time(time_t *out) {const time_t value=scenario==46?1790395200:1768498496;if(out)*out=value;return value;}
int setupTitles=0,setupNames=0,setupAddresses=0;
int frames=0,titles=0,ends=0,starts=0,deletes=0,delays=0;
char lastTrainText[24]={};
int powerCommands=0;
int retryDelays=0, contrastCalls=0; uint8_t lastContrast=207;
int scenario=0; bool open=false; uint8_t lastAddress=0;
void (*worker)(void *)=nullptr;
void FakeSerial::println(const char *text) {
 if(scenario>=17 && scenario<=20 && strcmp(text,"OLED clock: initialization and first frame complete")==0) {
   brooklyn::oled::setEnabled(false);
   brooklyn::oled::showBrightness(scenario==18,5,scenario==18?255:16);
 }
 if (scenario>=15 && scenario<=20 && strcmp(text,"OLED clock: initialization and first frame complete")==0) brooklyn::oled::setEnabled(false);
 if ((scenario==12 || scenario==14) && strcmp(text,"OLED clock: initialization and first frame complete")==0)
   brooklyn::oled::setContrast(5);
}
int digitalRead(uint8_t pin){OLED_CHECK(pin==8||pin==9); return scenario==1?LOW:HIGH;}
int xTaskCreatePinnedToCore(void (*f)(void *),const char *,uint32_t stack,void *,int priority,TaskHandle_t *handle,int core){
 OLED_CHECK(stack==4096 && priority==1 && core==1);
 if(scenario==6)return 0; worker=f; *handle=reinterpret_cast<void *>(1);return pdPASS;
}
void delay(uint32_t ms){OLED_CHECK(ms==100);++retryDelays;}
void vTaskDelete(void *) {++deletes;}
void vTaskDelay(uint32_t ms) {
 OLED_CHECK(ms==20);++delays;fakeNowMs+=(scenario==19 || scenario==39)?1000:20;
 if(scenario==43 && delays==1) {brooklyn::oled::clearEasterEgg();return;}
 if(scenario==46 && delays<=2) {
   using namespace brooklyn::oled;
   if(delays==2) {OLED_CHECK(selectedScreen()==Screen::Dedication);navigate(Action::NextScreen,fakeNowMs);}
   serviceNavigation(fakeNowMs,true);return;
 }
 if(scenario==40 && delays==1) {brooklyn::oled::clearLineTest();return;}
 if(scenario==33 && delays==1) {brooklyn::oled::advanceScreen();return;}
 if(scenario==34 && delays==1) {brooklyn::oled::navigate(brooklyn::oled::Action::TurnNext,fakeNowMs);return;}
 if(scenario==35 && delays<51) return;
 if(scenario==39 && delays<7) {
   if(delays==6) {brooklyn::oled::ScreenData data;data.wifiConnected=true;brooklyn::oled::publishScreenData(data);}
   brooklyn::oled::serviceNavigation(fakeNowMs,true);
   if(delays==5) OLED_CHECK(brooklyn::oled::selectedScreen()==brooklyn::oled::Screen::Dedication);
   return;
 }
 if(scenario==26 && delays==1) {brooklyn::oled::setSetupMode(false);return;}
 if(scenario==19 && delays<5) return;
 if(scenario==20 && delays==1) {brooklyn::oled::clearBrightnessFeedback();return;}
 if(scenario==16 && delays==1) {brooklyn::oled::setEnabled(true);return;}
 longjmp(iterationEnd,1);
}

void FakeWire::setTimeOut(uint16_t ms){OLED_CHECK(ms==50);}
void FakeWire::setClock(uint32_t hz){OLED_CHECK(hz==100000);}
void FakeWire::beginTransmission(uint8_t a){OLED_CHECK(!open); OLED_CHECK(a==0x38||a==0x3c||a==0x3d);lastAddress=a;open=true;++starts;}
uint8_t FakeWire::endTransmission(){OLED_CHECK(open);open=false;++ends;
 if(scenario==2)return 2;
 if(scenario==7 && lastAddress==0x38)return 2;
 if(scenario==9 && ends<=3)return 2;
 if(scenario==13 && ends>17)return 5;
 if(scenario==14 && ends>33)return 5;
 if(scenario==3 && ends>=2)return 5;
 if(scenario==5 && ends>17)return 5;
 if(scenario==10 && ends>33)return 5;
 return 0;
}
size_t FakeWire::write(uint8_t *,size_t n){OLED_CHECK(open);return scenario==4?0:n;}
int main(int argc,char **argv){OLED_CHECK(argc==2);scenario=atoi(argv[1]);
 brooklyn::oled::setScreenPreferences(brooklyn::config::kDefaultScreenPreferences & ~brooklyn::config::kStartupDedicationBit);
 if(scenario==37 || scenario==39) brooklyn::oled::setScreenPreferences(brooklyn::config::kDefaultScreenPreferences);
 if(scenario==38) {brooklyn::config::ScreenPreferences p;p.quietEnabled=true;p.startupDedication=false;p.startMinute=600;p.endMinute=840;brooklyn::oled::setScreenPreferences(brooklyn::config::packScreenPreferences(p));}

 if(scenario==8){brooklyn::oled::begin(0x07); OLED_CHECK(!worker && starts==0); puts("OLED invalid address rejected PASS"); return 0;}
 if(scenario==21) brooklyn::oled::setTrackedTrains(132,true);
 if(scenario==22) brooklyn::oled::setTrackedTrains(0,true);
 if(scenario==23) {brooklyn::oled::setTrackedTrains(132,true);brooklyn::oled::setTrackedTrains(132,false);}
 if(scenario==24) {brooklyn::oled::setTrackedTrains(80,true);brooklyn::oled::setVisibleMapLights(12);}
 if(scenario==25||scenario==26) brooklyn::oled::setSetupMode(true);
 if(scenario>=27 && scenario<=32) {
   brooklyn::oled::ScreenData d; d.wifiConnected=true; d.hasUpdate=true; d.updateAgeSeconds=15;
   d.sampledAtMs=119000; d.indications={10,20,3,5}; d.lightsEnabled=true;
   d.feedsSucceeded=8; d.feedsTotal=8; d.feedHealth=brooklyn::oled::FeedHealth::Fresh;
   d.rssi=-70; strcpy(d.ip,"255.255.255.255");strcpy(d.firmware,"0.2.51");
   brooklyn::oled::publishScreenData(d);
   if(scenario==32) {
     using namespace brooklyn::oled;
     ScreenText out; formatScreen(Screen::Connection,d,120000,out);
     OLED_CHECK(strcmp(out.lines[3],"Updated 16 sec ago")==0);
     d.sampledAtMs=UINT32_MAX-499; d.updateAgeSeconds=60;
     formatScreen(Screen::Connection,d,500,out);
     OLED_CHECK(strcmp(out.lines[3],"Updated 61 sec ago")==0);
     d.updateAgeSeconds=UINT32_MAX; formatScreen(Screen::Connection,d,500,out);
     OLED_CHECK(strcmp(out.lines[3],"Data is old: 99999s")==0);
     d.wifiConnected=false;d.hasUpdate=false;d.feedsTotal=0;
     formatScreen(Screen::Connection,d,500,out);
     OLED_CHECK(strcmp(out.lines[1],"Wi-Fi not connected")==0 && strcmp(out.lines[3],"Waiting for trains")==0);
     const FeedHealth healthValues[]={FeedHealth::Waiting, FeedHealth::Fresh, FeedHealth::Partial, FeedHealth::Held, FeedHealth::Stale, FeedHealth::NoData};
     for(auto health : healthValues) {
       d.feedHealth=health;d.feedsTotal=255;d.feedsSucceeded=255;
       const Screen pages[]={Screen::Activity, Screen::Connection, Screen::Device, Screen::Lines};
       for(auto page : pages) {
         formatScreen(page,d,UINT32_MAX,out);
         for(auto &line : out.lines) OLED_CHECK(strlen(line)<=21);
       }
     }
     OLED_CHECK(nextScreen(Screen::Dedication)==Screen::Main);
     puts("OLED page formatting, age rollover, unavailable data and text bounds PASS"); return 0;
   }
   brooklyn::oled::advanceScreen();
   if(scenario==28) brooklyn::oled::advanceScreen();
   if(scenario==29) { brooklyn::oled::advanceScreen(); brooklyn::oled::advanceScreen(); }
   if(scenario==30) {brooklyn::oled::setSetupMode(true);brooklyn::oled::advanceScreen();OLED_CHECK(brooklyn::oled::selectedScreen()==brooklyn::oled::Screen::Activity);}
   if(scenario==31) brooklyn::oled::setEnabled(false);
 }

 if(scenario==34) brooklyn::oled::navigate(brooklyn::oled::Action::OpenLines,fakeNowMs);
 if(scenario==36) brooklyn::oled::navigate(brooklyn::oled::Action::TurnPrevious,fakeNowMs);
 if(scenario>=40 && scenario<=42) {
   using namespace brooklyn::oled;
   showBrightness(false,5,16);
   showLineTest(static_cast<brooklyn::semantic::Route>(9),1,12,12,false);
   ScreenText text;LineTestView v;v.route=static_cast<brooklyn::semantic::Route>(9);v.total=12;v.color=1;
   formatLineTest(v,text);OLED_CHECK(strcmp(text.lines[0],"Testing line F")==0 && strcmp(text.lines[1],"Whole line GREEN")==0);
   v.individual=true;v.current=2;formatLineTest(v,text);OLED_CHECK(strcmp(text.lines[2],"Light 2 / 12")==0);
   for(auto &line:text.lines) OLED_CHECK(strlen(line)<=21);
   if(scenario==41) setSetupMode(true);
   if(scenario==42) setEnabled(false);
 }
 if(scenario>=43 && scenario<=45) {
   brooklyn::oled::ScreenText text;strcpy(text.lines[0],"Happy Bar Mitsvah!");strcpy(text.lines[1],"Avi Yao");
   brooklyn::oled::showEasterEgg(text);
   if(scenario==44) brooklyn::oled::setSetupMode(true);
   if(scenario==45) brooklyn::oled::setEnabled(false);
 }
 const uint8_t preferred=scenario==11?0x3c:0x38;
 brooklyn::oled::begin(preferred,scenario==12||scenario==13?32:207);OLED_CHECK(starts==0 && frames==0); // Startup must return without any I2C work.
 if(scenario==6){OLED_CHECK(worker==nullptr);puts("OLED task allocation failure isolated PASS");return 0;}
 OLED_CHECK(worker);brooklyn::oled::begin(preferred);
 if(setjmp(iterationEnd)==0) worker(nullptr);
 OLED_CHECK(!open);
 if(scenario==0) OLED_CHECK(borderDraws==2);
 if(scenario==0){OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::Ready); OLED_CHECK(frames==2 && titles==2 && delays==1 && deletes==0);}
 if(scenario==7){OLED_CHECK(lastAddress==0x3c && frames==2 && titles==2 && delays==1);
 OLED_CHECK(brooklyn::oled::status().activeAddress==0x3c && brooklyn::oled::status().requestedAddress==0x38); }
 if(scenario==9){OLED_CHECK(retryDelays==1 && frames==2 && brooklyn::oled::status().state==brooklyn::oled::State::Ready); }
 if(scenario==1){OLED_CHECK(starts==0 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::BusLow); }
 if(scenario==2){OLED_CHECK(ends==6 && retryDelays==1 && frames==0 && deletes==1);
 OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::NoAck && brooklyn::oled::status().error==2); }
 if(scenario==3 || scenario==4){OLED_CHECK(ends==2 && frames==0 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::InitFailed); }
 if(scenario==5){OLED_CHECK(ends==18 && frames==1 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::InitFailed); }
 if(scenario==10){OLED_CHECK(ends==34 && frames==2 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::TransferFailed && brooklyn::oled::status().error==5);}
 if(scenario==11){OLED_CHECK(frames==2 && brooklyn::oled::status().requestedAddress==0x3c && brooklyn::oled::status().activeAddress==0x3c);}
 if(scenario==12){OLED_CHECK(contrastCalls==2 && lastContrast==5 && frames==2 && brooklyn::oled::status().state==brooklyn::oled::State::Ready);}
 if(scenario==13){OLED_CHECK(frames==0 && deletes==1 && brooklyn::oled::status().state==brooklyn::oled::State::InitFailed);}
 if(scenario==14){OLED_CHECK(frames==1 && deletes==1 && brooklyn::oled::status().state==brooklyn::oled::State::TransferFailed);}
 if(scenario==17||scenario==18) OLED_CHECK(powerCommands==0 && frames==2 && titles==1);
 if(scenario==19) OLED_CHECK(powerCommands==1 && frames==4 && titles==1 && delays==5);
 if(scenario==20) OLED_CHECK(powerCommands==1 && frames==2 && titles==1 && delays==2);
 if(scenario==15) OLED_CHECK(powerCommands==1 && frames==1 && deletes==0);
 if(scenario==16) OLED_CHECK(powerCommands==2 && frames==2 && deletes==0);
 if(scenario==0||scenario==23) OLED_CHECK(strcmp(lastTrainText,"Trains:-- Lit:0")==0);
 if(scenario==21) OLED_CHECK(strcmp(lastTrainText,"Trains:132 Lit:0")==0);
 if(scenario==22) OLED_CHECK(strcmp(lastTrainText,"Trains:0 Lit:0")==0);
 if(scenario==24) OLED_CHECK(strcmp(lastTrainText,"Trains:80 Lit:12")==0);
 if(scenario==25) OLED_CHECK(setupTitles==2 && setupNames==2 && setupAddresses==2 && titles==0);
 if(scenario==26) OLED_CHECK(setupTitles==2 && setupNames==2 && setupAddresses==2 && titles==1 && frames==3);
 if(scenario>=27 && scenario<=29) OLED_CHECK(frames==2 && titles==0 && setupTitles==0);
 if(scenario==30) OLED_CHECK(frames==2 && setupTitles==2 && titles==0);
 if(scenario==31) OLED_CHECK(frames==1 && powerCommands==1 && titles==0);
 if(scenario==33) OLED_CHECK(frames==3 && titles==2 && strcmp(lastScreenTitle,"Map lights OFF   2/6")==0 && fakeNowMs==120040);
 if(scenario==34) OLED_CHECK(frames==3 && titles==0 && strcmp(lastScreenTitle,"Line 3 Paused 5/6")==0 && fakeNowMs==120040);
 if(scenario==35) OLED_CHECK(frames==3 && titles==3 && delays==51);
 if(scenario==36) OLED_CHECK(frames==2 && titles==0 && strcmp(lastScreenTitle,"Happy Bar Mitsvah!")==0);
 if(scenario==37) OLED_CHECK(frames==2 && titles==0 && borderDraws==0 && strcmp(lastScreenTitle,"Happy Bar Mitsvah!")==0);
 if(scenario==38) OLED_CHECK(contrastCalls==1 && lastContrast==1 && frames==2);
 if(scenario==39) OLED_CHECK(titles==1 && frames==8 && brooklyn::oled::selectedScreen()==brooklyn::oled::Screen::Main);
 if(scenario==40) OLED_CHECK(frames==3 && titles==1 && borderDraws==1 && brooklyn::oled::selectedScreen()==brooklyn::oled::Screen::Main);
 if(scenario==41) OLED_CHECK(frames==2 && setupTitles==2 && titles==0);
 if(scenario==42) OLED_CHECK(frames==1 && powerCommands==1 && titles==0);
 if(scenario==43) OLED_CHECK(frames==3 && titles==1 && brooklyn::oled::selectedScreen()==brooklyn::oled::Screen::Main);
 if(scenario==44) OLED_CHECK(frames==2 && setupTitles==2 && titles==0);
 if(scenario==45) OLED_CHECK(frames==1 && powerCommands==1 && titles==0);
 if(scenario==46) OLED_CHECK(frames==4 && titles==3 && brooklyn::oled::selectedScreen()==brooklyn::oled::Screen::Main);
 printf("OLED runtime scenario %d PASS\n",scenario);
}
