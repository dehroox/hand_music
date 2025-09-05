/*
    X11 (for now) window creation api, with an opaque exposed api, see
   `window.h`
*/

#include "window.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "types.h"

struct BackendInternal {
    Display *display;
    Window window;
    GC gc;
    XImage *image;
} __attribute__((aligned(32)));

ErrorCode Window_create(WindowState *state, const char *title,
			FrameDimensions dimensions) {
    state->internal = NULL;
    state->dimensions = dimensions;

    Display *display = XOpenDisplay(NULL);
    if (UNLIKELY(!display)) {
	return ERROR_DISPLAY_OPEN_FAILED;
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
	(char *)malloc((size_t)dimensions.width * dimensions.height * 4),
	dimensions.width, dimensions.height, 32, 0);
    if (UNLIKELY(!image)) {
	XFreeGC(display, internal_gc);
	XDestroyWindow(display, xWindow);
	XCloseDisplay(display);
	return ERROR_ALLOCATION_FAILED;
    }

    BackendInternal *internal = malloc(sizeof(struct BackendInternal));
    if (UNLIKELY(!internal)) {
	free(image->data);
	XDestroyImage(image);
	XFreeGC(display, internal_gc);
	XDestroyWindow(display, xWindow);
	XCloseDisplay(display);
	return ERROR_ALLOCATION_FAILED;
    }
    internal->display = display;
    internal->window = xWindow;
    internal->gc = internal_gc;
    internal->image = image;

    state->internal = internal;
    return ERROR_NONE;
}

void Window_draw(WindowState *state, const unsigned char *buffer) {
    BackendInternal *backend = state->internal;
    memcpy(backend->image->data, buffer,
	   (size_t)state->dimensions.width * state->dimensions.height * 4);
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
    if (LIKELY(state && state->internal)) {
	BackendInternal *backend = state->internal;
	XDestroyImage(backend->image);
	XFreeGC(backend->display, backend->gc);
	XDestroyWindow(backend->display, backend->window);
	XCloseDisplay(backend->display);
	free(backend);
	state->internal = NULL;
    }
}
