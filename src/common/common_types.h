#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stddef.h>

typedef struct {
    void *start_address;
    size_t length_bytes;
} __attribute__((aligned(16))) MemoryMappedBuffer;

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int stride_bytes;
} __attribute__((aligned(16))) FrameDimensions;

#endif  // COMMON_TYPES_H
