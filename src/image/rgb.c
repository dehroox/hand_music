/*
    Contains RGB processing related functions, exposed api is in `rgb.h`
*/

#include "rgb.h"

#include <assert.h>

#ifdef __AVX__
#include <immintrin.h>
#endif

#include <stdlib.h>

#include "branch.h"
#include "types.h"

ErrorCode flipRgbHorizontal(const unsigned char *rgbBuffer,
			    unsigned char *destBuffer,
			    const FrameDimensions *frame_dimensions) {
    if (UNLIKELY(rgbBuffer == NULL || destBuffer == NULL ||
		 frame_dimensions == NULL)) {
	return ERROR_INVALID_ARGUMENT;
    }
    if (UNLIKELY(frame_dimensions->width == 0 ||
		 frame_dimensions->height == 0)) {
	return ERROR_INVALID_ARGUMENT;
    }
    if (UNLIKELY(frame_dimensions->width % 4 != 0)) {
	return ERROR_INVALID_ARGUMENT;
    }

    const size_t rowBytes = (size_t)frame_dimensions->width * 4;

#ifdef __AVX2__
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
#else
    for (size_t row = 0; row < frame_dimensions->height; ++row) {
	const unsigned char *src = rgbBuffer + (row * rowBytes);
	unsigned char *dest = destBuffer + (row * rowBytes);

	size_t width = frame_dimensions->width;
	size_t number = (width + 31) / 32;  // number of 32-pixel blocks
	size_t rem = width % 32;

	switch (rem) {
	    case 0:
		do {
		    for (size_t i = 0; i < 32; ++i) {
			const unsigned char *srcPixel = src + (i * 4);
			unsigned char *destPixel = dest + ((width - 1 - i) * 4);

			destPixel[0] = srcPixel[0];
			destPixel[1] = srcPixel[1];
			destPixel[2] = srcPixel[2];
			destPixel[3] = srcPixel[3];
		    }

		    src += 128;
		    dest -= 128;
		    [[fallthrough]];

		    case 31:
		    case 30:
		    case 29:
		    case 28:
		    case 27:
		    case 26:
		    case 25:
		    case 24:
		    case 23:
		    case 22:
		    case 21:
		    case 20:
		    case 19:
		    case 18:
		    case 17:
		    case 16:
		    case 15:
		    case 14:
		    case 13:
		    case 12:
		    case 11:
		    case 10:
		    case 9:
		    case 8:
		    case 7:
		    case 6:
		    case 5:
		    case 4:
		    case 3:
		    case 2:
		    case 1:
		    default:
		} while (--number > 0);
	}
    }
#endif
    return ERROR_NONE;
}
