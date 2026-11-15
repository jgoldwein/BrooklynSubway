#include "weather.h"
#include "oled_screens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using namespace brooklyn;
bool parse(const char *s,weather::Snapshot &out) {return weather::parseCsv(reinterpret_cast<const uint8_t *>(s),strlen(s),out);}
int main() {
  const char *valid="latitude,longitude\n40.67,-73.93\n\ntime,temperature_2m (\xc2\xb0" "F),weather_code (wmo code)\n1794774600,48.6,2\n\n";
  weather::Snapshot w;CHECK(parse(valid,w));CHECK(w.available && w.sourceUnix==1794774600 && w.fahrenheitTenths==486 && w.code==2);
  const char *negative="time,temperature_2m (F),weather_code (wmo code)\r\n1794774600,-0.5,71\r\n";
  CHECK(parse(negative,w) && w.fahrenheitTenths==-5 && strcmp(weather::description(w.code),"Snow")==0);
  const char *bad[]={"","{}","time,temperature_2m (C),weather_code (wmo code)\n1794774600,10,0\n",
    "time,temperature_2m (F),weather_code (wmo code)\n1794774600,NaN,0\n",
    "time,temperature_2m (F),weather_code (wmo code)\n1794774600,999999999999,0\n",
    "time,temperature_2m (F),weather_code (wmo code)\n1794774600,50,100\n",
    "time,temperature_2m (F),weather_code (wmo code)\n1794774600,50,0",
    "time,temperature_2m (F),weather_code (wmo code)\n1,50,0\n"};
  for(const char *s:bad) {CHECK(!parse(s,w));CHECK(w.fahrenheitTenths==-5);}
  CHECK(!weather::parseCsv(nullptr,20,w));CHECK(!weather::parseCsv(reinterpret_cast<const uint8_t *>(valid),1025,w));
  for(size_t n=0;n<strlen(valid)-2;++n) CHECK(!weather::parseCsv(reinterpret_cast<const uint8_t *>(valid),n,w));
  CHECK(weather::due(false,0,10000,10000));CHECK(!weather::due(false,0,11000,10000));
  CHECK(!weather::due(true,10000,909999,909999));CHECK(weather::due(true,10000,910000,910000));
  CHECK(weather::due(true,UINT32_MAX-899999,0,0));
  oled::ScreenData data;data.unixTime=1794774900;data.sampledAtMs=1000;data.weather={1794774600,486,2,true,true,false};
  oled::ScreenText text;oled::formatScreen(oled::Screen::Weather,data,1000,text);
  CHECK(strcmp(text.lines[1],"48.6 F")==0 && strcmp(text.lines[2],"Partly cloudy")==0 && strcmp(text.lines[3],"Data: 5m old")==0);
  data.weather.lastFailed=true;oled::formatScreen(oled::Screen::Weather,data,1000,text);CHECK(strstr(text.lines[3],"Delayed:"));
  data.unixTime+=3601;oled::formatScreen(oled::Screen::Weather,data,1000,text);CHECK(strstr(text.lines[3],"Old data:"));
  data.unixTime+=10800;oled::formatScreen(oled::Screen::Weather,data,1000,text);CHECK(strcmp(text.lines[1],"Weather unavailable")==0);
  data.weather={};oled::formatScreen(oled::Screen::Weather,data,1000,text);CHECK(strcmp(text.lines[1],"Waiting for weather")==0);
  for(unsigned code=0;code<=255;++code) CHECK(strlen(weather::description(code))<=21);
  puts("Weather: bounded CSV/units/temperature/time validation, malformed/truncated rejection, scheduling/rollover and stale/failed display PASS");
}
