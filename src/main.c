#include <assert.h>
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
#include "image/include/v4l2_device_api.h"

typedef struct {
    V4l2DeviceContext *video_device;
    FrontendContext *frontend_context;
    FrameDimensions *frame_dimensions;
    _Atomic bool *running_flag;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    _Atomic bool *gray_view;
} __attribute__((aligned(64))) ApplicationContext;

static inline void application_update_display_callback(
    void *context_ptr, unsigned char *frame_data) {
    assert(context_ptr != NULL && "context_ptr cannot be NULL");
    assert(frame_data != NULL && "frame_data cannot be NULL");
    const ApplicationContext *app_context = (ApplicationContext *)context_ptr;
    assert(app_context->frontend_context != NULL &&
           "frontend_context cannot be NULL");
    X11Utils_update_window(&app_context->frontend_context->x11_context,
                           frame_data);
}

static void cleanup_application_resources(ApplicationContext *context) {
    assert(context != NULL && "context cannot be NULL");
    if (LIKELY(context->video_device)) {
        V4l2Device_stop_video_stream(context->video_device->file_descriptor);
        V4l2Device_unmap_buffers(context->video_device);
        V4l2Device_close(context->video_device->file_descriptor);
        free(context->video_device);
        context->video_device = nullptr;
    }
    if (LIKELY(context->frontend_context)) {
        Frontend_cleanup(context->frontend_context);
        free(context->frontend_context);
        context->frontend_context = nullptr;
    }
    if (LIKELY(context->rgb_frame_buffer)) {
        free(context->rgb_frame_buffer);
        context->rgb_frame_buffer = nullptr;
    }
    if (LIKELY(context->rgb_flipped_buffer)) {
        free(context->rgb_flipped_buffer);
        context->rgb_flipped_buffer = nullptr;
    }
    if (LIKELY(context->gray_frame_buffer)) {
        free(context->gray_frame_buffer);
        context->gray_frame_buffer = nullptr;
    }
    if (LIKELY(context->frame_dimensions)) {
        free((void *)context->frame_dimensions);
        context->frame_dimensions = nullptr;
    }
    if (LIKELY(context->running_flag)) {
        free(context->running_flag);
        context->running_flag = nullptr;
    }
    if (LIKELY(context->gray_view)) {
        free(context->gray_view);
        context->gray_view = nullptr;
    }
}

static inline bool initialize_gray_view(ApplicationContext *app_context,
                                        int argc) {
    app_context->gray_view = calloc(1, sizeof(atomic_bool));

    if (UNLIKELY(!app_context->gray_view)) {
        (void)fputs("Failed to allocate gray_view flag\n", stderr);
        return false;
    }
    assert(app_context->gray_view != NULL && "gray_view allocation failed");

    atomic_init(app_context->gray_view, (bool)(argc > 2));
    return true;
}

