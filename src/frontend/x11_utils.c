#include "include/x11_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/branch_prediction.h"

#define WINDOW_BORDER_WIDTH 1
#define WINDOW_POSITION_X 10
#define WINDOW_POSITION_Y 10
#define BITMAP_PAD 32

bool X11Utils_init(X11Context *context, const FrameDimensions *frame_dimensions,
                   unsigned char *rgb_frame_buffer) {
    assert(context != NULL && "context cannot be NULL");
    assert(frame_dimensions != NULL && "frame_dimensions cannot be NULL");
    assert(rgb_frame_buffer != NULL && "rgb_frame_buffer cannot be NULL");
    assert(frame_dimensions->width > 0 &&
           "frame_dimensions->width must be greater than 0");
    assert(frame_dimensions->height > 0 &&
           "frame_dimensions->height must be greater than 0");
    context->display = XOpenDisplay(nullptr);
    if (UNLIKELY(context->display == NULL)) {
        (void)fputs("Cannot open X display\n", stderr);
        return false;
    }

    context->screen = DefaultScreen(context->display);

    context->window = XCreateSimpleWindow(
        context->display, RootWindow(context->display, context->screen),
        WINDOW_POSITION_X, WINDOW_POSITION_Y, frame_dimensions->width,
        frame_dimensions->height, WINDOW_BORDER_WIDTH,
        BlackPixel(context->display, context->screen),
        WhitePixel(context->display, context->screen));

    XSelectInput(context->display, context->window,
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(context->display, context->window);

    context->wm_delete_window_atom =
        XInternAtom(context->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(context->display, context->window,
                    &context->wm_delete_window_atom, 1);

    context->x_image = XCreateImage(
        context->display, DefaultVisual(context->display, context->screen),
        (unsigned int)DefaultDepth(context->display, context->screen), ZPixmap,
        0, (char *)rgb_frame_buffer, frame_dimensions->width,
        frame_dimensions->height, BITMAP_PAD, 0);
    if (UNLIKELY(context->x_image == NULL)) {
        (void)fputs("Failed to create XImage\n", stderr);
        XCloseDisplay(context->display);
        return false;
    }
    context->x_image->f.destroy_image =
        False;  // XImage data is managed externally

    return true;
}

void X11Utils_cleanup(X11Context *context) {
    assert(context != NULL && "context cannot be NULL");
    if (LIKELY(context->x_image != NULL)) {
        XDestroyImage(context->x_image);
    }
    if (LIKELY(context->display != NULL)) {
        XCloseDisplay(context->display);
    }
}

void X11Utils_update_window(X11Context *context, unsigned char *frame_data) {
    assert(context != NULL && "context cannot be NULL");
    assert(frame_data != NULL && "frame_data cannot be NULL");
    assert(context->display != NULL && "context->display cannot be NULL");
    assert(context->window != 0 && "context->window cannot be 0");
    assert(context->x_image != NULL && "context->x_image cannot be NULL");
    context->x_image->data = (char *)frame_data;
    XPutImage(context->display, context->window,
              DefaultGC(context->display, context->screen), context->x_image, 0,
              0, 0, 0, (unsigned int)context->x_image->width,
              (unsigned int)context->x_image->height);
    XFlush(context->display);
}
