#include "window.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

struct BackendInternal {
    Display *display;
    Window window;
    GC gc;
    XImage *image;
} __attribute__((aligned(32)));

WindowState *Window_create(const char *title, FrameDimensions dimensions) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        return NULL;
    }

    int screen = DefaultScreen(display);
    Window xWindow = XCreateSimpleWindow(
        display, RootWindow(display, screen), 0, 0, dimensions.width,
        dimensions.height, 1, BlackPixel(display, screen),
        WhitePixel(display, screen));
    XStoreName(display, xWindow, title);
    XSelectInput(display, xWindow,
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, xWindow);

    GC internal_gc = XCreateGC(display, xWindow, 0, NULL);

    XImage *image = XCreateImage(
        display, DefaultVisual(display, screen), 24, ZPixmap, 0,
        malloc((unsigned long)dimensions.width * dimensions.height * 4),
        dimensions.width, dimensions.height, 32, 0);
    if (!image) {
        XCloseDisplay(display);
        return NULL;
    }

    BackendInternal *internal = malloc(sizeof(BackendInternal));
    internal->display = display;
    internal->window = xWindow;
    internal->gc = internal_gc;
    internal->image = image;

    WindowState *state = malloc(sizeof(Window));
    state->internal = internal;
    state->dimensions = dimensions;
    return state;
}

void Window_draw(WindowState *state, const unsigned char *buffer) {
    BackendInternal *backend = state->internal;
    memcpy(
        backend->image->data, buffer,
        (unsigned long)state->dimensions.width * state->dimensions.height * 4);
    XPutImage(backend->display, backend->window, backend->gc, backend->image, 0,
              0, 0, 0, state->dimensions.width, state->dimensions.height);
    XFlush(backend->display);
}

bool Window_pollEvents(WindowState *state) {
    BackendInternal *backend = state->internal;
    while (XPending(backend->display)) {
        XEvent event;
        XNextEvent(backend->display, &event);
        if (event.type == DestroyNotify) {
            return true;
        }
    }
    return false;
}

void Window_destroy(WindowState *state) {
    BackendInternal *backend = state->internal;
    XDestroyImage(backend->image);
    XFreeGC(backend->display, backend->gc);
    XDestroyWindow(backend->display, backend->window);
    XCloseDisplay(backend->display);
    free(backend);
    free(state);
}
