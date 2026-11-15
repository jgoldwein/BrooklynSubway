#include "oled_screens.h"
#include "rotary_policy.h"
#include "test_button_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using namespace brooklyn::oled;
int main() {
  Navigation nav;
  CHECK(nav.view().screen==Screen::Main && !nav.view().automatic);
  nav.input(Action::TurnPrevious,10);CHECK(nav.view().screen==Screen::Lines && !nav.view().lineBrowsing);
  nav.input(Action::TurnNext,20);CHECK(nav.view().screen==Screen::Main);
  nav.input(Action::TurnPrevious,30);nav.input(Action::Select,40);
  CHECK(nav.view().lineBrowsing);
  nav.input(Action::TurnPrevious,50);CHECK(nav.view().line==brooklyn::semantic::kRouteCount);
  nav.input(Action::TurnNext,60);CHECK(nav.view().line==1);
  nav.input(Action::TurnNext,70);CHECK(nav.view().line==2);
  nav.input(Action::Select,80);CHECK(!nav.view().lineBrowsing);
  nav.input(Action::TurnNext,90);CHECK(nav.view().screen==Screen::Main && nav.view().line==2);
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
  puts("OLED navigation: turn screens / press to browse lines, context exit, auto pause, remembered line, timeout/Stay, rollover, rotary bounce, long press, SSID scrolling and shared-light counts PASS");
}
