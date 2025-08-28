#ifndef V4L2_DEVICE_API_H
#define V4L2_DEVICE_API_H

#include <stdbool.h>

#include "../../common/common_types.h"
#include "constants.h"

typedef struct {
    int file_descriptor;
    MemoryMappedBuffer mapped_buffers[V4L2_MAX_BUFFERS];
    unsigned int buffer_count;
} __attribute__((aligned(128))) V4l2DeviceContext;

int V4l2Device_open(const char *device_path);
void V4l2Device_close(int file_descriptor);

void V4l2Device_select_highest_resolution(
    int video_file_descriptor, FrameDimensions *out_frame_dimensions);

bool V4l2Device_configure_video_format(int video_file_descriptor,
                                       FrameDimensions *frame_dimensions);

bool V4l2Device_setup_memory_mapped_buffers(V4l2DeviceContext *device,
                                            unsigned int buffer_count);

void V4l2Device_unmap_buffers(V4l2DeviceContext *device);

bool V4l2Device_start_video_stream(int video_file_descriptor);

void V4l2Device_stop_video_stream(int video_file_descriptor);

#endif  // V4L2_DEVICE_API_H
