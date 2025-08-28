#pragma once

#include <assert.h>
#include <immintrin.h>

#include "types.h"

typedef struct {
    __m128i red;
    __m128i green;
    __m128i blue;
} __attribute__((aligned(64))) RGBLane;

ErrorCode yuyvToRgb(const unsigned char *yuyvBuffer, unsigned char *rgbBuffer,
               const FrameDimensions *dimensions);
ErrorCode yuyvToGray(const unsigned char *__restrict yuyvBuffer,
                unsigned char *__restrict grayBuffer,
                const FrameDimensions *dimensions);
