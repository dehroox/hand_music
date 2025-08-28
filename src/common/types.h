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

typedef enum {
    ERROR_NONE = 0,
    ERROR_FILE_OPEN_FAILED = -1,
    ERROR_IOCTL_FAILED = -2,
    ERROR_MMAP_FAILED = -3,
    ERROR_ALLOCATION_FAILED = -4,
    ERROR_INVALID_ARGUMENT = -5,
    ERROR_UNSUPPORTED_OPERATION = -6,
    ERROR_DISPLAY_OPEN_FAILED = -7
} ErrorCode;
