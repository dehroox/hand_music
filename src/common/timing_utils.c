#include "timing_utils.h"

#include <assert.h>
#include <bits/time.h>
#include <time.h>

#include "common_types.h"

long long TimingUtils_measure_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         const FrameDimensions *),
    const unsigned char *source_frame, unsigned char *destination_frame,
    const FrameDimensions *frame_dimensions) {
    const long long MICROSECONDS_IN_SECOND = 1000000LL;
    const long long NANOSECONDS_IN_MICROSECOND = 1000LL;

    assert(convert_func != NULL && "convert_func cannot be NULL");
    assert(source_frame != NULL && "source_frame cannot be NULL");
    assert(destination_frame != NULL && "destination_frame cannot be NULL");
    assert(frame_dimensions != NULL && "frame_dimensions cannot be NULL");
    assert(frame_dimensions->width > 0 &&
           "frame_dimensions->width must be greater than 0");
    assert(frame_dimensions->height > 0 &&
           "frame_dimensions->height must be greater than 0");

    struct timespec start_time;
    struct timespec end_time;

    (void)clock_gettime(CLOCK_MONOTONIC, &start_time);
    convert_func(source_frame, destination_frame, frame_dimensions);
    (void)clock_gettime(CLOCK_MONOTONIC, &end_time);

    return ((end_time.tv_sec - start_time.tv_sec) * MICROSECONDS_IN_SECOND) +
           ((end_time.tv_nsec - start_time.tv_nsec) /
            NANOSECONDS_IN_MICROSECOND);
}
