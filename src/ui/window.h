#pragma once
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "types.h"

typedef struct BackendInternal BackendInternal;

typedef struct {
    BackendInternal *internal;
    FrameDimensions dimensions;
} __attribute__((aligned(32))) WindowState;

ErrorCode Window_create(WindowState *state, const char *title,
                        FrameDimensions dimensions);

void Window_draw(WindowState *state, const unsigned char *buffer);

bool Window_pollEvents(WindowState *state);

void Window_destroy(WindowState *state);
