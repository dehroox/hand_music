#ifndef HAND_MUSIC_H
#define HAND_MUSIC_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/ioctl.h>

struct MemoryMappedBuffer {
    void *start_address;
    size_t length_bytes;
};

struct FrameDimensions {
    unsigned int width;
    unsigned int height;
    unsigned int stride_bytes;
};

// --- Constants ---
#define K_MAX_RGB_VALUE 255
#define K_U_OFFSET 128
#define K_RGB_COMPONENTS 3

#define RGB_MULTIPLIER 1000
#define RED_V_MULTIPLIER 1402
#define GREEN_U_MULTIPLIER 344
#define GREEN_V_MULTIPLIER 714
#define BLUE_U_MULTIPLIER 1772

// when an ioctl call is interrupted by a signal (EINTR), it should be retried.
// this function wraps the ioctl call in a loop to handle such interruptions.
static inline int continually_retry_ioctl(int file_descriptor,
                                          unsigned long request,
                                          void *argument) {
    int result = -1;
    while (true) {
        result = ioctl(file_descriptor, request, argument);
        if (result != -1 || errno != EINTR) {
            break;
        }
    }
    return result;
}

#define V4L2_MAX_BUFFERS 4

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

// clamps a value to the valid 8-bit RGB range (0-255).
static inline unsigned char clamp_rgb_value(int value) {
    int clamped_value = value;
    if (clamped_value > K_MAX_RGB_VALUE) {
        clamped_value = K_MAX_RGB_VALUE;
    } else if (clamped_value < 0) {
        clamped_value = 0;
    }
    return (unsigned char)clamped_value;
}

void convert_yuv_to_rgb(const unsigned char *__restrict yuv_frame_pointer,
                        unsigned char *__restrict rgb_frame_pointer,
                        struct FrameDimensions frame_dimensions);

void convert_yuv_to_gray(const unsigned char *__restrict yuv_frame_pointer,
                         unsigned char *__restrict gray_frame_pointer,
                         struct FrameDimensions frame_dimensions);

#endif  // HAND_MUSIC_H
