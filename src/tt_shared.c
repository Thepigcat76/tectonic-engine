#include "../include/tt_shared.h"
#include <time.h>

void tt_wait(i32 millis) {
  struct timespec ts;
  ts.tv_sec = millis / 1000;
  ts.tv_nsec = (millis % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}
