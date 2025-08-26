#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hand_music.h"

#define MICROSECONDS_IN_SECOND 1000000LL
#define NANOSECONDS_IN_MICROSECOND 1000LL
#define WINDOW_BORDER_WIDTH 1
#define WINDOW_POSITION_X 10
#define WINDOW_POSITION_Y 10
#define BITMAP_PAD 32
#define VIDEO_DEVICE_PATH "/dev/video0"
#define VIDEO_MAX_BUFFERS V4L2_MAX_BUFFERS

typedef struct {
    struct V4l2Device_Device *device;
    Display *display;
    Window window;
    int screen;
    XImage *x_image;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    struct FrameDimensions frame_dimensions;
    _Atomic bool *running_flag;
} CaptureThreadArguments;

static inline void flip_rgb_horizontal(
    const unsigned char *source_rgb_buffer,
    unsigned char *destination_rgb_buffer,
    struct FrameDimensions frame_dimensions) {
    const unsigned int number_of_bytes_per_pixel_in_rgba_format = 4;
    const size_t number_of_bytes_per_row_of_pixels =
        (size_t)frame_dimensions.width *
        number_of_bytes_per_pixel_in_rgba_format;

    for (unsigned int current_row_index = 0;
         current_row_index < frame_dimensions.height; ++current_row_index) {
        const unsigned char *source_row_pointer_for_current_row =
            source_rgb_buffer +
            ((size_t)current_row_index * number_of_bytes_per_row_of_pixels);

        unsigned char *destination_row_pointer_for_current_row_flipped =
            destination_rgb_buffer +
            ((size_t)current_row_index * number_of_bytes_per_row_of_pixels);

        for (unsigned int current_column_index = 0;
             current_column_index < frame_dimensions.width;
             ++current_column_index) {
            const unsigned int source_pixel_column_index = current_column_index;
            const unsigned int destination_pixel_column_index_flipped =
                frame_dimensions.width - 1 - current_column_index;

            const unsigned char *source_pixel_pointer =
                source_row_pointer_for_current_row +
                ((size_t)source_pixel_column_index *
                 number_of_bytes_per_pixel_in_rgba_format);

            unsigned char *destination_pixel_pointer_flipped =
                destination_row_pointer_for_current_row_flipped +
                ((size_t)destination_pixel_column_index_flipped *
                 number_of_bytes_per_pixel_in_rgba_format);

            destination_pixel_pointer_flipped[0] = source_pixel_pointer[0];
            destination_pixel_pointer_flipped[1] = source_pixel_pointer[1];
            destination_pixel_pointer_flipped[2] = source_pixel_pointer[2];
            destination_pixel_pointer_flipped[3] = source_pixel_pointer[3];
        }
    }
}

inline static long long measure_frame_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         struct FrameDimensions),
    const unsigned char *source_frame, unsigned char *destination_frame,
    struct FrameDimensions frame_dimensions) {
    struct timespec start_time;
    struct timespec end_time;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    convert_func(source_frame, destination_frame, frame_dimensions);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    return ((end_time.tv_sec - start_time.tv_sec) * MICROSECONDS_IN_SECOND) +
           ((end_time.tv_nsec - start_time.tv_nsec) /
            NANOSECONDS_IN_MICROSECOND);
}

