#ifndef TIMING_UTILS_H
#define TIMING_UTILS_H

#include "common_types.h"

long long TimingUtils_measure_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         FrameDimensions *),
    const unsigned char *source_frame, unsigned char *destination_frame,
    FrameDimensions *frame_dimensions);

#endif  // TIMING_UTILS_H
