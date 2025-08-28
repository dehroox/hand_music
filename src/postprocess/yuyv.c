#include "yuyv.h"

#include <assert.h>
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "branch.h"
#include "types.h"

static inline RGBLane yuyvLaneToRgb(const __m128i yuyvLane) {
    static const short FIXED_POINT_SHIFT = 8;

    const __m128i zero = _mm_setzero_si128();
    const __m128i uOffset = _mm_set1_epi16(128);
    const __m128i vOffset = _mm_set1_epi16(128);

    const __m128i redScale = _mm_set1_epi16(359);
    const __m128i greenUScale = _mm_set1_epi16(88);
    const __m128i greenVScale = _mm_set1_epi16(183);
    const __m128i blueScale = _mm_set1_epi16(454);

    const __m128i maxRgb = _mm_set1_epi16(255);

    const __m128i shuffled = _mm_shuffle_epi8(
        yuyvLane,
        _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15));

    const __m128i yBytes16 = _mm_cvtepu8_epi16(shuffled);
    const __m128i uvBytes = _mm_srli_si128(shuffled, 8);

    const __m128i uDuplicated = _mm_shuffle_epi8(
        uvBytes, _mm_setr_epi8(0, 0, 2, 2, 4, 4, 6, 6, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80));
    const __m128i vDuplicated = _mm_shuffle_epi8(
        uvBytes, _mm_setr_epi8(1, 1, 3, 3, 5, 5, 7, 7, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80));

    const __m128i u16 = _mm_cvtepu8_epi16(uDuplicated);
    const __m128i v16 = _mm_cvtepu8_epi16(vDuplicated);

    const __m128i uSigned = _mm_sub_epi16(u16, uOffset);
    const __m128i vSigned = _mm_sub_epi16(v16, vOffset);

    __m128i red = _mm_add_epi16(
        yBytes16,
        _mm_srai_epi16(_mm_mullo_epi16(vSigned, redScale), FIXED_POINT_SHIFT));

    const __m128i gTemp = _mm_add_epi16(_mm_mullo_epi16(uSigned, greenUScale),
                                        _mm_mullo_epi16(vSigned, greenVScale));
    __m128i green =
        _mm_sub_epi16(yBytes16, _mm_srai_epi16(gTemp, FIXED_POINT_SHIFT));

    __m128i blue = _mm_add_epi16(
        yBytes16,
        _mm_srai_epi16(_mm_mullo_epi16(uSigned, blueScale), FIXED_POINT_SHIFT));

    red = _mm_max_epi16(zero, _mm_min_epi16(red, maxRgb));
    green = _mm_max_epi16(zero, _mm_min_epi16(green, maxRgb));
    blue = _mm_max_epi16(zero, _mm_min_epi16(blue, maxRgb));

    RGBLane rgb = {0};
    rgb.red = _mm_packus_epi16(red, zero);
    rgb.green = _mm_packus_epi16(green, zero);
    rgb.blue = _mm_packus_epi16(blue, zero);

    return rgb;
}

void yuyvToRgb(const unsigned char *yuyvBuffer, unsigned char *rgbBuffer,
               const FrameDimensions *dimensions) {
    assert(yuyvBuffer != NULL && "yuyvBuffer cannot be NULL");
    assert(dimensions != NULL && "dimensions cannot be NULL");
    assert(dimensions->width > 0 && "dimensions width must be greater than 0");
    assert(dimensions->height > 0 &&
           "dimensions height must be greater than 0");

    assert(LIKELY(dimensions->width % 16 == 0));

    for (size_t rowIndex = 0; rowIndex < dimensions->height; ++rowIndex) {
        const uint8_t *yuyvRowPtr =
            yuyvBuffer + (rowIndex * dimensions->stride);
        uint8_t *rgbRowPtr = rgbBuffer + (rowIndex * dimensions->width * 4);

        for (size_t columnIndex = 0; columnIndex < dimensions->width;
             columnIndex += 16) {
            const __m128i lane0 = _mm_loadu_si128(
                (const __m128i *)(yuyvRowPtr + (columnIndex * 2)));
            const __m128i lane1 = _mm_loadu_si128(
                (const __m128i *)(yuyvRowPtr + (columnIndex * 2) + 16));

            const RGBLane lane0Rgb = yuyvLaneToRgb(lane0);
            const RGBLane lane1Rgb = yuyvLaneToRgb(lane1);

            const __m128i red = _mm_unpacklo_epi64(lane0Rgb.red, lane1Rgb.red);
            const __m128i green =
                _mm_unpacklo_epi64(lane0Rgb.green, lane1Rgb.green);
            const __m128i blue =
                _mm_unpacklo_epi64(lane0Rgb.blue, lane1Rgb.blue);
            const __m128i alpha = _mm_set1_epi8((char)0xFF);

            const __m128i bgLo = _mm_unpacklo_epi8(blue, green);
            const __m128i bgHi = _mm_unpackhi_epi8(blue, green);
            const __m128i raLo = _mm_unpacklo_epi8(red, alpha);
            const __m128i raHi = _mm_unpackhi_epi8(red, alpha);

            const __m128i bgra0 = _mm_unpacklo_epi16(bgLo, raLo);
            const __m128i bgra1 = _mm_unpackhi_epi16(bgLo, raLo);
            const __m128i bgra2 = _mm_unpacklo_epi16(bgHi, raHi);
            const __m128i bgra3 = _mm_unpackhi_epi16(bgHi, raHi);

            uint8_t *outputPixelPtr = rgbRowPtr + (columnIndex * 4);

            _mm_storeu_si128((__m128i *)(outputPixelPtr + 0), bgra0);
            _mm_storeu_si128((__m128i *)(outputPixelPtr + 16), bgra1);
            _mm_storeu_si128((__m128i *)(outputPixelPtr + 32), bgra2);
            _mm_storeu_si128((__m128i *)(outputPixelPtr + 48), bgra3);
        }
    }
}
