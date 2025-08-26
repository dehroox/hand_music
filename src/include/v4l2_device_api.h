#ifndef V4L2_DEVICE_API_H
#define V4L2_DEVICE_API_H

#include <stdbool.h>

#include "common_types.h"
#include "constants.h"

struct V4l2Device_Device {
    int file_descriptor;
    struct MemoryMappedBuffer mapped_buffers[V4L2_MAX_BUFFERS];
    unsigned int buffer_count;
};

int V4l2Device_open(const char *device_path);
void V4l2Device_close_device(int file_descriptor);

struct FrameDimensions V4l2Device_select_highest_resolution(
    int video_file_descriptor);

bool V4l2Device_configure_video_format(
    int video_file_descriptor, struct FrameDimensions *frame_dimensions);

bool V4l2Device_setup_memory_mapped_buffers(struct V4l2Device_Device *device,
                                            unsigned int buffer_count);

void V4l2Device_unmap_buffers(struct V4l2Device_Device *device);

bool V4l2Device_start_video_stream(int video_file_descriptor);

void V4l2Device_stop_video_stream(int video_file_descriptor);

#endif  // V4L2_DEVICE_API_H
