#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <stdatomic.h>
#include <stdbool.h>

#include "common_types.h"
#include "v4l2_device_api.h"
#include "x11_utils.h"

typedef struct {
    struct V4l2Device_Device *device;
    X11Context *x11_context;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    struct FrameDimensions frame_dimensions;
    _Atomic bool *running_flag;
} CaptureThreadArguments;

void *CaptureThread_function(void *arguments);

#endif  // CAPTURE_THREAD_H
