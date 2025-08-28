// clang-format off
#include <time.h>
// clang-format on
#include <assert.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../common/branch_prediction.h"
#include "../common/common_types.h"
#include "../common/ioctl_utils.h"
#include "include/v4l2_device_api.h"

#define PIXEL_FORMAT V4L2_PIX_FMT_YUYV
#define VIDEO_CAPTURE_TYPE V4L2_BUF_TYPE_VIDEO_CAPTURE

static inline bool process_frame_size_enumeration(
    struct v4l2_frmsizeenum *frame_size_enumerator,
    FrameDimensions *out_frame_dimensions, uint32_t *current_max_area) {
    assert(frame_size_enumerator != NULL &&
           "frame_size_enumerator cannot be NULL");
    assert(out_frame_dimensions != NULL &&
           "out_frame_dimensions cannot be NULL");
    assert(current_max_area != NULL && "current_max_area cannot be NULL");

    const uint32_t frame_type = frame_size_enumerator->type;

    if (UNLIKELY(frame_type == V4L2_FRMSIZE_TYPE_STEPWISE)) {
        out_frame_dimensions->width = frame_size_enumerator->stepwise.max_width;
        out_frame_dimensions->height =
            frame_size_enumerator->stepwise.max_height;
        return true;  // Indicate that we found a stepwise type and can break
    }

    if (LIKELY(frame_type == V4L2_FRMSIZE_TYPE_DISCRETE)) {
        uint32_t discrete_width = frame_size_enumerator->discrete.width;
        uint32_t discrete_height = frame_size_enumerator->discrete.height;
        uint32_t candidate_area = discrete_width * discrete_height;

        if (LIKELY(candidate_area > *current_max_area)) {
            *current_max_area = candidate_area;
            out_frame_dimensions->width = discrete_width;
            out_frame_dimensions->height = discrete_height;
        }
    }
    return false;  // Continue enumeration
}

static inline bool process_buffer_setup(int video_file_descriptor,
                                        V4l2DeviceContext *device_reference,
                                        unsigned int index) {
    struct v4l2_buffer buffer_descriptor;
    memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));

    buffer_descriptor.type = VIDEO_CAPTURE_TYPE;
    buffer_descriptor.memory = V4L2_MEMORY_MMAP;
    buffer_descriptor.index = index;

    if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_QUERYBUF,
                                         &buffer_descriptor) ==
                 INVALID_FILE_DESCRIPTOR)) {
        V4l2Device_unmap_buffers(device_reference);
        return false;
    }

    void *mmap_start_address =
        mmap(NULL, buffer_descriptor.length, PROT_READ | PROT_WRITE, MAP_SHARED,
             video_file_descriptor, buffer_descriptor.m.offset);

    if (UNLIKELY(mmap_start_address == MAP_FAILED)) {
        V4l2Device_unmap_buffers(device_reference);
        return false;
    }

    device_reference->mapped_buffers[index].start_address = mmap_start_address;
    device_reference->mapped_buffers[index].length_bytes =
        buffer_descriptor.length;

    if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_QBUF,
                                         &buffer_descriptor) ==
                 INVALID_FILE_DESCRIPTOR)) {
        V4l2Device_unmap_buffers(device_reference);
        return false;
    }
    return true;
}

int V4l2Device_open(const char *video_device_path) {
    assert(video_device_path != NULL && "video_device_path cannot be NULL");
    const int video_file_descriptor = open(video_device_path, O_RDWR);

    if (UNLIKELY(video_file_descriptor == INVALID_FILE_DESCRIPTOR)) {
        (void)fprintf(stderr, "Failed to open video device %s\n",
                      video_device_path);
    }

    return video_file_descriptor;
}

void V4l2Device_close(int video_file_descriptor) {
    assert(video_file_descriptor != INVALID_FILE_DESCRIPTOR &&
           "video_file_descriptor cannot be INVALID_FILE_DESCRIPTOR");
    close(video_file_descriptor);
}

void V4l2Device_select_highest_resolution(
    int video_file_descriptor, FrameDimensions *out_frame_dimensions) {
    assert(video_file_descriptor != INVALID_FILE_DESCRIPTOR &&
           "video_file_descriptor cannot be INVALID_FILE_DESCRIPTOR");
    assert(out_frame_dimensions != NULL &&
           "out_frame_dimensions cannot be NULL");
    struct v4l2_frmsizeenum frame_size_enumerator;
    uint32_t current_max_area;

    out_frame_dimensions->width = 0U;
    out_frame_dimensions->height = 0U;
    out_frame_dimensions->stride_bytes = 0U;

    memset(&frame_size_enumerator, 0, sizeof(frame_size_enumerator));
    frame_size_enumerator.index = 0;
    frame_size_enumerator.pixel_format = PIXEL_FORMAT;

    current_max_area = 0;

    while (LIKELY(ioctl(video_file_descriptor, VIDIOC_ENUM_FRAMESIZES,
                        &frame_size_enumerator) != INVALID_FILE_DESCRIPTOR)) {
        if (process_frame_size_enumeration(&frame_size_enumerator,
                                           out_frame_dimensions,
                                           &current_max_area)) {
            break;
        }
        ++frame_size_enumerator.index;
    }

    (void)fprintf(
        stdout,
        "Using highest supported resolution: %ux%u (Aspect ratio: %f)\n",
        out_frame_dimensions->width, out_frame_dimensions->height,
        (double)out_frame_dimensions->width / out_frame_dimensions->height);
}

