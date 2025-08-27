#define _POSIX_C_SOURCE 200809L

#include "timing_utils.h"

#include <time.h>

#define MICROSECONDS_IN_SECOND 1000000LL
#define NANOSECONDS_IN_MICROSECOND 1000LL

long long TimingUtils_measure_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         const FrameDimensions *),
    const unsigned char *source_frame, unsigned char *destination_frame,
    const FrameDimensions *frame_dimensions) {
    struct timespec start_time;
    struct timespec end_time;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    convert_func(source_frame, destination_frame, frame_dimensions);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    return ((end_time.tv_sec - start_time.tv_sec) * MICROSECONDS_IN_SECOND) +
           ((end_time.tv_nsec - start_time.tv_nsec) /
            NANOSECONDS_IN_MICROSECOND);
}
