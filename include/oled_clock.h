#pragma once
#include <stddef.h>
#include <time.h>
namespace brooklyn::oled {
// New York civil time; independent of the UTC SNTP configuration.
bool formatClock(time_t utc, char *out, size_t capacity);
void begin();
}
