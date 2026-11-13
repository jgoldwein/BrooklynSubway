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
bool formatClock(time_t utc, char *out, size_t capacity) {
  if (!out || !capacity) return false;
  if (utc < 1700000000) { snprintf(out, capacity, "Syncing time..."); return false; }
  struct tm calendar{};
  if (!utcCalendar(utc, calendar)) { out[0] = '\0'; return false; }
  // US DST: second Sunday in March at 07:00 UTC to first Sunday in November at 06:00 UTC.
  const int firstSunday = 1 + (7 - (calendar.tm_wday - (calendar.tm_mday - 1) % 7 + 7) % 7) % 7;
  bool daylight = calendar.tm_mon > 2 && calendar.tm_mon < 10;
  if (calendar.tm_mon == 2) daylight = calendar.tm_mday > firstSunday + 7 ||
      (calendar.tm_mday == firstSunday + 7 && calendar.tm_hour >= 7);
  if (calendar.tm_mon == 10) daylight = calendar.tm_mday < firstSunday ||
      (calendar.tm_mday == firstSunday && calendar.tm_hour < 6);
  const time_t local = utc - (daylight ? 4 : 5) * 3600;
  if (!utcCalendar(local, calendar)) { out[0] = '\0'; return false; }
  const int hour = calendar.tm_hour % 12 ? calendar.tm_hour % 12 : 12;
  const int written = snprintf(out, capacity, "%d:%02d:%02d %s", hour, calendar.tm_min,
      calendar.tm_sec, calendar.tm_hour < 12 ? "AM" : "PM");
  return written > 0 && static_cast<size_t>(written) < capacity;
}
}
