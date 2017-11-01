#pragma once

#include <sys/time.h>

inline double timestamp() {
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + 1e-6 * now.tv_usec;
  }


