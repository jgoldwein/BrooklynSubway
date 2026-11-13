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
puts("OLED clock: synchronization, winter/summer, DST boundaries, noon/midnight and buffer limits PASS");
}
