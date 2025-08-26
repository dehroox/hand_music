#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hand_music.h"

#define MICROSECONDS_IN_SECOND 1000000LL
#define NANOSECONDS_IN_MICROSECOND 1000LL

struct CaptureThreadArgs {
    struct V4l2Device_Device *device;
    Display *display;
    Window window;
    int screen;
    XImage *x_image;
    unsigned char *rgb_frame_buffer;
    unsigned char *gray_frame_buffer;
    struct FrameDimensions frame_dimensions;
    _Atomic bool *running;
};

void *capture_thread_func(void *arg) {
    struct CaptureThreadArgs *args = (struct CaptureThreadArgs *)arg;
    struct V4l2Device_Device *device = args->device;
    Display *display = args->display;
    Window window = args->window;
    int screen = args->screen;
    XImage *x_image = args->x_image;
    unsigned char *rgb_frame_buffer = args->rgb_frame_buffer;
    unsigned char *gray_frame_buffer = args->gray_frame_buffer;
    struct FrameDimensions frame_dimensions = args->frame_dimensions;
    _Atomic bool *running = args->running;

    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    const int width_int = (int)frame_dimensions.width;
    const int height_int = (int)frame_dimensions.height;

    long long total_duration_rgb = 0;
    long long total_duration_gray = 0;
    int frame_count = 0;

    struct timespec start_rgb;
    struct timespec end_rgb;
    struct timespec start_gray;
    struct timespec end_gray;

    while (atomic_load_explicit(running, memory_order_relaxed)) {
        if (continually_retry_ioctl(device->file_descriptor, VIDIOC_DQBUF,
                                    &buffer) == -1) {
            continue;
        }

        unsigned char *yuv_data =
            (unsigned char *)device->mapped_buffers[buffer.index].start_address;

        (void)clock_gettime(1, &start_rgb);
        convert_yuv_to_rgb(yuv_data, rgb_frame_buffer, frame_dimensions);
        (void)clock_gettime(1, &end_rgb);
        total_duration_rgb += (long long)(end_rgb.tv_sec - start_rgb.tv_sec) *
                                  MICROSECONDS_IN_SECOND +
                              (long long)(end_rgb.tv_nsec - start_rgb.tv_nsec) /
                                  NANOSECONDS_IN_MICROSECOND;

        (void)clock_gettime(1, &start_gray);
        convert_yuv_to_gray(yuv_data, gray_frame_buffer, frame_dimensions);
        (void)clock_gettime(1, &end_gray);
        total_duration_gray +=
            (long long)(end_gray.tv_sec - start_gray.tv_sec) *
                MICROSECONDS_IN_SECOND +
            (long long)(end_gray.tv_nsec - start_gray.tv_nsec) /
                NANOSECONDS_IN_MICROSECOND;

        frame_count++;

        XPutImage(display, window, DefaultGC(display, screen), x_image, 0, 0, 0,
                  0, (unsigned int)width_int, (unsigned int)height_int);
        XFlush(display);

        continually_retry_ioctl(device->file_descriptor, VIDIOC_QBUF, &buffer);
    }

    if (frame_count > 0) {
        (void)fprintf(
            stdout, "Average YUV to RGB conversion time: %lld microseconds.\n",
            total_duration_rgb / frame_count);
        (void)fprintf(
            stdout, "Average YUV to Gray conversion time: %lld microseconds.\n",
            total_duration_gray / frame_count);
    }
    return NULL;
}

