/*
    Common types used across most of the codebase.
*/

#pragma once

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int stride;
    unsigned int pixels;
} __attribute__((aligned(16))) FrameDimensions;