static inline bool initialize_video_device(ApplicationContext *app_context) {
    app_context->video_device = calloc(1, sizeof(V4l2DeviceContext));
    if (UNLIKELY(!app_context->video_device)) {
        (void)fputs("Failed to allocate V4L2 device context\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    assert(app_context->video_device != NULL &&
           "video_device allocation failed");

    app_context->video_device->file_descriptor =
        V4l2Device_open(VIDEO_DEVICE_PATH);

    if (UNLIKELY(app_context->video_device->file_descriptor ==
                 INVALID_FILE_DESCRIPTOR)) {
        (void)fputs("Failed to open video device\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }

    app_context->frame_dimensions = calloc(1, sizeof(FrameDimensions));
    if (UNLIKELY(!app_context->frame_dimensions)) {
        (void)fputs("Failed to allocate frame dimensions\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }

    assert(app_context->frame_dimensions != NULL &&
           "frame_dimensions allocation failed");

    V4l2Device_select_highest_resolution(
        app_context->video_device->file_descriptor,
        app_context->frame_dimensions);
    if (UNLIKELY(app_context->frame_dimensions->width == 0 ||
                 app_context->frame_dimensions->height == 0)) {
        (void)fputs("Failed to select highest resolution.\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    return true;
}

static inline bool configure_video_stream(ApplicationContext *app_context) {
    if (UNLIKELY(!V4l2Device_configure_video_format(
                     app_context->video_device->file_descriptor,
                     app_context->frame_dimensions) ||
                 !V4l2Device_setup_memory_mapped_buffers(
                     app_context->video_device, V4L2_MAX_BUFFERS) ||
                 !V4l2Device_start_video_stream(
                     app_context->video_device->file_descriptor))) {
        (void)fputs("Failed to configure video stream or buffers.\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    return true;
}

static inline bool initialize_frame_buffers(ApplicationContext *app_context) {
    const size_t rgb_buffer_size =
        (size_t)app_context->frame_dimensions->width *
        app_context->frame_dimensions->height * RGB_CHANNELS;
    app_context->rgb_frame_buffer = calloc(1, rgb_buffer_size);
    app_context->rgb_flipped_buffer = calloc(1, rgb_buffer_size);

    app_context->gray_frame_buffer =
        calloc(1, (size_t)app_context->frame_dimensions->width *
                      app_context->frame_dimensions->height);
    if (UNLIKELY(!app_context->rgb_frame_buffer ||
                 !app_context->rgb_flipped_buffer ||
                 !app_context->gray_frame_buffer)) {
        (void)fputs("Failed to allocate frame buffers.\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    assert(app_context->rgb_frame_buffer != NULL &&
           "rgb_frame_buffer allocation failed");
    assert(app_context->rgb_flipped_buffer != NULL &&
           "rgb_flipped_buffer allocation failed");
    assert(app_context->gray_frame_buffer != NULL &&
           "gray_frame_buffer allocation failed");
    return true;
}

static inline bool initialize_frontend(ApplicationContext *app_context) {
    app_context->frontend_context = calloc(1, sizeof(FrontendContext));
    if (UNLIKELY(!app_context->frontend_context)) {
        (void)fputs("Failed to allocate frontend context\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    assert(app_context->frontend_context != NULL &&
           "frontend_context allocation failed");

    if (UNLIKELY(!Frontend_init(app_context->frontend_context,
                                app_context->frame_dimensions,
                                app_context->rgb_flipped_buffer))) {
        (void)fputs("Failed to initialize frontend.\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    return true;
}

static inline bool initialize_running_flag(ApplicationContext *app_context) {
    app_context->running_flag = calloc(1, sizeof(atomic_bool));
    if (UNLIKELY(!app_context->running_flag)) {
        (void)fputs("Failed to allocate running flag\n", stderr);
        cleanup_application_resources(app_context);
        return false;
    }
    assert(app_context->running_flag != NULL &&
           "running_flag allocation failed");
    atomic_init(app_context->running_flag, true);
    return true;
}

static inline bool initialize_application_context(
    ApplicationContext *app_context, int argc) {
    if (!initialize_gray_view(app_context, argc)) {
        return false;
    }
    if (!initialize_video_device(app_context)) {
        return false;
    }
    if (!configure_video_stream(app_context)) {
        return false;
    }
    if (!initialize_frame_buffers(app_context)) {
        return false;
    }
    if (!initialize_frontend(app_context)) {
        return false;
    }
    if (!initialize_running_flag(app_context)) {
        return false;
    }
    return true;
}

static inline bool run_application_loop(ApplicationContext *app_context) {
    pthread_t capture_thread_handle;
    CaptureThreadArguments capture_thread_arguments = {
        .device = app_context->video_device,
        .rgb_frame_buffer = app_context->rgb_frame_buffer,
        .rgb_flipped_buffer = app_context->rgb_flipped_buffer,
        .gray_frame_buffer = app_context->gray_frame_buffer,
        .frame_dimensions = app_context->frame_dimensions,
        .running_flag = app_context->running_flag,
        .gray_view = app_context->gray_view,
        .display_update_callback = application_update_display_callback,
        .display_update_context = app_context};

    if (UNLIKELY(pthread_create(&capture_thread_handle, nullptr,
                                CaptureThread_run,
                                &capture_thread_arguments) != 0)) {
        (void)fputs("Failed to create capture thread\n", stderr);
        return false;
    }

    Frontend_run(app_context->frontend_context, app_context->running_flag);

    if (UNLIKELY(pthread_join(capture_thread_handle, nullptr) != 0)) {
        (void)fputs("Failed to join capture thread\n", stderr);
        return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    (void)argv;
    ApplicationContext app_context;
    if (!initialize_application_context(&app_context, argc)) {
        return EXIT_FAILURE;
    }

    if (!run_application_loop(&app_context)) {
        cleanup_application_resources(&app_context);
        return EXIT_FAILURE;
    }

    cleanup_application_resources(&app_context);

    return EXIT_SUCCESS;
}
