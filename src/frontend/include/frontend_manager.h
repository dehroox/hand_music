#ifndef FRONTEND_MANAGER_H
#define FRONTEND_MANAGER_H

#include <stdbool.h>

#include "common_types.h"
#include "x11_utils.h"

typedef struct {
    X11Context x11_context;
    const FrameDimensions *frame_dimensions;
} __attribute__((aligned(64))) FrontendContext;

bool Frontend_init(FrontendContext *context,
                   const FrameDimensions *frame_dimensions,
                   unsigned char *rgb_flipped_buffer);

void Frontend_run(FrontendContext *context, _Atomic bool *running_flag);

void Frontend_cleanup(FrontendContext *context);

#endif  // FRONTEND_MANAGER_H
