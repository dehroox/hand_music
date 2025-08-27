#include "include/capture_thread.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../common/branch_prediction.h"
#include "../common/ioctl_utils.h"
#include "../common/timing_utils.h"
#include "include/image_conversions.h"
#include "include/image_processing.h"
#include "include/v4l2_device_api.h"
#include "linux/videodev2.h"

typedef struct {
    long long *total_rgb_conversion_time_microseconds;
    long long *total_gray_conversion_time_microseconds;
} ConversionTime;

static void process_frame(const CaptureThreadArguments *thread_arguments,
                          const unsigned char *yuv_frame_data,
                          struct v4l2_buffer *capture_buffer,
                          ConversionTime conversion_times,
                          int *captured_frame_count) {
    assert(thread_arguments != NULL && "thread_arguments cannot be NULL");
    assert(yuv_frame_data != NULL && "yuv_frame_data cannot be NULL");
    assert(capture_buffer != NULL && "capture_buffer cannot be NULL");
    assert(conversion_times.total_rgb_conversion_time_microseconds != NULL &&
           "total_rgb_conversion_time_microseconds cannot be NULL");
    assert(conversion_times.total_gray_conversion_time_microseconds != NULL &&
           "total_gray_conversion_time_microseconds cannot be NULL");
    assert(captured_frame_count != NULL &&
           "captured_frame_count cannot be NULL");

    *conversion_times.total_rgb_conversion_time_microseconds +=
        TimingUtils_measure_conversion_time(ImageConversions_convert_yuv_to_rgb,
                                            yuv_frame_data,
                                            thread_arguments->rgb_frame_buffer,
                                            thread_arguments->frame_dimensions);
    *conversion_times.total_gray_conversion_time_microseconds +=
        TimingUtils_measure_conversion_time(
            ImageConversions_convert_yuv_to_gray, yuv_frame_data,
            thread_arguments->gray_frame_buffer,
            thread_arguments->frame_dimensions);

    (*captured_frame_count)++;

    if (UNLIKELY(atomic_load_explicit(thread_arguments->gray_view,
                                      memory_order_relaxed))) {
        ImageProcessing_expand_grayscale(thread_arguments->gray_frame_buffer,
                                         thread_arguments->rgb_frame_buffer,
                                         thread_arguments->frame_dimensions);
    }

    ImageProcessing_flip_rgb_horizontal(thread_arguments->rgb_frame_buffer,
                                        thread_arguments->rgb_flipped_buffer,
                                        thread_arguments->frame_dimensions);
    thread_arguments->display_update_callback(
        thread_arguments->display_update_context,
        thread_arguments->rgb_flipped_buffer);

    continually_retry_ioctl(thread_arguments->device->file_descriptor,
                            VIDIOC_QBUF, capture_buffer);
}

void *CaptureThread_run(void *arguments) {
    assert(arguments != NULL && "Arguments cannot be NULL");
    const CaptureThreadArguments *thread_arguments =
        (CaptureThreadArguments *)arguments;
    assert(thread_arguments != NULL && "thread_arguments cannot be NULL");
    assert(thread_arguments->running_flag != NULL &&
           "running_flag cannot be NULL");
    assert(thread_arguments->device != NULL && "device cannot be NULL");
    assert(thread_arguments->rgb_frame_buffer != NULL &&
           "rgb_frame_buffer cannot be NULL");
    assert(thread_arguments->rgb_flipped_buffer != NULL &&
           "rgb_flipped_buffer cannot be NULL");
    assert(thread_arguments->gray_frame_buffer != NULL &&
           "gray_frame_buffer cannot be NULL");
    assert(thread_arguments->frame_dimensions != NULL &&
           "frame_dimensions cannot be NULL");
    assert(thread_arguments->gray_view != NULL && "gray_view cannot be NULL");
    assert(thread_arguments->display_update_callback != NULL &&
           "display_update_callback cannot be NULL");
    assert(thread_arguments->display_update_context != NULL &&
           "display_update_context cannot be NULL");

    struct v4l2_buffer capture_buffer;
    long long total_rgb_conversion_time_microseconds;
    long long total_gray_conversion_time_microseconds;
    int captured_frame_count;

    total_rgb_conversion_time_microseconds = 0;
    total_gray_conversion_time_microseconds = 0;
    captured_frame_count = 0;

    memset(&capture_buffer, 0, sizeof(capture_buffer));
    capture_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    capture_buffer.memory = V4L2_MEMORY_MMAP;

    while (LIKELY(atomic_load_explicit(thread_arguments->running_flag,
                                       memory_order_relaxed))) {
        int file_descriptor = thread_arguments->device->file_descriptor;
        int ioctl_result = continually_retry_ioctl(
            file_descriptor, VIDIOC_DQBUF, &capture_buffer);
        if (UNLIKELY(ioctl_result == INVALID_FILE_DESCRIPTOR)) {
            continue;
        }

        const MemoryMappedBuffer *mapped_buffers =
            thread_arguments->device->mapped_buffers;
        const MemoryMappedBuffer current_buffer =
            mapped_buffers[capture_buffer.index];
        const unsigned char *yuv_frame_data =
            (unsigned char *)current_buffer.start_address;

        const ConversionTime conversion_time = {
            &total_rgb_conversion_time_microseconds,
            &total_gray_conversion_time_microseconds,
        };

        process_frame(thread_arguments, yuv_frame_data, &capture_buffer,
                      conversion_time, &captured_frame_count);
    }

    printf("Average YUV to RGB conversion time: %lld microseconds\n",
           total_rgb_conversion_time_microseconds / captured_frame_count);
    printf("Average YUV to Gray conversion time: %lld microseconds\n",
           total_gray_conversion_time_microseconds / captured_frame_count);

    return NULL;
}