bool V4l2Device_configure_video_format(int video_file_descriptor,
                                       FrameDimensions *frame_dimensions) {
    assert(video_file_descriptor != INVALID_FILE_DESCRIPTOR &&
           "video_file_descriptor cannot be INVALID_FILE_DESCRIPTOR");
    assert(frame_dimensions != NULL && "frame_dimensions cannot be NULL");
    assert(frame_dimensions->width > 0 &&
           "frame_dimensions->width must be greater than 0");
    assert(frame_dimensions->height > 0 &&
           "frame_dimensions->height must be greater than 0");
    struct v4l2_format video_format_descriptor;

    memset(&video_format_descriptor, 0, sizeof(video_format_descriptor));

    video_format_descriptor.type = VIDEO_CAPTURE_TYPE;
    uint32_t frame_width = frame_dimensions->width;
    uint32_t frame_height = frame_dimensions->height;
    video_format_descriptor.fmt.pix.width = frame_width;
    video_format_descriptor.fmt.pix.height = frame_height;
    video_format_descriptor.fmt.pix.pixelformat = PIXEL_FORMAT;
    video_format_descriptor.fmt.pix.field = V4L2_FIELD_NONE;

    if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_S_FMT,
                                         &video_format_descriptor) ==
                 INVALID_FILE_DESCRIPTOR)) {
        return false;
    }

    uint32_t bytes_per_line = video_format_descriptor.fmt.pix.bytesperline;
    frame_dimensions->stride_bytes = bytes_per_line;
    return true;
}

bool V4l2Device_setup_memory_mapped_buffers(
    V4l2DeviceContext *device_reference, unsigned int requested_buffer_count) {
    const unsigned char MINIMUM_BUFFER_COUNT = 2;

    assert(device_reference != NULL && "device_reference cannot be NULL");
    assert(
        device_reference->file_descriptor != INVALID_FILE_DESCRIPTOR &&
        "device_reference->file_descriptor cannot be INVALID_FILE_DESCRIPTOR");
    assert(requested_buffer_count > 0 &&
           "requested_buffer_count must be greater than 0");
    int video_file_descriptor = device_reference->file_descriptor;
    struct v4l2_requestbuffers buffer_request;

    memset(&buffer_request, 0, sizeof(buffer_request));
    buffer_request.count = requested_buffer_count;
    buffer_request.type = VIDEO_CAPTURE_TYPE;
    buffer_request.memory = V4L2_MEMORY_MMAP;

    if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_REQBUFS,
                                         &buffer_request) ==
                 INVALID_FILE_DESCRIPTOR)) {
        return false;
    }

    if (UNLIKELY(buffer_request.count < MINIMUM_BUFFER_COUNT)) {
        return false;
    }

    device_reference->buffer_count = buffer_request.count;
    for (unsigned int i = 0; i < device_reference->buffer_count; ++i) {
        if (!process_buffer_setup(video_file_descriptor, device_reference, i)) {
            return false;
        }
    }

    return true;
}

void V4l2Device_unmap_buffers(V4l2DeviceContext *device_reference) {
    assert(device_reference != NULL && "device_reference cannot be NULL");
    for (unsigned int i = 0; i < device_reference->buffer_count; ++i) {
        if (LIKELY(device_reference->mapped_buffers[i].start_address)) {
            munmap(device_reference->mapped_buffers[i].start_address,
                   device_reference->mapped_buffers[i].length_bytes);
        }
    }
}

bool V4l2Device_start_video_stream(int video_file_descriptor) {
    assert(video_file_descriptor != INVALID_FILE_DESCRIPTOR &&
           "video_file_descriptor cannot be INVALID_FILE_DESCRIPTOR");
    enum v4l2_buf_type capture_buffer_type;

    capture_buffer_type = VIDEO_CAPTURE_TYPE;

    return (bool)(continually_retry_ioctl(
                      video_file_descriptor, VIDIOC_STREAMON,
                      &capture_buffer_type) != INVALID_FILE_DESCRIPTOR);
}

void V4l2Device_stop_video_stream(int video_file_descriptor) {
    assert(video_file_descriptor != INVALID_FILE_DESCRIPTOR &&
           "video_file_descriptor cannot be INVALID_FILE_DESCRIPTOR");
    enum v4l2_buf_type capture_buffer_type;

    capture_buffer_type = VIDEO_CAPTURE_TYPE;

    continually_retry_ioctl(video_file_descriptor, VIDIOC_STREAMOFF,
                            &capture_buffer_type);
}
