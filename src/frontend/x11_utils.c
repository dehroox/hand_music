#include "include/x11_utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "../common/constants.h"

bool X11Utils_init(X11Context *context, const FrameDimensions *frame_dimensions,
                   unsigned char *rgb_frame_buffer) {
    context->display = XOpenDisplay(NULL);
    if (context->display == NULL) {
        fputs("Cannot open X display\n", stderr);
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
    if (context->x_image == NULL) {
        fputs("Failed to create XImage\n", stderr);
        XCloseDisplay(context->display);
        return false;
    }
    context->x_image->f.destroy_image =
        False;  // XImage data is managed externally

    return true;
}

void X11Utils_cleanup(X11Context *context) {
    if (context->x_image != NULL) {
        XDestroyImage(context->x_image);
    }
    if (context->display != NULL) {
        XCloseDisplay(context->display);
    }
}

void X11Utils_update_window(X11Context *context, unsigned char *frame_data) {
    context->x_image->data = (char *)frame_data;
    XPutImage(context->display, context->window,
              DefaultGC(context->display, context->screen), context->x_image, 0,
              0, 0, 0, (unsigned int)context->x_image->width,
              (unsigned int)context->x_image->height);
    XFlush(context->display);
}
