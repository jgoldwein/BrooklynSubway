// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#include "easter_eggs.h"
#include "personalization.h"
#include <stdio.h>
namespace brooklyn::eggs {
namespace {
const char *morse(char c) {
  static const char *codes[]={".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--..","-----",".----","..---","...--","....-",".....","-....","--...","---..","----."};
  if(c>='a' && c<='z')c-=32;
  return c>='A' && c<='Z'?codes[c-'A']:c>='0'&&c<='9'?codes[26+c-'0']:"";
}
uint32_t morseUnits(const char *name) {
  uint32_t units=0;bool previous=false,space=false;
  for(const char *p=name;*p;++p) {
    const char *code=morse(*p);if(!*code) {if(*p==' ')space=true;continue;}
    if(previous)units+=space?7:3;
    for(unsigned n=0;code[n];++n) {units+=code[n]=='.'?1:3;if(code[n+1])++units;}
    previous=true;space=false;
  }return units;
}
}

uint32_t duration(Kind kind) {
  switch(kind) {
    case Kind::Celebration:return 6000;
    case Kind::Express:return 7000;
    case Kind::Credits:return 12000;
    case Kind::Morse:{const auto p=personal::snapshot();const uint32_t ms=(morseUnits(p.recipient)+10)*100;return ms<7500?7500:ms;}
    case Kind::Anniversary:return 8000;
  }
  return 0;
}
bool sample(Kind kind,uint32_t elapsed,Sample &out) {
  out={};if(elapsed>=duration(kind)) return false;
  auto &lines=out.text.lines;
  const auto identity=personal::snapshot();
  snprintf(lines[4],sizeof(lines[4]),"POWER: exit");
  switch(kind) {
    case Kind::Celebration: {
      snprintf(lines[0],sizeof(lines[0]),"%s",identity.greeting);
      snprintf(lines[1],sizeof(lines[1]),"%.20s",identity.recipient);
      snprintf(lines[2],sizeof(lines[2]),"Let's celebrate!");
      out.visual=Visual::Line;
      out.route=static_cast<semantic::Route>(1+(elapsed/300)%semantic::kRouteCount);
      constexpr uint16_t notes[]={523,659,784,1047,784,659,523,784};
      if(elapsed<4000 && elapsed%300<200) out.frequency=notes[(elapsed/300)%8];
      out.color=(elapsed/300)%2?led::Rgb{0,3,1}:led::Rgb{3,0,3};break;
    }
    case Kind::Express:
      snprintf(lines[0],sizeof(lines[0]),"%.12s Express",identity.shortName);
      snprintf(lines[1],sizeof(lines[1]),"Next stop: adventure!");
      snprintf(lines[2],sizeof(lines[2]),"Just a light chaser");
      out.visual=Visual::Chaser;out.position=elapsed*semantic::kMapLedCount/7000;out.color={0,3,3};break;
    case Kind::Credits:
      snprintf(lines[0],sizeof(lines[0]),"For %.17s",identity.recipient);
      snprintf(lines[1],sizeof(lines[1]),"%s",identity.from);
      snprintf(lines[2],sizeof(lines[2]),"By %.18s",identity.creator);
      oled::scrollDedicationNames(elapsed*3,lines[3]);break;
    case Kind::Anniversary:
      snprintf(lines[0],sizeof(lines[0]),"%s",identity.greeting);
      snprintf(lines[1],sizeof(lines[1]),"%.21s",identity.recipient);
      snprintf(lines[2],sizeof(lines[2]),"%s",identity.date);
      snprintf(lines[3],sizeof(lines[3]),"A day to remember!");break;
    case Kind::Morse: {
      snprintf(lines[0],sizeof(lines[0]),"%.12s in Morse",identity.shortName);
      snprintf(lines[1],sizeof(lines[1]),"%.21s",identity.recipient);
      snprintf(lines[2],sizeof(lines[2]),"Dots and dashes");
      uint32_t cursor=0;const uint32_t unit=elapsed/100;bool previous=false,space=false;
      for(const char *p=identity.recipient;*p;++p) {
        const char *code=morse(*p);if(!*code) {if(*p==' ')space=true;continue;}
        if(previous)cursor+=space?7:3;
        for(unsigned n=0;code[n];++n) {
          const uint8_t width=code[n]=='.'?1:3;
          if(unit>=cursor && unit<cursor+width) {
            out.frequency=700;
            snprintf(lines[3],sizeof(lines[3]),"%c: %s  %s",*p,code,code[n]=='.'?"DOT":"DASH");return true;
          }
          cursor+=width;if(code[n+1])++cursor;
        }
        previous=true;space=false;
      }
      snprintf(lines[3],sizeof(lines[3]),"%s",unit>=cursor?"Message complete":"(pause)");break;
    }
  }
  return true;
}
bool Chord::update(bool test,bool route,uint32_t now) {
  if(!initialized_) {initialized_=true;ready_=!test&&!route;blocked_=test&&route;return false;}
  if(!ready_) {if(!test&&!route) {ready_=true;blocked_=false;}return false;}
  if(test&&route) {
    blocked_=true;releasing_=false;
    if(!held_) {held_=true;since_=now;}
    if(static_cast<uint32_t>(now-since_)>=2000) armed_=true;
  } else {
    held_=false;
    if(blocked_ && !test && !route) {
      if(!releasing_) {releasing_=true;releasedAt_=now;}
      if(static_cast<uint32_t>(now-releasedAt_)>=60) {
        const bool launch=armed_;blocked_=armed_=releasing_=false;return launch;
      }
    } else releasing_=false;
  }
  return false;
}
}
