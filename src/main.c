#define _POSIX_C_SOURCE 200809L

#include <X11/keysym.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capture_thread.h"
#include "include/common_types.h"
#include "include/constants.h"
#include "include/v4l2_device_api.h"
#include "include/x11_utils.h"

int main(void) {
    struct V4l2Device_Device video_device;
    struct FrameDimensions selected_frame_dimensions;
    X11Context x11_context;
    unsigned char *rgb_frame_buffer = NULL;
    unsigned char *rgb_flipped_buffer = NULL;
    unsigned char *gray_frame_buffer = NULL;
    size_t rgb_frame_buffer_size;
    size_t gray_frame_buffer_size;
    _Atomic bool running_flag = true;
    pthread_t capture_thread_handle;
    CaptureThreadArguments capture_thread_arguments;
    XEvent received_event;

    memset(&video_device, 0, sizeof(video_device));
    memset(&selected_frame_dimensions, 0, sizeof(selected_frame_dimensions));
    memset(&x11_context, 0, sizeof(x11_context));

    /* Open video device */
    video_device.file_descriptor = V4l2Device_open(VIDEO_DEVICE_PATH);
    if (video_device.file_descriptor == -1) {
        return 1;
    }

    selected_frame_dimensions =
        V4l2Device_select_highest_resolution(video_device.file_descriptor);

    /* Allocate frame buffers */
    rgb_frame_buffer_size = (size_t)selected_frame_dimensions.width *
                            selected_frame_dimensions.height * 4;
    rgb_frame_buffer = calloc(1, rgb_frame_buffer_size);
    if (rgb_frame_buffer == NULL) {
        fputs("Failed to allocate RGB frame buffer\n", stderr);
        V4l2Device_close_device(video_device.file_descriptor);
        return 1;
    }

    rgb_flipped_buffer = calloc(1, rgb_frame_buffer_size);
    if (rgb_flipped_buffer == NULL) {
        fputs("Failed to allocate RGB flipped buffer\n", stderr);
        free(rgb_frame_buffer);
        V4l2Device_close_device(video_device.file_descriptor);
        return 1;
    }

    gray_frame_buffer_size = (size_t)selected_frame_dimensions.width *
                             selected_frame_dimensions.height;
    gray_frame_buffer = calloc(1, gray_frame_buffer_size);
    if (gray_frame_buffer == NULL) {
        fputs("Failed to allocate Gray frame buffer\n", stderr);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_close_device(video_device.file_descriptor);
        return 1;
    }

    /* Initialize X11 display and window */
    if (!X11Utils_init(&x11_context, &selected_frame_dimensions,
                       rgb_frame_buffer)) {
        free(gray_frame_buffer);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_close_device(video_device.file_descriptor);
        return 1;
    }

    /* Configure video device */
    if (!V4l2Device_configure_video_format(video_device.file_descriptor,
                                           &selected_frame_dimensions) ||
        !V4l2Device_setup_memory_mapped_buffers(&video_device,
                                                V4L2_MAX_BUFFERS) ||
        !V4l2Device_start_video_stream(video_device.file_descriptor)) {
        X11Utils_cleanup(&x11_context);
        free(gray_frame_buffer);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_close_device(video_device.file_descriptor);
        return 1;
    }

    /* Setup capture thread arguments */
    capture_thread_arguments.device = &video_device;
    capture_thread_arguments.x11_context = &x11_context;
    capture_thread_arguments.rgb_frame_buffer = rgb_frame_buffer;
    capture_thread_arguments.rgb_flipped_buffer = rgb_flipped_buffer;
    capture_thread_arguments.gray_frame_buffer = gray_frame_buffer;
    capture_thread_arguments.frame_dimensions = selected_frame_dimensions;
    capture_thread_arguments.running_flag = &running_flag;

    /* Start capture thread */
    if (pthread_create(&capture_thread_handle, NULL, CaptureThread_function,
                       &capture_thread_arguments) != 0) {
        fputs("Failed to create capture thread\n", stderr);
        X11Utils_cleanup(&x11_context);
        free(gray_frame_buffer);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_stop_video_stream(video_device.file_descriptor);
        V4l2Device_unmap_buffers(&video_device);
        V4l2Device_close_device(video_device.file_descriptor);
        return 1;
    }

    /* Event loop */
    while (atomic_load_explicit(&running_flag, memory_order_relaxed)) {
        XNextEvent(x11_context.display, &received_event);

        if (received_event.type == KeyPress) {
            KeySym pressed_key = XLookupKeysym(&received_event.xkey, 0);
            if (pressed_key == XK_Escape || pressed_key == XK_q) {
                atomic_store_explicit(&running_flag, false,
                                      memory_order_relaxed);
            }
        }

        if (received_event.type == ClientMessage) {
            if ((Atom)received_event.xclient.data.l[0] ==
                x11_context.wm_delete_window_atom) {
                atomic_store_explicit(&running_flag, false,
                                      memory_order_relaxed);
            }
        }
    }

    /* Wait for capture thread to finish */
    pthread_join(capture_thread_handle, NULL);

    /* Cleanup resources */
    X11Utils_cleanup(&x11_context);
    free(gray_frame_buffer);
    free(rgb_frame_buffer);
    free(rgb_flipped_buffer);
    V4l2Device_stop_video_stream(video_device.file_descriptor);
    V4l2Device_unmap_buffers(&video_device);
    V4l2Device_close_device(video_device.file_descriptor);

    return 0;
}
