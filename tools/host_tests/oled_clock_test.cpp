#include "personalization.h"
#include "oled_clock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); exit(1); } } while(0)
int main() {
char text[24]{};
CHECK(!brooklyn::oled::formatClock(0, text, sizeof(text)));
CHECK(strcmp(text,"Syncing time...")==0);
CHECK(!brooklyn::oled::formatClock(1794573786, nullptr, 0));
CHECK(brooklyn::oled::formatClock(1768498496, text, sizeof(text))); CHECK(strcmp(text,"12:34:56 PM")==0);
CHECK(brooklyn::oled::formatClock(1784133296, text, sizeof(text))); CHECK(strcmp(text,"12:34:56 PM")==0);
CHECK(brooklyn::oled::formatClock(1772953199, text, sizeof(text))); CHECK(strcmp(text,"1:59:59 AM")==0);
CHECK(brooklyn::oled::formatClock(1772953200, text, sizeof(text))); CHECK(strcmp(text,"3:00:00 AM")==0);
CHECK(brooklyn::oled::formatClock(1793512799, text, sizeof(text))); CHECK(strcmp(text,"1:59:59 AM")==0);
CHECK(brooklyn::oled::formatClock(1793512800, text, sizeof(text))); CHECK(strcmp(text,"1:00:00 AM")==0);
CHECK(brooklyn::oled::formatClock(1794632400, text, sizeof(text))); CHECK(strcmp(text,"12:00:00 AM")==0);
CHECK(brooklyn::oled::formatClock(1805007600, text, sizeof(text))); CHECK(strcmp(text,"3:00:00 AM")==0);
CHECK(!brooklyn::oled::formatClock(1794573786, text, 2));
uint16_t minute=0;
CHECK(!brooklyn::oled::newYorkMinutes(0,minute));
CHECK(brooklyn::oled::newYorkMinutes(1768498496,minute) && minute==754);
CHECK(brooklyn::oled::newYorkMinutes(1784133296,minute) && minute==754);
CHECK(brooklyn::oled::newYorkMinutes(1772953199,minute) && minute==119);
CHECK(brooklyn::oled::newYorkMinutes(1772953200,minute) && minute==180);
CHECK(brooklyn::oled::newYorkMinutes(1793512799,minute) && minute==119);
CHECK(brooklyn::oled::newYorkMinutes(1793512800,minute) && minute==60);
CHECK(brooklyn::oled::newYorkAnniversaryYear(0)==0);
CHECK(brooklyn::oled::newYorkAnniversaryYear(1790395199)==0);
CHECK(brooklyn::oled::newYorkAnniversaryYear(1790395200)==2026);
{brooklyn::personal::ExtraSettings p;p.enabledMask=13;brooklyn::personal::applyExtras(p);
CHECK(brooklyn::oled::newYorkAnniversaryYear(1790395200)==0);brooklyn::personal::applyExtras(brooklyn::personal::ExtraSettings{});}
CHECK(brooklyn::oled::newYorkAnniversaryYear(1790481599)==2026);
CHECK(brooklyn::oled::newYorkAnniversaryYear(1790481600)==0);
CHECK(brooklyn::oled::formatDate(1768498496,text,sizeof(text)) && strcmp(text,"Thu, Jan 15, 2026")==0);
CHECK(brooklyn::oled::formatDate(1794632400,text,sizeof(text)) && strcmp(text,"Sat, Nov 14, 2026")==0);
CHECK(!brooklyn::oled::formatDate(0,text,sizeof(text)) && strcmp(text,"Waiting for time")==0);
CHECK(!brooklyn::oled::formatDate(1768498496,text,2));CHECK(!brooklyn::oled::formatDate(1768498496,nullptr,0));
brooklyn::personal::Settings custom;strcpy(custom.date,"2026-11-14");brooklyn::personal::apply(custom);
CHECK(brooklyn::oled::newYorkAnniversaryYear(1794632400)==2026);
custom.date[0]=0;brooklyn::personal::apply(custom);CHECK(brooklyn::oled::newYorkAnniversaryYear(1794632400)==0);
brooklyn::personal::apply(brooklyn::personal::Settings{});
puts("OLED clock: synchronization, winter/summer, DST boundaries, noon/midnight and buffer limits PASS");
}
