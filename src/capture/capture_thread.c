#include "include/capture_thread.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../common/ioctl_utils.h"

#include "include/frame_processing.h"
#include "include/image_conversions.h"
#include "include/v4l2_device_api.h"
#include "linux/videodev2.h"

void *CaptureThread_function(void *arguments) {
    CaptureThreadArguments *thread_arguments =
        (CaptureThreadArguments *)arguments;

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

    while (atomic_load_explicit(thread_arguments->running_flag,
                                memory_order_relaxed)) {
        int ioctl_result =
            continually_retry_ioctl(thread_arguments->device->file_descriptor,
                                    VIDIOC_DQBUF, &capture_buffer);
        if (ioctl_result == -1) {
            continue;
        }

        unsigned char *yuv_frame_data =
            (unsigned char *)thread_arguments->device
                ->mapped_buffers[capture_buffer.index]
                .start_address;

        total_rgb_conversion_time_microseconds +=
            FrameProcessing_measure_conversion_time(
                convert_yuv_to_rgb, yuv_frame_data,
                thread_arguments->rgb_frame_buffer,
                thread_arguments->frame_dimensions);
        total_gray_conversion_time_microseconds +=
            FrameProcessing_measure_conversion_time(
                convert_yuv_to_gray, yuv_frame_data,
                thread_arguments->gray_frame_buffer,
                thread_arguments->frame_dimensions);

        captured_frame_count++;

        FrameProcessing_flip_rgb_horizontal(
            thread_arguments->rgb_frame_buffer,
            thread_arguments->rgb_flipped_buffer,
            thread_arguments->frame_dimensions);

        thread_arguments->display_update_callback(thread_arguments->display_update_context,
                               thread_arguments->rgb_flipped_buffer);

        continually_retry_ioctl(thread_arguments->device->file_descriptor,
                                VIDIOC_QBUF, &capture_buffer);
    }

    if (captured_frame_count > 0) {
        printf("Average YUV to RGB conversion time: %lld microseconds\n",
               total_rgb_conversion_time_microseconds / captured_frame_count);
        printf("Average YUV to Gray conversion time: %lld microseconds\n",
               total_gray_conversion_time_microseconds / captured_frame_count);
    }

    return NULL;
}
