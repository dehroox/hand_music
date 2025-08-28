/*
    Contains RGB processing related functions, exposed api is in `rgb.h`
*/

#include "rgb.h"

#include <assert.h>
#include <immintrin.h>
#include <stdlib.h>

#include "types.h"

void flipRgbHorizontal(const unsigned char *rgbBuffer,
                       unsigned char *destBuffer,
                       const FrameDimensions *frame_dimensions) {
    assert(rgbBuffer != NULL && "rgbBuffer cannot be NULL");
    assert(destBuffer != NULL && "destBuffer cannot be NULL");
    assert(frame_dimensions != NULL && "frame_dimensions cannot be NULL");
    assert(frame_dimensions->width > 0 &&
           "frame_dimensions->width must be greater than 0");
    assert(frame_dimensions->height > 0 &&
           "frame_dimensions->height must be greater than 0");
    assert(frame_dimensions->width % 4 == 0 &&
           "frame_dimensions->width must be a multiple of 4");

    const size_t rowBytes = (size_t)frame_dimensions->width * 4;

    for (size_t row = 0; row < frame_dimensions->height; ++row) {
        const unsigned char *srcRow = rgbBuffer + (row * rowBytes);
        unsigned char *destRow = destBuffer + (row * rowBytes);

        const size_t blockAmount = (size_t)frame_dimensions->width / 4;

        for (size_t block = 0; block < blockAmount; ++block) {
            const size_t srcColumn = block * 4;
            const size_t destColumn = frame_dimensions->width - 4 - srcColumn;

            const __m128i *srcVector =
                (const __m128i *)(srcRow + (srcColumn * 4));
            __m128i *destVector = (__m128i *)(destRow + (destColumn * 4));

            __m128i loaded = _mm_loadu_si128(srcVector);
            __m128i flipped =
                _mm_shuffle_epi32(loaded, _MM_SHUFFLE(0, 1, 2, 3));

            _mm_storeu_si128(destVector, flipped);
        }
    }
}
