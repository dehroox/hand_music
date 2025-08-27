#ifndef X11_UTILS_H
#define X11_UTILS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "../../common/common_types.h"

typedef struct {
    Display *display;
    Window window;
    int screen;
    XImage *x_image;
    Atom wm_delete_window_atom;
} X11Context;

bool X11Utils_init(X11Context *context, const FrameDimensions *frame_dimensions,
                   unsigned char *rgb_frame_buffer);

void X11Utils_cleanup(X11Context *context);

void X11Utils_update_window(X11Context *context, unsigned char *frame_data);

#endif  // X11_UTILS_H
