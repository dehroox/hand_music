// clang-format off
#include <time.h>
// clang-format on
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

#define MINIMUM_BUFFER_COUNT 2
#define PIXEL_FORMAT V4L2_PIX_FMT_YUYV
#define VIDEO_CAPTURE_TYPE V4L2_BUF_TYPE_VIDEO_CAPTURE

int V4l2Device_open(const char *video_device_path) {
    int video_file_descriptor = open(video_device_path, O_RDWR);

    if (UNLIKELY(video_file_descriptor == -1)) {
        fprintf(stderr, "Failed to open video device %s\n", video_device_path);
    }

    return video_file_descriptor;
}

void V4l2Device_close(int video_file_descriptor) {
    close(video_file_descriptor);
}

void V4l2Device_select_highest_resolution(
    int video_file_descriptor, FrameDimensions *out_frame_dimensions) {
    struct v4l2_frmsizeenum frame_size_enumerator;
    uint32_t candidate_area;
    uint32_t current_max_area;

    out_frame_dimensions->width = 0U;
    out_frame_dimensions->height = 0U;
    out_frame_dimensions->stride_bytes = 0U;

    memset(&frame_size_enumerator, 0, sizeof(frame_size_enumerator));
    frame_size_enumerator.index = 0;
    frame_size_enumerator.pixel_format = PIXEL_FORMAT;

    current_max_area = 0;
    while (LIKELY(ioctl(video_file_descriptor, VIDIOC_ENUM_FRAMESIZES,
                        &frame_size_enumerator) != -1)) {
        if (UNLIKELY(frame_size_enumerator.type ==
                     V4L2_FRMSIZE_TYPE_STEPWISE)) {
            out_frame_dimensions->width =
                frame_size_enumerator.stepwise.max_width;
            out_frame_dimensions->height =
                frame_size_enumerator.stepwise.max_height;
            break;
        }

        if (LIKELY(frame_size_enumerator.type == V4L2_FRMSIZE_TYPE_DISCRETE)) {
            candidate_area = (uint32_t)frame_size_enumerator.discrete.width *
                             (uint32_t)frame_size_enumerator.discrete.height;

            if (LIKELY(candidate_area > current_max_area)) {
                current_max_area = candidate_area;
                out_frame_dimensions->width =
                    frame_size_enumerator.discrete.width;
                out_frame_dimensions->height =
                    frame_size_enumerator.discrete.height;
            }
        }

        ++frame_size_enumerator.index;
    }

    fprintf(stdout,
            "Using highest supported resolution: %ux%u (Aspect ratio: %f)\n",
            out_frame_dimensions->width, out_frame_dimensions->height,
            (double)out_frame_dimensions->width /
                out_frame_dimensions->height);
}

bool V4l2Device_configure_video_format(int video_file_descriptor,
                                       FrameDimensions *frame_dimensions) {
    struct v4l2_format video_format_descriptor;

    memset(&video_format_descriptor, 0, sizeof(video_format_descriptor));

    video_format_descriptor.type = VIDEO_CAPTURE_TYPE;
    video_format_descriptor.fmt.pix.width = frame_dimensions->width;
    video_format_descriptor.fmt.pix.height = frame_dimensions->height;
    video_format_descriptor.fmt.pix.pixelformat = PIXEL_FORMAT;
    video_format_descriptor.fmt.pix.field = V4L2_FIELD_NONE;

    if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_S_FMT,
                                         &video_format_descriptor) == -1)) {
        return false;
    }

    frame_dimensions->stride_bytes =
        video_format_descriptor.fmt.pix.bytesperline;
    return true;
}

bool V4l2Device_setup_memory_mapped_buffers(
    V4l2DeviceContext *device_reference, unsigned int requested_buffer_count) {
    int video_file_descriptor = device_reference->file_descriptor;
    struct v4l2_requestbuffers buffer_request;

    memset(&buffer_request, 0, sizeof(buffer_request));
    buffer_request.count = requested_buffer_count;
    buffer_request.type = VIDEO_CAPTURE_TYPE;
    buffer_request.memory = V4L2_MEMORY_MMAP;

    if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_REQBUFS,
                                         &buffer_request) == -1)) {
        return false;
    }

    if (UNLIKELY(buffer_request.count < MINIMUM_BUFFER_COUNT)) {
        return false;
    }

    device_reference->buffer_count = buffer_request.count;

    for (unsigned int i = 0; i < device_reference->buffer_count; ++i) {
        struct v4l2_buffer buffer_descriptor;
        memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));

        buffer_descriptor.type = VIDEO_CAPTURE_TYPE;
        buffer_descriptor.memory = V4L2_MEMORY_MMAP;
        buffer_descriptor.index = i;

        if (UNLIKELY(continually_retry_ioctl(video_file_descriptor,
                                             VIDIOC_QUERYBUF,
                                             &buffer_descriptor) == -1)) {
            V4l2Device_unmap_buffers(device_reference);
            return false;
        }

        device_reference->mapped_buffers[i].start_address =
            mmap(NULL, buffer_descriptor.length, PROT_READ | PROT_WRITE,
                 MAP_SHARED, video_file_descriptor, buffer_descriptor.m.offset);

        if (UNLIKELY(device_reference->mapped_buffers[i].start_address ==
                     MAP_FAILED)) {
            V4l2Device_unmap_buffers(device_reference);
            return false;
        }

        device_reference->mapped_buffers[i].length_bytes =
            buffer_descriptor.length;

        if (UNLIKELY(continually_retry_ioctl(video_file_descriptor, VIDIOC_QBUF,
                                             &buffer_descriptor) == -1)) {
            V4l2Device_unmap_buffers(device_reference);
            return false;
        }
    }

    return true;
}

void V4l2Device_unmap_buffers(V4l2DeviceContext *device_reference) {
    for (unsigned int i = 0; i < device_reference->buffer_count; ++i) {
        if (LIKELY(device_reference->mapped_buffers[i].start_address)) {
            munmap(device_reference->mapped_buffers[i].start_address,
                   device_reference->mapped_buffers[i].length_bytes);
        }
    }
}

bool V4l2Device_start_video_stream(int video_file_descriptor) {
    enum v4l2_buf_type capture_buffer_type;

    capture_buffer_type = VIDEO_CAPTURE_TYPE;

    return (bool)LIKELY(continually_retry_ioctl(video_file_descriptor,
                                                VIDIOC_STREAMON,
                                                &capture_buffer_type) != -1);
}

void V4l2Device_stop_video_stream(int video_file_descriptor) {
    enum v4l2_buf_type capture_buffer_type;

    capture_buffer_type = VIDEO_CAPTURE_TYPE;

    continually_retry_ioctl(video_file_descriptor, VIDIOC_STREAMOFF,
                            &capture_buffer_type);
}
