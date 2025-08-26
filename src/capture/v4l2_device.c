#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../common/common_types.h"
#include "../common/ioctl_utils.h"
#include "include/v4l2_device_api.h"
#include "linux/videodev2.h"

typedef struct {
    int file_descriptor;
    struct V4l2Device_Device *device;
    unsigned int buffer_index;
    bool *result_flag;
} MapBufferThreadArguments;

static void *map_single_buffer_thread(void *arguments) {
    MapBufferThreadArguments *thread_arguments =
        (MapBufferThreadArguments *)arguments;

    int video_file_descriptor = thread_arguments->file_descriptor;
    struct V4l2Device_Device *device_reference = thread_arguments->device;
    unsigned int buffer_index = thread_arguments->buffer_index;
    bool *result_flag_pointer = thread_arguments->result_flag;

    struct v4l2_buffer buffer_descriptor;
    memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));

    buffer_descriptor.type = VIDEO_CAPTURE_TYPE;
    buffer_descriptor.memory = V4L2_MEMORY_MMAP;
    buffer_descriptor.index = buffer_index;

    if (continually_retry_ioctl(video_file_descriptor, VIDIOC_QUERYBUF,
                                &buffer_descriptor) == -1) {
        *result_flag_pointer = false;
        return NULL;
    }

    device_reference->mapped_buffers[buffer_index].start_address =
        mmap(NULL, buffer_descriptor.length, PROT_READ | PROT_WRITE, MAP_SHARED,
             video_file_descriptor, buffer_descriptor.m.offset);

    if (device_reference->mapped_buffers[buffer_index].start_address ==
        MAP_FAILED) {
        *result_flag_pointer = false;
        return NULL;
    }

    device_reference->mapped_buffers[buffer_index].length_bytes =
        buffer_descriptor.length;

    if (continually_retry_ioctl(video_file_descriptor, VIDIOC_QBUF,
                                &buffer_descriptor) == -1) {
        *result_flag_pointer = false;
    } else {
        *result_flag_pointer = true;
    }

    return NULL;
}

int V4l2Device_open(const char *video_device_path) {
    int video_file_descriptor = open(video_device_path, O_RDWR);

    if (video_file_descriptor == -1) {
        fprintf(stderr, "Failed to open video device %s\n", video_device_path);
    }

    return video_file_descriptor;
}

void V4l2Device_close_device(int video_file_descriptor) {
    close(video_file_descriptor);
}

struct FrameDimensions V4l2Device_select_highest_resolution(
    int video_file_descriptor) {
    struct FrameDimensions selected_frame_dimensions;
    struct v4l2_frmsizeenum frame_size_enumerator;
    uint32_t candidate_area;
    uint32_t current_max_area;

    selected_frame_dimensions.width = 0U;
    selected_frame_dimensions.height = 0U;
    selected_frame_dimensions.stride_bytes = 0U;

    memset(&frame_size_enumerator, 0, sizeof(frame_size_enumerator));
    frame_size_enumerator.index = 0;
    frame_size_enumerator.pixel_format = PIXEL_FORMAT;

    while (ioctl(video_file_descriptor, VIDIOC_ENUM_FRAMESIZES,
                 &frame_size_enumerator) != -1) {
        if (frame_size_enumerator.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            selected_frame_dimensions.width =
                frame_size_enumerator.stepwise.max_width;
            selected_frame_dimensions.height =
                frame_size_enumerator.stepwise.max_height;
            break;
        }

        if (frame_size_enumerator.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            candidate_area = (uint32_t)frame_size_enumerator.discrete.width *
                             (uint32_t)frame_size_enumerator.discrete.height;
            current_max_area = (uint32_t)selected_frame_dimensions.width *
                               (uint32_t)selected_frame_dimensions.height;

            if (candidate_area > current_max_area) {
                selected_frame_dimensions.width =
                    frame_size_enumerator.discrete.width;
                selected_frame_dimensions.height =
                    frame_size_enumerator.discrete.height;
            }
        }

        ++frame_size_enumerator.index;
    }

    fprintf(stdout,
            "Using highest supported resolution: %ux%u (Aspect ratio: %f)\n",
            selected_frame_dimensions.width, selected_frame_dimensions.height,
            (double)selected_frame_dimensions.width /
                selected_frame_dimensions.height);

    return selected_frame_dimensions;
}

