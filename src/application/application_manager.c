#include "application_manager.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "../capture/include/capture_thread.h"
#include "../capture/include/v4l2_device_api.h"
#include "../common/constants.h"
#include "../frontend/include/frontend_manager.h"

static void application_update_display_callback(void *context_ptr,
                                                unsigned char *frame_data) {
    ApplicationContext *app_context = (ApplicationContext *)context_ptr;
    X11Utils_update_window(&app_context->frontend_context->x11_context,
                           frame_data);
}

bool Application_init(ApplicationContext *context,
                      const char *video_device_path) {
    bool success = false;

    context->video_device = calloc(1, sizeof(struct V4l2Device_Device));
    if (!context->video_device) {
        fputs("Failed to allocate V4L2 device context\n", stderr);
        goto cleanup;
    }

    context->video_device->file_descriptor = V4l2Device_open(video_device_path);
    if (context->video_device->file_descriptor == -1) {
        fputs("Failed to open video device\n", stderr);
        goto cleanup;
    }

    context->frame_dimensions = V4l2Device_select_highest_resolution(
        context->video_device->file_descriptor);
    if (context->frame_dimensions.width == 0 ||
        context->frame_dimensions.height == 0) {
        fputs("Failed to select highest resolution.\n", stderr);
        goto cleanup;
    }

    if (!V4l2Device_configure_video_format(
            context->video_device->file_descriptor,
            &context->frame_dimensions) ||
        !V4l2Device_setup_memory_mapped_buffers(context->video_device,
                                                V4L2_MAX_BUFFERS) ||
        !V4l2Device_start_video_stream(
            context->video_device->file_descriptor)) {
        fputs("Failed to configure video stream or buffers.\n", stderr);
        goto cleanup;
    }

    size_t rgb_buffer_size = (size_t)context->frame_dimensions.width *
                             context->frame_dimensions.height * RGB_CHANNELS;
    context->rgb_frame_buffer = calloc(1, rgb_buffer_size);
    context->rgb_flipped_buffer = calloc(1, rgb_buffer_size);
    context->gray_frame_buffer =
        calloc(1, (size_t)context->frame_dimensions.width *
                      context->frame_dimensions.height);

    if (!context->rgb_frame_buffer || !context->rgb_flipped_buffer ||
        !context->gray_frame_buffer) {
        fputs("Failed to allocate frame buffers.\n", stderr);
        goto cleanup;
    }

    context->frontend_context = calloc(1, sizeof(FrontendContext));
    if (!context->frontend_context) {
        fputs("Failed to allocate frontend context\n", stderr);
        goto cleanup;
    }

    if (!Frontend_init(context->frontend_context, &context->frame_dimensions,
                       context->rgb_flipped_buffer)) {
        fputs("Failed to initialize frontend.\n", stderr);
        goto cleanup;
    }

    context->running_flag = calloc(1, sizeof(atomic_bool));
    if (!context->running_flag) {
        fputs("Failed to allocate running flag\n", stderr);
        goto cleanup;
    }
    atomic_init(context->running_flag, true);

    success = true;

cleanup:
    if (!success) {
        Application_cleanup(context);
    }
    return success;
}

void Application_run(ApplicationContext *context) {
    pthread_t capture_thread_handle;
    CaptureThreadArguments capture_thread_arguments = {
        .device = context->video_device,
        .rgb_frame_buffer = context->rgb_frame_buffer,
        .rgb_flipped_buffer = context->rgb_flipped_buffer,
        .gray_frame_buffer = context->gray_frame_buffer,
        .frame_dimensions = context->frame_dimensions,
        .running_flag = context->running_flag,
        .display_update_callback = application_update_display_callback,
        .display_update_context = context};

    if (pthread_create(&capture_thread_handle, NULL, CaptureThread_function,
                       &capture_thread_arguments) != 0) {
        fputs("Failed to create capture thread\n", stderr);
        return;
    }

    Frontend_run(context->frontend_context, context->running_flag);

    pthread_join(capture_thread_handle, NULL);
}

void Application_cleanup(ApplicationContext *context) {
    if (context->video_device) {
        V4l2Device_stop_video_stream(context->video_device->file_descriptor);
        V4l2Device_unmap_buffers(context->video_device);
        V4l2Device_close_device(context->video_device->file_descriptor);
        free(context->video_device);
        context->video_device = NULL;
    }
    if (context->frontend_context) {
        Frontend_cleanup(context->frontend_context);
        free(context->frontend_context);
        context->frontend_context = NULL;
    }
    if (context->rgb_frame_buffer) {
        free(context->rgb_frame_buffer);
        context->rgb_frame_buffer = NULL;
    }
    if (context->rgb_flipped_buffer) {
        free(context->rgb_flipped_buffer);
        context->rgb_flipped_buffer = NULL;
    }
    if (context->gray_frame_buffer) {
        free(context->gray_frame_buffer);
        context->gray_frame_buffer = NULL;
    }
    if (context->running_flag) {
        free(context->running_flag);
        context->running_flag = NULL;
    }
}
