#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "hand_music.h"
#include "linux/videodev2.h"

struct MapBufferThreadArgs {
    int file_descriptor;
    struct V4l2Device_Device *device;
    unsigned int buffer_index;
    bool *result_ptr;
};

void *map_buffer_thread(void *arg) {
    struct MapBufferThreadArgs *args = (struct MapBufferThreadArgs *)arg;
    int file_descriptor = args->file_descriptor;
    struct V4l2Device_Device *device = args->device;
    unsigned int buffer_index = args->buffer_index;
    bool *result_ptr = args->result_ptr;

    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = buffer_index;

    if (continually_retry_ioctl(file_descriptor, VIDIOC_QUERYBUF, &buffer) ==
        -1) {
        *result_ptr = false;
        return NULL;
    }

    device->mapped_buffers[buffer_index].start_address =
        mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
             file_descriptor, buffer.m.offset);
    if (device->mapped_buffers[buffer_index].start_address == MAP_FAILED) {
        *result_ptr = false;
        return NULL;
    }

    device->mapped_buffers[buffer_index].length_bytes = buffer.length;

    *result_ptr = (bool)(continually_retry_ioctl(file_descriptor, VIDIOC_QBUF,
                                                 &buffer) != -1);
    return NULL;
}

int V4l2Device_open(const char *device_path) {
    const int file_descriptor = open(device_path, O_RDWR);
    if (file_descriptor == -1) {
        (void)fprintf(stderr, "Failed to open video device %s\n", device_path);
    }
    return file_descriptor;
}

void V4l2Device_close_device(int file_descriptor) { close(file_descriptor); }

struct FrameDimensions V4l2Device_select_highest_resolution(
    int video_file_descriptor) {
    struct FrameDimensions frame_dimensions = {
        .width = 0U, .height = 0U, .stride_bytes = 0U};
    struct v4l2_frmsizeenum frame_size;
    memset(&frame_size, 0, sizeof(frame_size));
    frame_size.index = 0;
    frame_size.pixel_format = V4L2_PIX_FMT_YUYV;

    while (ioctl(video_file_descriptor, VIDIOC_ENUM_FRAMESIZES, &frame_size) !=
           -1) {
        if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            frame_dimensions.width = frame_size.stepwise.max_width;
            frame_dimensions.height = frame_size.stepwise.max_height;
            break;
        }
        if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            uint32_t area = ((uint32_t)frame_size.discrete.width *
                             (uint32_t)frame_size.discrete.height);
            uint32_t current_area = ((uint32_t)frame_dimensions.width *
                                     (uint32_t)frame_dimensions.height);
            if (area > current_area) {
                frame_dimensions.width = frame_size.discrete.width;
                frame_dimensions.height = frame_size.discrete.height;
            }
        }
        ++frame_size.index;
    }

    (void)fprintf(
        stdout,
        "Using highest supported resolution: %ux%u (Aspect ratio: %f)\n",
        frame_dimensions.width, frame_dimensions.height,
        (double)frame_dimensions.width / frame_dimensions.height);

    return frame_dimensions;
}

bool V4l2Device_configure_video_format(
    int video_file_descriptor, struct FrameDimensions *frame_dimensions) {
    struct v4l2_format video_format;
    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.width = frame_dimensions->width;
    video_format.fmt.pix.height = frame_dimensions->height;
    video_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    video_format.fmt.pix.field = V4L2_FIELD_NONE;

    if (continually_retry_ioctl(video_file_descriptor, VIDIOC_S_FMT,
                                &video_format) == -1) {
        return false;
    }
    frame_dimensions->stride_bytes = video_format.fmt.pix.bytesperline;
    return true;
}

bool V4l2Device_setup_memory_mapped_buffers(struct V4l2Device_Device *device,
                                            unsigned int buffer_count) {
    int file_descriptor = device->file_descriptor;
    struct v4l2_requestbuffers request_buffers;
    memset(&request_buffers, 0, sizeof(request_buffers));
    request_buffers.count = buffer_count;
    request_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request_buffers.memory = V4L2_MEMORY_MMAP;

    if (continually_retry_ioctl(file_descriptor, VIDIOC_REQBUFS,
                                &request_buffers) == -1 ||
        request_buffers.count < 2) {
        return false;
    }

    device->buffer_count = request_buffers.count;

    pthread_t *threads = malloc(device->buffer_count * sizeof(pthread_t));
    struct MapBufferThreadArgs *thread_args =
        malloc(device->buffer_count * sizeof(*thread_args));
    bool *results = malloc(device->buffer_count * sizeof(*results));

    if (!threads || !thread_args || !results) {
        free(threads);
        free(thread_args);
        free(results);
        return false;
    }

    for (unsigned int i = 0; i < device->buffer_count; ++i) {
        thread_args[i].file_descriptor = file_descriptor;
        thread_args[i].device = device;
        thread_args[i].buffer_index = i;
        thread_args[i].result_ptr = &results[i];
        if (pthread_create(&threads[i], NULL, map_buffer_thread,
                           &thread_args[i]) != 0) {
            results[i] = false;
        }
    }

    bool all_success = true;
    for (unsigned int i = 0; i < device->buffer_count; ++i) {
        pthread_join(threads[i], NULL);
        if (!results[i]) {
            all_success = false;
        }
    }

    free(threads);
    free(thread_args);
    free(results);
    return all_success;
}

void V4l2Device_unmap_buffers(struct V4l2Device_Device *device) {
    struct MemoryMappedBuffer *mapped_buffers_ref = device->mapped_buffers;
    for (unsigned int i = 0; i < device->buffer_count; ++i) {
        munmap(mapped_buffers_ref[i].start_address,
               mapped_buffers_ref[i].length_bytes);
    }
}

bool V4l2Device_start_video_stream(int video_file_descriptor) {
    enum v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return (bool)(continually_retry_ioctl(video_file_descriptor,
                                          VIDIOC_STREAMON,
                                          &capture_type) != -1);
}

void V4l2Device_stop_video_stream(int video_file_descriptor) {
    enum v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    continually_retry_ioctl(video_file_descriptor, VIDIOC_STREAMOFF,
                            &capture_type);
}