bool V4l2Device_configure_video_format(
    int video_file_descriptor, struct FrameDimensions *frame_dimensions) {
    struct v4l2_format video_format_descriptor;

    memset(&video_format_descriptor, 0, sizeof(video_format_descriptor));

    video_format_descriptor.type = VIDEO_CAPTURE_TYPE;
    video_format_descriptor.fmt.pix.width = frame_dimensions->width;
    video_format_descriptor.fmt.pix.height = frame_dimensions->height;
    video_format_descriptor.fmt.pix.pixelformat = PIXEL_FORMAT;
    video_format_descriptor.fmt.pix.field = V4L2_FIELD_NONE;

    if (continually_retry_ioctl(video_file_descriptor, VIDIOC_S_FMT,
                                &video_format_descriptor) == -1) {
        return false;
    }

    frame_dimensions->stride_bytes =
        video_format_descriptor.fmt.pix.bytesperline;
    return true;
}

bool V4l2Device_setup_memory_mapped_buffers(
    struct V4l2Device_Device *device_reference,
    unsigned int requested_buffer_count) {
    int video_file_descriptor = device_reference->file_descriptor;
    struct v4l2_requestbuffers buffer_request;
    pthread_t *thread_handles;
    MapBufferThreadArguments *thread_arguments_array;
    bool *thread_results;
    bool all_buffers_mapped_successfully;
    unsigned int buffer_index;

    memset(&buffer_request, 0, sizeof(buffer_request));
    buffer_request.count = requested_buffer_count;
    buffer_request.type = VIDEO_CAPTURE_TYPE;
    buffer_request.memory = V4L2_MEMORY_MMAP;

    if (continually_retry_ioctl(video_file_descriptor, VIDIOC_REQBUFS,
                                &buffer_request) == -1) {
        return false;
    }

    if (buffer_request.count < MINIMUM_BUFFER_COUNT) {
        return false;
    }

    device_reference->buffer_count = buffer_request.count;

    thread_handles = malloc(device_reference->buffer_count * sizeof(*thread_handles));
    if (!thread_handles) {
        return false;
    }

    thread_arguments_array = malloc(device_reference->buffer_count * sizeof(*thread_arguments_array));
    if (!thread_arguments_array) {
        free(thread_handles);
        return false;
    }

    thread_results = malloc(device_reference->buffer_count * sizeof(*thread_results));
    if (!thread_results) {
        free(thread_arguments_array);
        free(thread_handles);
        return false;
    }

    for (buffer_index = 0; buffer_index < device_reference->buffer_count;
         ++buffer_index) {
        thread_arguments_array[buffer_index].file_descriptor =
            video_file_descriptor;
        thread_arguments_array[buffer_index].device = device_reference;
        thread_arguments_array[buffer_index].buffer_index = buffer_index;
        thread_arguments_array[buffer_index].result_flag =
            &thread_results[buffer_index];

        if (pthread_create(&thread_handles[buffer_index], NULL,
                           map_single_buffer_thread,
                           &thread_arguments_array[buffer_index]) != 0) {
            thread_results[buffer_index] = false;
        }
    }

    all_buffers_mapped_successfully = true;
    for (buffer_index = 0; buffer_index < device_reference->buffer_count;
         ++buffer_index) {
        pthread_join(thread_handles[buffer_index], NULL);

        if (!thread_results[buffer_index]) {
            all_buffers_mapped_successfully = false;
        }
    }

    free(thread_handles);
    free(thread_arguments_array);
    free(thread_results);

    return all_buffers_mapped_successfully;
}

void V4l2Device_unmap_buffers(struct V4l2Device_Device *device_reference) {
    struct MemoryMappedBuffer *mapped_buffers_reference;
    unsigned int buffer_index;

    mapped_buffers_reference = device_reference->mapped_buffers;

    for (buffer_index = 0; buffer_index < device_reference->buffer_count;
         ++buffer_index) {
        munmap(mapped_buffers_reference[buffer_index].start_address,
               mapped_buffers_reference[buffer_index].length_bytes);
    }
}

bool V4l2Device_start_video_stream(int video_file_descriptor) {
    enum v4l2_buf_type capture_buffer_type;

    capture_buffer_type = VIDEO_CAPTURE_TYPE;

    return (bool)(continually_retry_ioctl(video_file_descriptor,
                                          VIDIOC_STREAMON,
                                          &capture_buffer_type) != -1);
}

void V4l2Device_stop_video_stream(int video_file_descriptor) {
    enum v4l2_buf_type capture_buffer_type;

    capture_buffer_type = VIDEO_CAPTURE_TYPE;

    continually_retry_ioctl(video_file_descriptor, VIDIOC_STREAMOFF,
                            &capture_buffer_type);
}
