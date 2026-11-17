#include "led_power.h"
#include "personalization.h"
#include "easter_eggs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using namespace brooklyn;
int main() {
  for(uint8_t k=0;k<eggs::kCount;++k) {
    const auto kind=static_cast<eggs::Kind>(k);eggs::Sample out;
    CHECK(eggs::duration(kind)<=12000);
    for(uint32_t ms=0;ms<eggs::duration(kind);ms+=20) {
      CHECK(eggs::sample(kind,ms,out));
      for(const auto &line:out.text.lines) CHECK(strlen(line)<=21);
      CHECK(out.frequency==0 || (out.frequency>=100 && out.frequency<=4000));
      CHECK(out.color.red<=4 && out.color.green<=4 && out.color.blue<=4);
      if(out.visual==eggs::Visual::Line) CHECK(unsigned(out.route)>=1 && unsigned(out.route)<=18);
    }
    CHECK(!eggs::sample(kind,eggs::duration(kind),out));
    CHECK(!eggs::sample(kind,UINT32_MAX,out));
  }
  const char *codes[]={".-","...-","..","-.--",".-","---"};
  uint32_t unit=0;
  for(unsigned l=0;l<6;++l) {
    for(unsigned n=0;codes[l][n];++n) {
      const unsigned width=codes[l][n]=='.'?1:3;
      for(unsigned t=0;t<width;++t) {eggs::Sample s;CHECK(eggs::sample(eggs::Kind::Morse,(unit+t)*100,s) && s.frequency==700);}
      unit+=width;
      const unsigned gap=codes[l][n+1]?1:l==5?0:l==2?7:3;
      for(unsigned t=0;t<gap;++t) {eggs::Sample s;CHECK(eggs::sample(eggs::Kind::Morse,(unit+t)*100,s) && s.frequency==0);}
      unit+=gap;
    }
  }
  CHECK(unit==65);
  const char *names[]={"Joel","Marlene","Marc","Kim","Juliana","Levi","Robbie","Eric"};
  unsigned visible=0;
  for(uint32_t ms=0;ms<12000;ms+=20) {
    eggs::Sample s;CHECK(eggs::sample(eggs::Kind::Credits,ms,s));
    for(unsigned i=0;i<8;++i) if(strstr(s.text.lines[3],names[i])) visible|=1u<<i;
  }
  CHECK(visible==255);
  eggs::Sample invalid;CHECK(!eggs::sample(static_cast<eggs::Kind>(255),0,invalid));
  eggs::Chord chord;
  CHECK(!chord.update(false,false,0));CHECK(!chord.update(true,true,100));
  CHECK(chord.suppressed());CHECK(!chord.update(true,true,2099));
  CHECK(!chord.update(true,true,2100));CHECK(!chord.update(false,true,2110));
  CHECK(!chord.update(false,false,2200));CHECK(chord.suppressed());
  CHECK(chord.update(false,false,2260));CHECK(!chord.suppressed());
  CHECK(!chord.update(false,false,2300));
  eggs::Chord boot;CHECK(!boot.update(true,true,0));CHECK(!boot.update(true,true,9000));
  CHECK(!boot.update(false,false,10000));CHECK(!boot.suppressed());
  eggs::Chord shortHold;shortHold.update(false,false,0);shortHold.update(true,true,10);
  shortHold.update(false,false,1000);CHECK(!shortHold.update(false,false,1060));
  eggs::Chord wrap;wrap.update(false,false,UINT32_MAX-3000);wrap.update(true,true,UINT32_MAX-1000);
  wrap.update(true,true,1000);wrap.update(false,false,1010);CHECK(wrap.update(false,false,1070));
  chord.update(true,true,3000);chord.update(true,true,6000);
  chord=eggs::Chord{}; // Diagnostic exit forgets a held/pending chord.
  CHECK(!chord.update(true,true,7000));CHECK(!chord.update(false,false,7100));
  personal::Settings custom;strcpy(custom.recipient,"Sam 123");strcpy(custom.shortName,"Sam");strcpy(custom.greeting,"Welcome!");personal::apply(custom);
  eggs::Sample customized;CHECK(eggs::sample(eggs::Kind::Celebration,0,customized));
  CHECK(strcmp(customized.text.lines[0],"Welcome!")==0 && strcmp(customized.text.lines[1],"Sam 123")==0);
  bool sounded=false;for(uint32_t at=0;at<eggs::duration(eggs::Kind::Morse);at+=100) {
    CHECK(eggs::sample(eggs::Kind::Morse,at,customized));sounded|=customized.frequency!=0;
    for(const auto &line:customized.text.lines)CHECK(strlen(line)<=21);
  }CHECK(sounded);CHECK(!eggs::sample(eggs::Kind::Morse,eggs::duration(eggs::Kind::Morse),customized));
  personal::apply(personal::Settings{});
  CHECK(eggs::kCount==6 && eggs::duration(eggs::Kind::Rangers)==9600);
  const uint32_t hornTimes[]={0,20,40,60,1100,1350,1370,1390,2450,2700,4100,4799};
  const uint16_t hornNotes[]={185,233,277,185,0,185,233,277,0,185,0,0};
  for(unsigned i=0;i<sizeof(hornTimes)/sizeof(hornTimes[0]);++i) {
    CHECK(eggs::sample(eggs::Kind::Rangers,hornTimes[i],customized));
    CHECK(customized.frequency==hornNotes[i] && customized.visual==eggs::Visual::All);
    CHECK(strcmp(customized.text.lines[0],"New York Rangers")==0);
    CHECK(strcmp(customized.text.lines[4],"POWER: exit")==0);
  }
  led::DisplayState hornFrame[306]{};
  const uint32_t colorTimes[]={0,240,480,720,960,1200,4100,4799};
  const led::Rgb colors[]={{3,0,0},{0,0,0},{2,2,2},{0,0,0},{0,0,3},{0,0,0},{0,0,0},{0,0,0}};
  for(unsigned i=0;i<sizeof(colorTimes)/sizeof(colorTimes[0]);++i) {
    CHECK(eggs::sample(eggs::Kind::Rangers,colorTimes[i],customized));
    CHECK(customized.color.red==colors[i].red && customized.color.green==colors[i].green && customized.color.blue==colors[i].blue);
    for(auto &pixel:hornFrame)pixel.color=customized.color;
    CHECK(led::power::estimateFrameCurrentMa(hornFrame,306,255)<=led::power::FIRMWARE_LED_CURRENT_BUDGET_MA);
  }
  CHECK(!eggs::sample(eggs::Kind::Rangers,9600,customized));CHECK(customized.frequency==0);
  CHECK(eggs::sample(eggs::Kind::Rangers,4800,customized) && customized.frequency==185);
  CHECK(eggs::sample(eggs::Kind::Rangers,0,customized) && strcmp(customized.text.lines[1],"GO RANGERS!")==0);
  CHECK(eggs::sample(eggs::Kind::Rangers,500,customized) && customized.text.lines[1][0]==0);
  CHECK(eggs::sample(eggs::Kind::Rangers,1000,customized) && strcmp(customized.text.lines[1],"GO RANGERS!")==0);
  personal::ExtraSettings extras;extras.enabledMask=0;personal::applyExtras(extras);
  CHECK(!eggs::enabled(eggs::Kind::Celebration) && !eggs::enabled(eggs::Kind::Anniversary));
  CHECK(!eggs::enabled(eggs::Kind::Credits) && !eggs::enabled(eggs::Kind::Rangers));
  CHECK(eggs::enabled(eggs::Kind::Express) && eggs::enabled(eggs::Kind::Morse));
  uint8_t cursor=0;eggs::Kind selected;
  CHECK(eggs::nextEnabled(cursor,selected) && selected==eggs::Kind::Express);
  CHECK(eggs::nextEnabled(cursor,selected) && selected==eggs::Kind::Morse);
  CHECK(eggs::nextEnabled(cursor,selected) && selected==eggs::Kind::Express);
  CHECK(!eggs::sample(eggs::Kind::Rangers,0,customized) && customized.frequency==0);
  extras.enabledMask=15;strcpy(extras.celebrationNote,"Congratulations!");strcpy(extras.anniversaryNote,"Remember this day");
  strcpy(extras.creditsTitle,"Our family");strcpy(extras.teamTitle,"NY Rangers");strcpy(extras.teamNote,"Sam's favorite team");strcpy(extras.teamCheer,"Go team!");personal::applyExtras(extras);
  CHECK(eggs::sample(eggs::Kind::Celebration,0,customized) && strcmp(customized.text.lines[2],"Congratulations!")==0);
  CHECK(eggs::sample(eggs::Kind::Anniversary,0,customized) && strcmp(customized.text.lines[3],"Remember this day")==0);
  CHECK(eggs::sample(eggs::Kind::Credits,0,customized) && strcmp(customized.text.lines[0],"Our family")==0);
  CHECK(eggs::sample(eggs::Kind::Rangers,0,customized) && strcmp(customized.text.lines[0],"NY Rangers")==0);
  CHECK(strcmp(customized.text.lines[3],"Sam's favorite team")==0 && strcmp(customized.text.lines[1],"Go team!")==0);
  personal::applyExtras(personal::ExtraSettings{});
  CHECK(eggs::sample(eggs::Kind::Rangers,0,customized) && strcmp(customized.text.lines[3],"Avi's favorite team")==0);
  puts("Easter eggs: bounded text/colors/duration, exact AVI YAO Morse, chord hold/release/boot/rollover PASS");
}
