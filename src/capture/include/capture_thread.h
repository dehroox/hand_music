#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <stdatomic.h>
#include <stdbool.h>

#include "../../common/common_types.h"

#include "v4l2_device_api.h"

typedef struct {
    V4l2DeviceContext *device;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    struct FrameDimensions frame_dimensions;
    _Atomic bool *running_flag;
    void (*display_update_callback)(void *context, unsigned char *frame_data);
    void *display_update_context;
} CaptureThreadArguments;

void *CaptureThread_run(void *arguments);

#endif  // CAPTURE_THREAD_H
