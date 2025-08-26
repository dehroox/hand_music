#ifndef FRONTEND_MANAGER_H
#define FRONTEND_MANAGER_H

#include <stdbool.h>

#include "../../common/common_types.h"
#include "x11_utils.h"

// Define a context struct for the Frontend module
typedef struct {
    X11Context x11_context;
    struct FrameDimensions frame_dimensions;
} FrontendContext;

// Function to initialize the Frontend module
bool Frontend_init(FrontendContext *context,
                   const struct FrameDimensions *frame_dimensions,
                   unsigned char *rgb_flipped_buffer);

// Function to run the Frontend event loop
void Frontend_run(FrontendContext *context, _Atomic bool *running_flag);

// Function to clean up the Frontend module
void Frontend_cleanup(FrontendContext *context);

#endif // FRONTEND_MANAGER_H
