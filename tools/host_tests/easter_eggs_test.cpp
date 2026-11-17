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
  puts("Easter eggs: bounded text/colors/duration, exact AVI YAO Morse, chord hold/release/boot/rollover PASS");
}
