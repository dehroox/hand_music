#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stddef.h> 

struct MemoryMappedBuffer {
    void *start_address;
    size_t length_bytes;
};

struct FrameDimensions {
    unsigned int width;
    unsigned int height;
    unsigned int stride_bytes;
};

#endif  // COMMON_TYPES_H
