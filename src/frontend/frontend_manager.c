#include "include/frontend_manager.h"

#include <assert.h>

#include "../common/branch_prediction.h"

bool Frontend_init(FrontendContext *context,
                   const FrameDimensions *frame_dimensions,
                   unsigned char *rgb_flipped_buffer) {
    assert(context != NULL && "context cannot be NULL");
    assert(frame_dimensions != NULL && "frame_dimensions cannot be NULL");
    assert(rgb_flipped_buffer != NULL && "rgb_flipped_buffer cannot be NULL");
    assert(frame_dimensions->width > 0 &&
           "frame_dimensions->width must be greater than 0");
    assert(frame_dimensions->height > 0 &&
           "frame_dimensions->height must be greater than 0");
    context->frame_dimensions = frame_dimensions;

    return X11Utils_init(&context->x11_context, context->frame_dimensions,
                         rgb_flipped_buffer);
}

void Frontend_run(FrontendContext *context, _Atomic bool *running_flag) {
    assert(context != NULL && "context cannot be NULL");
    assert(running_flag != NULL && "running_flag cannot be NULL");
    assert(context->x11_context.display != NULL &&
           "context->x11_context.display cannot be NULL");
    XEvent received_event;

    while (LIKELY(atomic_load_explicit(running_flag, memory_order_relaxed))) {
        XNextEvent(context->x11_context.display, &received_event);

        if (LIKELY(received_event.type == KeyPress)) {
            KeySym pressed_key = XLookupKeysym(&received_event.xkey, 0);
            if (UNLIKELY(pressed_key == XK_Escape || pressed_key == XK_q)) {
                atomic_store_explicit(running_flag, false,
                                      memory_order_relaxed);
            }
        }

        if (LIKELY(received_event.type == ClientMessage)) {
            if (UNLIKELY((Atom)received_event.xclient.data.l[0] ==
                         context->x11_context.wm_delete_window_atom)) {
                atomic_store_explicit(running_flag, false,
                                      memory_order_relaxed);
            }
        }
    }
}

void Frontend_cleanup(FrontendContext *context) {
    assert(context != NULL && "context cannot be NULL");
    X11Utils_cleanup(&context->x11_context);
}
