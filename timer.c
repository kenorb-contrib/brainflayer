#include "timer.h"
#include <stdio.h>

struct timespec get_clock() {
    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);

    return now_clock;
}


uint64_t get_clockdiff_ns(struct timespec prev_clock) {
    struct timespec now_clock = get_clock();

    return (now_clock.tv_sec  - prev_clock.tv_sec ) * TO_NANOSECOND
         + (now_clock.tv_nsec - prev_clock.tv_nsec);
}

// I hate reinventing the wheel, but simply doing get_clockdiff_ns() / 1000000
//   may not work if the high bits of get_clockdiff_ns() have overflowed
uint64_t get_clockdiff_ms(struct timespec prev_clock) {
    struct timespec now_clock = get_clock();

    return (now_clock.tv_sec  - prev_clock.tv_sec ) * TO_MILLISECOND
         + (now_clock.tv_nsec - prev_clock.tv_nsec) / TO_MICROSECOND;
}

// Ditto for here, except overflows converted to doubles have a tendancy
//   to result in negative values
double get_clockdiff_s(struct timespec prev_clock) {
    struct timespec now_clock = get_clock();

    return (double)(now_clock.tv_sec  - prev_clock.tv_sec )
         + (double)(now_clock.tv_nsec - prev_clock.tv_nsec) / (double)TO_NANOSECOND;
}