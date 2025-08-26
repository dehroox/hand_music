#ifndef FRAME_PROCESSING_H
#define FRAME_PROCESSING_H

#include <stddef.h>

#include "common_types.h"

void FrameProcessing_flip_rgb_horizontal(
    const unsigned char *source_rgb_buffer,
    unsigned char *destination_rgb_buffer,
    struct FrameDimensions frame_dimensions);

long long FrameProcessing_measure_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         struct FrameDimensions),
    const unsigned char *source_frame, unsigned char *destination_frame,
    struct FrameDimensions frame_dimensions);

#endif  // FRAME_PROCESSING_H
