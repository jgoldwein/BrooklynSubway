#include "personalization.h"
#include "oled_screens.h"
#include "screen_preferences.h"
#include "rotary_policy.h"
#include "test_button_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using namespace brooklyn::oled;
int main() {
  CHECK(nextScreen(Screen::Lines)==Screen::Clock && nextScreen(Screen::Clock)==Screen::Weather && nextScreen(Screen::Weather)==Screen::Dedication);
  Navigation nav;
  CHECK(nav.view().screen==Screen::Main && !nav.view().automatic);
  nav.input(Action::TurnPrevious,10);CHECK(nav.view().screen==Screen::Dedication);
  nav.input(Action::TurnPrevious,12);CHECK(nav.view().screen==Screen::Weather);
  nav.input(Action::TurnPrevious,13);CHECK(nav.view().screen==Screen::Clock);
  nav.input(Action::TurnPrevious,15);CHECK(nav.view().screen==Screen::Lines && !nav.view().lineBrowsing);
  nav.input(Action::TurnNext,20);CHECK(nav.view().screen==Screen::Clock);
  nav.input(Action::TurnNext,22);CHECK(nav.view().screen==Screen::Weather);
  nav.input(Action::TurnNext,24);CHECK(nav.view().screen==Screen::Dedication);
  nav.input(Action::TurnNext,25);CHECK(nav.view().screen==Screen::Main);
  nav.input(Action::TurnPrevious,30);nav.input(Action::TurnPrevious,32);nav.input(Action::TurnPrevious,33);nav.input(Action::TurnPrevious,35);nav.input(Action::Select,40);
  CHECK(nav.view().lineBrowsing);
  nav.input(Action::TurnPrevious,50);CHECK(nav.view().line==brooklyn::semantic::kRouteCount);
  nav.input(Action::TurnNext,60);CHECK(nav.view().line==1);
  nav.input(Action::TurnNext,70);CHECK(nav.view().line==2);
  nav.input(Action::Select,80);CHECK(!nav.view().lineBrowsing);
  nav.input(Action::TurnNext,90);CHECK(nav.view().screen==Screen::Clock && nav.view().line==2);
  nav.input(Action::TurnNext,92);nav.input(Action::TurnNext,94);
  nav.input(Action::TurnNext,95);CHECK(nav.view().screen==Screen::Main);
  nav.input(Action::OpenLines,100);CHECK(nav.view().screen==Screen::Lines && nav.view().line==2);
  nav.input(Action::ToggleAuto,200);CHECK(nav.view().automatic);
  CHECK(!nav.update(5199));CHECK(nav.update(5200));CHECK(nav.view().line==3);
  nav.input(Action::TurnNext,5300);CHECK(nav.view().line==4 && !nav.view().automatic);
  CHECK(!nav.update(10300));
  nav.input(Action::ToggleAuto,11000);nav.input(Action::Select,12000);
  CHECK(!nav.view().automatic && !nav.view().lineBrowsing);
  nav.setReturnSeconds(30,12000);CHECK(!nav.update(41999));CHECK(nav.update(42000));
  CHECK(nav.view().screen==Screen::Main && nav.view().line==4);
  nav.input(Action::OpenLines,43000);nav.setReturnSeconds(0,43000);
  CHECK(!nav.update(1000000));CHECK(nav.view().screen==Screen::Lines);
  nav.setReturnSeconds(30,1000000);CHECK(!nav.update(1020000,false)); // POWER/setup/test suspends timers.
  CHECK(!nav.update(1049999));CHECK(nav.update(1050000));
  nav.input(Action::OpenLines,UINT32_MAX-999);nav.input(Action::ToggleAuto,UINT32_MAX-999);
  CHECK(!nav.update(3999));CHECK(nav.update(4000)); // Auto advance across millis rollover.
  nav.input(Action::Select,5000);nav.setReturnSeconds(30,5000);
  CHECK(nav.update(35000));CHECK(nav.view().screen==Screen::Main);
  nav.input(Action::OpenLines,UINT32_MAX-999);nav.input(Action::Select,UINT32_MAX-999);
  CHECK(!nav.update(28999));CHECK(nav.update(29000));
  CHECK(!nav.input(Action::Select,30000));

  { Navigation clock;clock.input(Action::TurnPrevious,1);clock.input(Action::TurnPrevious,2);clock.input(Action::TurnPrevious,3);
    CHECK(clock.view().screen==Screen::Clock);clock.setReturnSeconds(30,3);
    CHECK(!clock.update(300000) && clock.view().screen==Screen::Clock);
    clock.input(Action::TurnNext,300001);CHECK(clock.view().screen==Screen::Weather);
    CHECK(clock.update(330001) && clock.view().screen==Screen::Main); }
  using brooklyn::controls::RotaryEncoder;
  RotaryEncoder knob;knob.reset(true,true);
  CHECK(knob.update(false,true)==0);CHECK(knob.update(true,true)==0); // Bounce cancels.
  CHECK(knob.update(false,true)==0);CHECK(knob.update(false,false)==1);
  CHECK(knob.update(true,false)==0);CHECK(knob.update(true,true)==1);
  CHECK(knob.update(true,false)==0);CHECK(knob.update(false,false)==-1);
  CHECK(knob.update(false,true)==0);CHECK(knob.update(true,true)==-1);
  CHECK(knob.update(false,false)==0); // Invalid two-bit jump cancels partial motion.
  CHECK(knob.update(true,true)==0);
  knob.reset(false,true); // Also support encoders resting at 01 / 10.
  CHECK(knob.update(false,false)==0);CHECK(knob.update(true,false)==1);
  CHECK(knob.update(true,true)==0);CHECK(knob.update(false,true)==1);
  brooklyn::controls::TestButton push;
  push.reset(true,0);CHECK(!push.update(false,10));CHECK(!push.update(false,50));
  CHECK(!push.update(true,100));CHECK(!push.update(true,140));
  CHECK(!push.update(false,200));CHECK(push.update(false,240));CHECK(push.heldMs()==100);
  CHECK(!push.update(true,300));CHECK(!push.update(true,340));
  CHECK(!push.update(false,10400));CHECK(push.update(false,10440));CHECK(push.heldMs()>=10000);

  char name[22];const char *ssid="ABCDEFGHIJKLMNOPQRSTUVWXYZ123456";
  scrollName(ssid,0,name);CHECK(strcmp(name,"ABCDEFGHIJKLMNOPQRSTU")==0);
  scrollName(ssid,1999,name);CHECK(strcmp(name,"ABCDEFGHIJKLMNOPQRSTU")==0);
  scrollName(ssid,3000,name);CHECK(strcmp(name,"BCDEFGHIJKLMNOPQRSTUV")==0);
  scrollName(ssid,13000,name);CHECK(strcmp(name,"LMNOPQRSTUVWXYZ123456")==0);
  scrollName(ssid,14500,name);CHECK(strcmp(name,"LMNOPQRSTUVWXYZ123456")==0);
  scrollName(ssid,26000,name);CHECK(strcmp(name,"ABCDEFGHIJKLMNOPQRSTU")==0);
  scrollName("bad\nSSID",50000,name);CHECK(strcmp(name,"bad?SSID")==0);
  ScreenData data;brooklyn::semantic::TrainFrame frame{};
  uint16_t lights[brooklyn::semantic::kRouteCount+1]{};
  uint16_t led=0;while(!brooklyn::semantic::semanticForLed(led)) ++led;
  const auto route=static_cast<uint8_t>(brooklyn::semantic::routeForLed(led));
  frame.occupancy[led]=3;lights[route]=1;
  setLineCounts(data,frame,lights);CHECK(data.lines[route].trains==3 && data.lines[route].lights==1);
  frame.occupancy[led]=255;setLineCounts(data,frame,lights);CHECK(data.lines[route].atLeast);
  ScreenText text;NavigationView view;view.screen=Screen::Lines;view.line=route;view.lineBrowsing=true;
  data.routeMask=0;formatScreen(Screen::Lines,data,0,text,view);
  CHECK(strstr(text.lines[2],"hidden")!=nullptr);
  CHECK(strstr(text.lines[4],"Turn:line")!=nullptr);
  nav.startDedication(100000);CHECK(nav.view().screen==Screen::Dedication);
  CHECK(!nav.update(104999));CHECK(nav.update(105000));CHECK(nav.view().screen==Screen::Main);
  nav.startDedication(200000);nav.input(Action::TurnNext,200010);CHECK(nav.view().screen==Screen::Main);CHECK(!nav.update(205000));
  nav.startDedication(300000);CHECK(nav.update(300010,false));CHECK(nav.view().screen==Screen::Main);
  nav.startDedication(400000,true);CHECK(!nav.update(404999,true,true));
  CHECK(!nav.update(520000,true,false));CHECK(nav.view().screen==Screen::Dedication);
  CHECK(nav.update(520001,true,true));CHECK(nav.view().screen==Screen::Main);
  CHECK(!nav.update(520002,true,false));CHECK(nav.view().screen==Screen::Main); // Later Wi-Fi drop does not restart greeting.
  nav.startDedication(600000,true);nav.input(Action::TurnNext,600001);CHECK(nav.view().screen==Screen::Main);
  nav.startDedication(700000,true);CHECK(nav.update(700001,false,false));CHECK(nav.view().screen==Screen::Main);
  using namespace brooklyn::config;
  ScreenPreferences preferences;preferences.quietEnabled=true;
  preferences.startupUntilWifi=false;
  const auto brief=packScreenPreferences(preferences);CHECK(!unpackScreenPreferences(brief).startupUntilWifi);
  const auto legacy=brief & ~(kScreenPreferencesSchemaBit|kStartupWaitForWifiBit);
  CHECK(validScreenPreferences(legacy) && unpackScreenPreferences(legacy).startupUntilWifi);
  preferences.startupUntilWifi=true;
  auto schedule=packScreenPreferences(preferences);CHECK(validScreenPreferences(schedule));
  CHECK(!quietActive(schedule,true,1319));CHECK(quietActive(schedule,true,1320));CHECK(quietActive(schedule,true,419));CHECK(!quietActive(schedule,true,420));
  CHECK(!quietActive(schedule,false,0));CHECK(!quietActive(schedule,false,1320));preferences.quietEnabled=false;CHECK(!quietActive(packScreenPreferences(preferences),false,0));
  preferences.quietEnabled=true;preferences.startMinute=540;preferences.endMinute=1020;schedule=packScreenPreferences(preferences);
  CHECK(!quietActive(schedule,true,539));CHECK(quietActive(schedule,true,540));CHECK(quietActive(schedule,true,1019));CHECK(!quietActive(schedule,true,1020));
  preferences.startMinute=preferences.endMinute;CHECK(!validScreenPreferences(packScreenPreferences(preferences)));
  preferences.startMinute=1440;CHECK(!validScreenPreferences(packScreenPreferences(preferences)));
  preferences.startMinute=1320;preferences.endMinute=420;
  preferences.quietMapBrightness=255;CHECK(validScreenPreferences(packScreenPreferences(preferences)));
  CHECK(unpackScreenPreferences(packScreenPreferences(preferences)).quietMapBrightness==255);
  preferences.quietMapBrightness=16;CHECK(validScreenPreferences(packScreenPreferences(preferences)));
  preferences.quietMapBrightness=17;CHECK(!validScreenPreferences(packScreenPreferences(preferences)));
  uint16_t minute=0;CHECK(parseClockMinute("22:00",minute) && minute==1320);
  CHECK(!parseClockMinute("24:00",minute));CHECK(!parseClockMinute("12:60",minute));CHECK(!parseClockMinute("9:00",minute));CHECK(!parseClockMinute("09:00x",minute));
  const char *names[]={"Joel","Marlene","Marc","Kim","Juliana","Levi","Robbie","Eric"};
  bool seen[8]{};view.screen=Screen::Dedication;view.changedAtMs=0;
  for(uint32_t at=0;at<60000;at+=1000) {
    formatScreen(Screen::Dedication,data,at,text,view);
    CHECK(strcmp(text.lines[0],"Happy Bar Mitzvah!")==0);
    CHECK(strcmp(text.lines[1],"Avi")==0);
    CHECK(strcmp(text.lines[2],"From the Goldweins")==0);
    CHECK(strcmp(text.lines[3],"September 26, 2026")==0);
    for(auto &line:text.lines) CHECK(strlen(line)<=21);
    for(unsigned i=0;i<8;++i) if(strstr(text.lines[4],names[i])) seen[i]=true;
  }
  for(bool visible:seen) CHECK(visible);
  data.sampledAtMs=1000;data.uptimeSeconds=49ull*86400+2*3600+3*60+4;
  formatScreen(Screen::Device,data,2000,text);CHECK(strcmp(text.lines[3],"Uptime 49d 02:03:05")==0);
  data.sampledAtMs=UINT32_MAX-499;
  formatScreen(Screen::Device,data,500,text);CHECK(strcmp(text.lines[3],"Uptime 49d 02:03:05")==0);
  data.uptimeSeconds=UINT64_MAX;formatScreen(Screen::Device,data,500,text);CHECK(strcmp(text.lines[3],"Uptime >9999 days")==0);
  strcpy(data.mac,"28:84:85:34:1F:74");formatScreen(Screen::Device,data,2000,text);CHECK(strcmp(text.lines[4],"MAC 28:84:85:34:1F:74")==0);
  data.hasUpdate=true;data.lightsEnabled=true;data.lines[9].trains=12;data.lines[9].lights=8;data.lines[9].atLeast=false;
  formatRouteFocus(brooklyn::semantic::Route::RouteF,data,text);CHECK(strcmp(text.lines[0],"Line F only")==0 && strcmp(text.lines[1],"Trains mapped: 12")==0);
  CHECK(strcmp(text.lines[4],"MODE: all saved lines")==0);for(const auto &line:text.lines) CHECK(strlen(line)<=21);
  brooklyn::personal::Settings custom;
  strcpy(custom.greeting,"Welcome!");strcpy(custom.shortName,"Sam");strcpy(custom.from,"From family");
  strcpy(custom.names,"Alex, Jamie");strcpy(custom.date,"2028-02-29");
  brooklyn::personal::apply(custom);formatScreen(Screen::Dedication,data,0,text,view);
  CHECK(strcmp(text.lines[0],"Welcome!")==0 && strcmp(text.lines[1],"Sam")==0);
  CHECK(strcmp(text.lines[2],"From family")==0 && strcmp(text.lines[3],"February 29, 2028")==0);
  CHECK(strcmp(text.lines[4],"Alex, Jamie")==0);
  brooklyn::personal::apply(brooklyn::personal::Settings{});
  puts("Dedication text, all eight scrolling names, 64-bit uptime and millis rollover PASS");
  puts("OLED navigation: turn screens / press to browse lines, context exit, auto pause, remembered line, timeout/Stay, rollover, rotary bounce, long press, SSID scrolling and shared-light counts PASS");
}
