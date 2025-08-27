#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/branch_prediction.h"
#include "common/constants.h"
#include "frontend/include/frontend_manager.h"
#include "frontend/include/x11_utils.h"
#include "image/include/capture_thread.h"
#include "image/include/image_conversions.h"
#include "image/include/image_processing.h"
#include "image/include/v4l2_device_api.h"

typedef struct {
    V4l2DeviceContext *video_device;
    FrontendContext *frontend_context;
    FrameDimensions frame_dimensions;
    _Atomic bool *running_flag;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    unsigned char *gray_rgba_buffer;
    _Atomic bool *gray_view;
} ApplicationContext;

static inline void application_update_display_callback(
    void *context_ptr, unsigned char *frame_data) {
    ApplicationContext *app_context = (ApplicationContext *)context_ptr;
    X11Utils_update_window(&app_context->frontend_context->x11_context,
                           frame_data);
}

static inline void cleanup_application_resources(ApplicationContext *context) {
    if (LIKELY(context->video_device)) {
        V4l2Device_stop_video_stream(context->video_device->file_descriptor);
        V4l2Device_unmap_buffers(context->video_device);
        V4l2Device_close(context->video_device->file_descriptor);
        free(context->video_device);
        context->video_device = NULL;
    }
    if (LIKELY(context->frontend_context)) {
        Frontend_cleanup(context->frontend_context);
        free(context->frontend_context);
        context->frontend_context = NULL;
    }
    if (LIKELY(context->rgb_frame_buffer)) {
        free(context->rgb_frame_buffer);
        context->rgb_frame_buffer = NULL;
    }
    if (LIKELY(context->rgb_flipped_buffer)) {
        free(context->rgb_flipped_buffer);
        context->rgb_flipped_buffer = NULL;
    }
    if (LIKELY(context->gray_frame_buffer)) {
        free(context->gray_frame_buffer);
        context->gray_frame_buffer = NULL;
    }
    if (LIKELY(context->gray_rgba_buffer)) {
        free(context->gray_rgba_buffer);
        context->gray_rgba_buffer = NULL;
    }
    if (LIKELY(context->running_flag)) {
        free(context->running_flag);
        context->running_flag = NULL;
    }
    if (LIKELY(context->gray_view)) {
        free(context->gray_view);
        context->gray_view = NULL;
    }
}

int main(int argc, char *argv[]) {
    (void)argv;
    ApplicationContext app_context = {0};
    app_context.gray_view = calloc(1, sizeof(atomic_bool));

    if (UNLIKELY(!app_context.gray_view)) {
        fputs("Failed to allocate gray_view flag\n", stderr);
        return EXIT_FAILURE;
    }

    atomic_init(app_context.gray_view, (bool)(argc > 1));

    bool success = false;

    app_context.video_device = calloc(1, sizeof(V4l2DeviceContext));
    if (UNLIKELY(!app_context.video_device)) {
        fputs("Failed to allocate V4L2 device context\n", stderr);
        goto cleanup;
    }

    app_context.video_device->file_descriptor =
        V4l2Device_open(VIDEO_DEVICE_PATH);
    if (UNLIKELY(app_context.video_device->file_descriptor == -1)) {
        fputs("Failed to open video device\n", stderr);
        goto cleanup;
    }

    app_context.frame_dimensions = V4l2Device_select_highest_resolution(
        app_context.video_device->file_descriptor);
    if (UNLIKELY(app_context.frame_dimensions.width == 0 ||
                 app_context.frame_dimensions.height == 0)) {
        fputs("Failed to select highest resolution.\n", stderr);
        goto cleanup;
    }

    if (UNLIKELY(!V4l2Device_configure_video_format(
                     app_context.video_device->file_descriptor,
                     &app_context.frame_dimensions) ||
                 !V4l2Device_setup_memory_mapped_buffers(
                     app_context.video_device, V4L2_MAX_BUFFERS) ||
                 !V4l2Device_start_video_stream(
                     app_context.video_device->file_descriptor))) {
        fputs("Failed to configure video stream or buffers.\n", stderr);
        goto cleanup;
    }

    size_t rgb_buffer_size = (size_t)app_context.frame_dimensions.width *
                             app_context.frame_dimensions.height * RGB_CHANNELS;
    app_context.rgb_frame_buffer = calloc(1, rgb_buffer_size);
    app_context.rgb_flipped_buffer = calloc(1, rgb_buffer_size);

    app_context.gray_frame_buffer =
        calloc(1, (size_t)app_context.frame_dimensions.width *
                      app_context.frame_dimensions.height);
    app_context.gray_rgba_buffer = calloc(1, rgb_buffer_size);

    if (UNLIKELY(
            !app_context.rgb_frame_buffer || !app_context.rgb_flipped_buffer ||
            !app_context.gray_frame_buffer || !app_context.gray_rgba_buffer)) {
        fputs("Failed to allocate frame buffers.\n", stderr);
        goto cleanup;
    }

    app_context.frontend_context = calloc(1, sizeof(FrontendContext));
    if (UNLIKELY(!app_context.frontend_context)) {
        fputs("Failed to allocate frontend context\n", stderr);
        goto cleanup;
    }

    if (UNLIKELY(!Frontend_init(app_context.frontend_context,
                                &app_context.frame_dimensions,
                                app_context.rgb_flipped_buffer))) {
        fputs("Failed to initialize frontend.\n", stderr);
        goto cleanup;
    }

    app_context.running_flag = calloc(1, sizeof(atomic_bool));
    if (UNLIKELY(!app_context.running_flag)) {
        fputs("Failed to allocate running flag\n", stderr);
        goto cleanup;
    }
    atomic_init(app_context.running_flag, true);

    success = true;

cleanup:
    if (UNLIKELY(!success)) {
        cleanup_application_resources(&app_context);
        return EXIT_FAILURE;
    }

    pthread_t capture_thread_handle;
    CaptureThreadArguments capture_thread_arguments = {
        .device = app_context.video_device,
        .rgb_frame_buffer = app_context.rgb_frame_buffer,
        .rgb_flipped_buffer = app_context.rgb_flipped_buffer,
        .gray_frame_buffer = app_context.gray_frame_buffer,
        .gray_rgba_buffer = app_context.gray_rgba_buffer,
        .frame_dimensions = app_context.frame_dimensions,
        .running_flag = app_context.running_flag,
        .gray_view = app_context.gray_view,
        .display_update_callback = application_update_display_callback,
        .display_update_context = &app_context,
        .convert_yuv_to_rgb = ImageConversions_convert_yuv_to_rgb,
        .convert_yuv_to_gray = ImageConversions_convert_yuv_to_gray,
        .expand_grayscale = ImageProcessing_expand_grayscale,
        .flip_rgb_horizontal = ImageProcessing_flip_rgb_horizontal};

    if (UNLIKELY(pthread_create(&capture_thread_handle, NULL, CaptureThread_run,
                                &capture_thread_arguments) != 0)) {
        fputs("Failed to create capture thread\n", stderr);
        cleanup_application_resources(
            &app_context);  // Cleanup if thread creation fails
        return EXIT_FAILURE;
    }

    Frontend_run(app_context.frontend_context, app_context.running_flag);

    pthread_join(capture_thread_handle, NULL);

    cleanup_application_resources(&app_context);

    return EXIT_SUCCESS;
}