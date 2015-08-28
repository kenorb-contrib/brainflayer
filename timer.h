#ifndef __TIMER_H__
#define __TIMER_H__

#include <time.h>
#include <stdint.h>

#define UNIT_MAGNITUDE  (1000)

#define TO_SECOND       (1)
#define TO_MILLISECOND  (UNIT_MAGNITUDE * TO_SECOND)
#define TO_MICROSECOND  (UNIT_MAGNITUDE * TO_MILLISECOND)
#define TO_NANOSECOND   (UNIT_MAGNITUDE * TO_MICROSECOND)


// WARNING: THESE ARE CLOCK FUNCTIONS, NOT TIME FUNCTIONS
// Do not expect a time-of-day representation from any of the below functions

struct timespec get_clock();

uint64_t get_clockdiff_ns(struct timespec prev_clock);  // Returns clock difference in nanoseconds
uint64_t get_clockdiff_ms(struct timespec prev_clock);  // Returns clock difference in milliseconds
double   get_clockdiff_s(struct timespec prev_clock);   // Returns clock difference in seconds

#endif