inline static void *capture_thread_function(void *arguments) {
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
            measure_frame_conversion_time(convert_yuv_to_rgb, yuv_frame_data,
                                          thread_arguments->rgb_frame_buffer,
                                          thread_arguments->frame_dimensions);
        total_gray_conversion_time_microseconds +=
            measure_frame_conversion_time(convert_yuv_to_gray, yuv_frame_data,
                                          thread_arguments->gray_frame_buffer,
                                          thread_arguments->frame_dimensions);

        captured_frame_count++;

        flip_rgb_horizontal(thread_arguments->rgb_frame_buffer,
                            thread_arguments->rgb_flipped_buffer,
                            thread_arguments->frame_dimensions);

        thread_arguments->x_image->data =
            (char *)thread_arguments->rgb_flipped_buffer;

        XPutImage(
            thread_arguments->display, thread_arguments->window,
            DefaultGC(thread_arguments->display, thread_arguments->screen),
            thread_arguments->x_image, 0, 0, 0, 0,
            thread_arguments->frame_dimensions.width,
            thread_arguments->frame_dimensions.height);
        XFlush(thread_arguments->display);

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

int main(void) {
    Display *x_display;
    struct V4l2Device_Device video_device;
    struct FrameDimensions selected_frame_dimensions;
    int default_screen_number;
    Window video_window;
    Atom wm_delete_window_atom;
    XImage *x_image_for_display;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    size_t rgb_frame_buffer_size;
    size_t gray_frame_buffer_size;
    _Atomic bool running_flag;
    pthread_t capture_thread_handle;
    CaptureThreadArguments capture_thread_arguments;
    XEvent received_event;

    /* Initialization */
    x_display = NULL;
    x_image_for_display = NULL;
    rgb_frame_buffer = NULL;
    gray_frame_buffer = NULL;
    running_flag = true;
    memset(&video_device, 0, sizeof(video_device));
    memset(&selected_frame_dimensions, 0, sizeof(selected_frame_dimensions));

    /* Open X11 display */
    x_display = XOpenDisplay(NULL);
    if (x_display == NULL) {
        fputs("Cannot open X display\n", stderr);
        return 1;
    }

    /* Open video device */
    video_device.file_descriptor = V4l2Device_open(VIDEO_DEVICE_PATH);
    if (video_device.file_descriptor == -1) {
        XCloseDisplay(x_display);
        return 1;
    }

    selected_frame_dimensions =
        V4l2Device_select_highest_resolution(video_device.file_descriptor);
    default_screen_number = DefaultScreen(x_display);

    /* Create X11 window */
    video_window = XCreateSimpleWindow(
        x_display, RootWindow(x_display, default_screen_number),
        WINDOW_POSITION_X, WINDOW_POSITION_Y, selected_frame_dimensions.width,
        selected_frame_dimensions.height, WINDOW_BORDER_WIDTH,
        BlackPixel(x_display, default_screen_number),
        WhitePixel(x_display, default_screen_number));

    XSelectInput(x_display, video_window,
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(x_display, video_window);

    wm_delete_window_atom = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x_display, video_window, &wm_delete_window_atom, 1);

    /* Configure video device */
    if (!V4l2Device_configure_video_format(video_device.file_descriptor,
                                           &selected_frame_dimensions) ||
        !V4l2Device_setup_memory_mapped_buffers(&video_device,
                                                VIDEO_MAX_BUFFERS) ||
        !V4l2Device_start_video_stream(video_device.file_descriptor)) {
        V4l2Device_close_device(video_device.file_descriptor);
        XCloseDisplay(x_display);
        return 1;
    }

    /* Allocate frame buffers */
    rgb_frame_buffer_size = (size_t)selected_frame_dimensions.width *
                            selected_frame_dimensions.height * 4;
    rgb_frame_buffer = calloc(1, rgb_frame_buffer_size);
    if (rgb_frame_buffer == NULL) {
        fputs("Failed to allocate RGB frame buffer\n", stderr);
        V4l2Device_stop_video_stream(video_device.file_descriptor);
        V4l2Device_unmap_buffers(&video_device);
        V4l2Device_close_device(video_device.file_descriptor);
        XCloseDisplay(x_display);
        return 1;
    }

    rgb_flipped_buffer = calloc(1, rgb_frame_buffer_size);
    if (rgb_flipped_buffer == NULL) {
        fputs("Failed to allocate RGB flipped buffer\n", stderr);
        free(gray_frame_buffer);
        free(rgb_frame_buffer);
        V4l2Device_stop_video_stream(video_device.file_descriptor);
        V4l2Device_unmap_buffers(&video_device);
        V4l2Device_close_device(video_device.file_descriptor);
        XCloseDisplay(x_display);
        return 1;
    }

    gray_frame_buffer_size = (size_t)selected_frame_dimensions.width *
                             selected_frame_dimensions.height;
    gray_frame_buffer = calloc(1, gray_frame_buffer_size);
    if (gray_frame_buffer == NULL) {
        fputs("Failed to allocate Gray frame buffer\n", stderr);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_stop_video_stream(video_device.file_descriptor);
        V4l2Device_unmap_buffers(&video_device);
        V4l2Device_close_device(video_device.file_descriptor);
        XCloseDisplay(x_display);
        return 1;
    }

    /* Create XImage for displaying RGB frames */
    x_image_for_display =
        XCreateImage(x_display, DefaultVisual(x_display, default_screen_number),
                     DefaultDepth(x_display, default_screen_number), ZPixmap, 0,
                     (char *)rgb_frame_buffer, selected_frame_dimensions.width,
                     selected_frame_dimensions.height, BITMAP_PAD, 0);
    if (x_image_for_display == NULL) {
        fputs("Failed to create XImage\n", stderr);
        free(gray_frame_buffer);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_stop_video_stream(video_device.file_descriptor);
        V4l2Device_unmap_buffers(&video_device);
        V4l2Device_close_device(video_device.file_descriptor);
        XCloseDisplay(x_display);
        return 1;
    }
    x_image_for_display->f.destroy_image = False;

    /* Setup capture thread arguments */
    capture_thread_arguments.device = &video_device;
    capture_thread_arguments.display = x_display;
    capture_thread_arguments.window = video_window;
    capture_thread_arguments.screen = default_screen_number;
    capture_thread_arguments.x_image = x_image_for_display;
    capture_thread_arguments.rgb_frame_buffer = rgb_frame_buffer;
    capture_thread_arguments.rgb_flipped_buffer = rgb_flipped_buffer;
    capture_thread_arguments.gray_frame_buffer = gray_frame_buffer;
    capture_thread_arguments.frame_dimensions = selected_frame_dimensions;
    capture_thread_arguments.running_flag = &running_flag;

    /* Start capture thread */
    if (pthread_create(&capture_thread_handle, NULL, capture_thread_function,
                       &capture_thread_arguments) != 0) {
        fputs("Failed to create capture thread\n", stderr);
        XDestroyImage(x_image_for_display);
        free(gray_frame_buffer);
        free(rgb_frame_buffer);
        free(rgb_flipped_buffer);
        V4l2Device_stop_video_stream(video_device.file_descriptor);
        V4l2Device_unmap_buffers(&video_device);
        V4l2Device_close_device(video_device.file_descriptor);
        XCloseDisplay(x_display);
        return 1;
    }

    /* Event loop */
    while (atomic_load_explicit(&running_flag, memory_order_relaxed)) {
        XNextEvent(x_display, &received_event);

        if (received_event.type == KeyPress) {
            KeySym pressed_key = XLookupKeysym(&received_event.xkey, 0);
            if (pressed_key == XK_Escape || pressed_key == XK_q) {
                atomic_store_explicit(&running_flag, false,
                                      memory_order_relaxed);
            }
        }

        if (received_event.type == ClientMessage) {
            if ((Atom)received_event.xclient.data.l[0] ==
                wm_delete_window_atom) {
                atomic_store_explicit(&running_flag, false,
                                      memory_order_relaxed);
            }
        }
    }

    /* Wait for capture thread to finish */
    pthread_join(capture_thread_handle, NULL);

    /* Cleanup resources */
    XDestroyImage(x_image_for_display);
    free(gray_frame_buffer);
    free(rgb_frame_buffer);
    free(rgb_flipped_buffer);
    V4l2Device_stop_video_stream(video_device.file_descriptor);
    V4l2Device_unmap_buffers(&video_device);
    V4l2Device_close_device(video_device.file_descriptor);
    XCloseDisplay(x_display);

    return 0;
}
