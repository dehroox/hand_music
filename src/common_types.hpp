#ifndef COMMON_TYPES_HPP
#define COMMON_TYPES_HPP

#include <cstddef>

// --- Helper Structures ---
struct MemoryMappedBuffer {
    void* start_address;
    size_t length_bytes;
};

struct FrameDimensions {
    unsigned int width;
    unsigned int height;
    unsigned int stride_bytes;
};

#endif  // COMMON_TYPES_HPP