int main() {
    Display *display = XOpenDisplay(nullptr);
    if (display == NULL) {
        (void)fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    struct V4l2Device_Device device;
    memset(&device, 0, sizeof(device));
    device.file_descriptor = V4l2Device_open("/dev/video0");
    if (device.file_descriptor == -1) {
        XCloseDisplay(display);
        return 1;
    }

    struct FrameDimensions frame_dimensions =
        V4l2Device_select_highest_resolution(device.file_descriptor);

    int screen = DefaultScreen(display);
    const int WINDOW_POS_X = 10;
    const int WINDOW_POS_Y = 10;

    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen), WINDOW_POS_X, WINDOW_POS_Y,
        frame_dimensions.width, frame_dimensions.height, 1,
        BlackPixel(display, screen), WhitePixel(display, screen));

    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);

    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    if (!V4l2Device_configure_video_format(device.file_descriptor,
                                           &frame_dimensions)) {
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    if (!V4l2Device_setup_memory_mapped_buffers(&device, V4L2_MAX_BUFFERS)) {
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    if (!V4l2Device_start_video_stream(device.file_descriptor)) {
        V4l2Device_unmap_buffers(&device);
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    const int frame_bytes =
        (int)frame_dimensions.width * (int)frame_dimensions.height * 4;
    unsigned char *rgb_frame_buffer =
        (unsigned char *)malloc((size_t)frame_bytes);
    if (rgb_frame_buffer == NULL) {
        (void)fprintf(stderr, "Failed to allocate rgb_frame_buffer\n");
        V4l2Device_unmap_buffers(&device);
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }
    memset(rgb_frame_buffer, 0, (size_t)frame_bytes);

    const int gray_frame_bytes =
        (int)frame_dimensions.width * (int)frame_dimensions.height;
    unsigned char *gray_frame_buffer =
        (unsigned char *)malloc((size_t)gray_frame_bytes);
    if (gray_frame_buffer == NULL) {
        (void)fprintf(stderr, "Failed to allocate gray_frame_buffer\n");
        free(rgb_frame_buffer);
        V4l2Device_unmap_buffers(&device);
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }
    memset(gray_frame_buffer, 0, (size_t)gray_frame_bytes);

    const int bitmap_pad = 32;
    XImage *x_image = XCreateImage(display, DefaultVisual(display, screen),
                                   (unsigned int)DefaultDepth(display, screen),
                                   ZPixmap, 0, nullptr, frame_dimensions.width,
                                   frame_dimensions.height, bitmap_pad, 0);
    if (x_image == NULL) {
        (void)fprintf(stderr, "Failed to create XImage\n");
        free(rgb_frame_buffer);
        free(gray_frame_buffer);
        V4l2Device_unmap_buffers(&device);
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }
    (void)fprintf(stdout, "XImage: %p\n", (void *)x_image);
    x_image->data = (char *)rgb_frame_buffer;
    x_image->f.destroy_image = False;

    _Atomic bool running = true;
    pthread_t capture_thread = 0;

    struct CaptureThreadArgs thread_args = {
        .device = &device,
        .display = display,
        .window = window,
        .screen = screen,
        .x_image = x_image,
        .rgb_frame_buffer = rgb_frame_buffer,
        .gray_frame_buffer = gray_frame_buffer,
        .frame_dimensions = frame_dimensions,
        .running = &running};

    if (pthread_create(&capture_thread, nullptr, capture_thread_func,
                       &thread_args) != 0) {
        (void)fprintf(stderr, "Failed to create capture thread\n");
        free(rgb_frame_buffer);
        free(gray_frame_buffer);
        XDestroyImage(x_image);
        V4l2Device_unmap_buffers(&device);
        V4l2Device_close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    XEvent event;
    while (atomic_load_explicit(&running, memory_order_relaxed)) {
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape || key == XK_q) {
                atomic_store_explicit(&running, false, memory_order_relaxed);
            }
        } else if (event.type == ClientMessage) {
            if ((Atom)event.xclient.data.l[0] == wmDeleteMessage) {
                atomic_store_explicit(&running, false, memory_order_relaxed);
            }
        }
    }

    atomic_store_explicit(&running, false, memory_order_relaxed);
    pthread_join(capture_thread, nullptr);

    V4l2Device_stop_video_stream(device.file_descriptor);
    V4l2Device_unmap_buffers(&device);
    V4l2Device_close_device(device.file_descriptor);

    XDestroyImage(x_image);
    free(rgb_frame_buffer);
    free(gray_frame_buffer);
    XCloseDisplay(display);

    return 0;
}
