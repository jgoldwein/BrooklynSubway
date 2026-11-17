#include "personalization.h"
#include "oled_clock.h"
#include <stdio.h>
namespace brooklyn::oled {
namespace {
bool utcCalendar(const time_t &value, struct tm &out) {
#if defined(_WIN32)
  return gmtime_s(&out, &value) == 0;
#else
  return gmtime_r(&value, &out) != nullptr;
#endif
}
}
namespace {
bool newYorkCalendar(time_t utc,struct tm &calendar) {
  if(utc<1700000000) return false;
  if (!utcCalendar(utc, calendar)) { return false; }
  // US DST: second Sunday in March at 07:00 UTC to first Sunday in November at 06:00 UTC.
  const int firstSunday = 1 + (7 - (calendar.tm_wday - (calendar.tm_mday - 1) % 7 + 7) % 7) % 7;
  bool daylight = calendar.tm_mon > 2 && calendar.tm_mon < 10;
  if (calendar.tm_mon == 2) daylight = calendar.tm_mday > firstSunday + 7 ||
      (calendar.tm_mday == firstSunday + 7 && calendar.tm_hour >= 7);
  if (calendar.tm_mon == 10) daylight = calendar.tm_mday < firstSunday ||
      (calendar.tm_mday == firstSunday && calendar.tm_hour < 6);
  const time_t local = utc - (daylight ? 4 : 5) * 3600;
  if (!utcCalendar(local, calendar)) { return false; }
  return true;
}
}
bool newYorkMinutes(time_t utc,uint16_t &minute) {
  struct tm local{};if(!newYorkCalendar(utc,local)) return false;
  minute=local.tm_hour*60+local.tm_min;return true;
}
uint16_t newYorkAnniversaryYear(time_t utc) {
  struct tm local{};
  if(!(personal::extraSnapshot().enabledMask&2))return 0;
  const auto p=personal::snapshot();int y,m,d;
  if(!personal::dateParts(p.date,y,m,d) || !newYorkCalendar(utc,local) || local.tm_mon!=m-1 || local.tm_mday!=d || local.tm_year+1900<y) return 0;
  return static_cast<uint16_t>(local.tm_year+1900);
}
bool formatClock(time_t utc, char *out, size_t capacity) {
  if (!out || !capacity) return false;
  if (utc < 1700000000) { snprintf(out, capacity, "Syncing time..."); return false; }
  struct tm calendar{};
  if(!newYorkCalendar(utc,calendar)) {out[0]=0;return false;}
  const int hour = calendar.tm_hour % 12 ? calendar.tm_hour % 12 : 12;
  const int written = snprintf(out, capacity, "%d:%02d:%02d %s", hour, calendar.tm_min,
      calendar.tm_sec, calendar.tm_hour < 12 ? "AM" : "PM");
  return written > 0 && static_cast<size_t>(written) < capacity;
}
bool formatDate(time_t utc,char *out,size_t capacity) {
  if(!out || !capacity) return false;
  struct tm local{};
  if(!newYorkCalendar(utc,local)) {snprintf(out,capacity,"Waiting for time");return false;}
  constexpr const char *days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  constexpr const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  const int n=snprintf(out,capacity,"%s, %s %d, %d",days[local.tm_wday],months[local.tm_mon],local.tm_mday,local.tm_year+1900);
  return n>0 && size_t(n)<capacity;
}
}
