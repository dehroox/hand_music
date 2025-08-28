/*
    Contains YUYV processing related functions, exposed api is in `yuyv.h`
*/

#include "yuyv.h"

#include <assert.h>
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "types.h"

static inline RGBLane YuyvLaneToRgb(const __m128i yuyvLane) {
    static const short FIXED_POINT_SHIFT = 8;

    const __m128i zero = _mm_setzero_si128();
    const __m128i uOffset = _mm_set1_epi16(128);
    const __m128i vOffset = _mm_set1_epi16(128);

    const __m128i redScale = _mm_set1_epi16(359);     // 1.402 * 256
    const __m128i greenUScale = _mm_set1_epi16(88);   // 0.344 * 256
    const __m128i greenVScale = _mm_set1_epi16(183);  // 0.714 * 256
    const __m128i blueScale = _mm_set1_epi16(454);    // 1.772 * 256

    const __m128i maxRgb = _mm_set1_epi16(255);

    const __m128i shuffled = _mm_shuffle_epi8(
        yuyvLane,
        _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15));

    const __m128i yBytes16 = _mm_cvtepu8_epi16(shuffled);
    const __m128i uvBytes = _mm_srli_si128(shuffled, 8);

    const __m128i uDup = _mm_shuffle_epi8(
        uvBytes, _mm_setr_epi8(0, 0, 2, 2, 4, 4, 6, 6, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80));
    const __m128i vDup = _mm_shuffle_epi8(
        uvBytes, _mm_setr_epi8(1, 1, 3, 3, 5, 5, 7, 7, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                               (char)0x80, (char)0x80));

    const __m128i u16 = _mm_cvtepu8_epi16(uDup);
    const __m128i v16 = _mm_cvtepu8_epi16(vDup);

    const __m128i uSigned = _mm_sub_epi16(u16, uOffset);
    const __m128i vSigned = _mm_sub_epi16(v16, vOffset);

    // R = Y + 1.402 * V
    __m128i red = _mm_add_epi16(
        yBytes16,
        _mm_srai_epi16(_mm_mullo_epi16(vSigned, redScale), FIXED_POINT_SHIFT));

    // G = Y - 0.344 * U - 0.714 * V
    const __m128i greenTemp =
        _mm_add_epi16(_mm_mullo_epi16(uSigned, greenUScale),
                      _mm_mullo_epi16(vSigned, greenVScale));
    __m128i green =
        _mm_sub_epi16(yBytes16, _mm_srai_epi16(greenTemp, FIXED_POINT_SHIFT));

    // B = Y + 1.772 * U
    __m128i blue = _mm_add_epi16(
        yBytes16,
        _mm_srai_epi16(_mm_mullo_epi16(uSigned, blueScale), FIXED_POINT_SHIFT));

    red = _mm_max_epi16(zero, _mm_min_epi16(red, maxRgb));
    green = _mm_max_epi16(zero, _mm_min_epi16(green, maxRgb));
    blue = _mm_max_epi16(zero, _mm_min_epi16(blue, maxRgb));

    RGBLane rgb;

    // 16b pack to 8b
    rgb.red = _mm_packus_epi16(red, zero);
    rgb.green = _mm_packus_epi16(green, zero);
    rgb.blue = _mm_packus_epi16(blue, zero);

    return rgb;
}

static inline void YuyvBlockToGray(const unsigned char *input,
                                   unsigned char *output,
                                   const __m256i shuffleMask) {
    assert(input && output);

    const __m256i block = _mm256_loadu_si256((const __m256i *)input);
    const __m256i shuffled = _mm256_shuffle_epi8(block, shuffleMask);

    const __m128i low = _mm256_castsi256_si128(shuffled);
    const __m128i high = _mm256_extracti128_si256(shuffled, 1);
    const __m128i packed = _mm_unpacklo_epi64(low, high);

    _mm_storeu_si128((__m128i *)output, packed);
}

ErrorCode yuyvToRgb(const unsigned char *yuyvBuffer, unsigned char *rgbBuffer,
               const FrameDimensions *dimensions) {
    if (yuyvBuffer == NULL || rgbBuffer == NULL || dimensions == NULL) {
        return ERROR_INVALID_ARGUMENT;
    }
    if (dimensions->width == 0 || dimensions->height == 0) {
        return ERROR_INVALID_ARGUMENT;
    }
    if (dimensions->width % 16 != 0) {
        return ERROR_INVALID_ARGUMENT;
    }

    for (size_t row = 0; row < dimensions->height; ++row) {
        const uint8_t *yuyvRow = yuyvBuffer + (row * dimensions->stride);
        uint8_t *rgbRow = rgbBuffer + (row * dimensions->width * 4);

        for (size_t col = 0; col < dimensions->width; col += 16) {
            const __m128i lane0 =
                _mm_loadu_si128((const __m128i *)(yuyvRow + (col * 2)));
            const __m128i lane1 =
                _mm_loadu_si128((const __m128i *)(yuyvRow + (col * 2) + 16));

            const RGBLane rgb0 = YuyvLaneToRgb(lane0);
            const RGBLane rgb1 = YuyvLaneToRgb(lane1);

            const __m128i red = _mm_unpacklo_epi64(rgb0.red, rgb1.red);
            const __m128i green = _mm_unpacklo_epi64(rgb0.green, rgb1.green);
            const __m128i blue = _mm_unpacklo_epi64(rgb0.blue, rgb1.blue);
            const __m128i alpha = _mm_set1_epi8((char)0xFF);

            const __m128i bgLo = _mm_unpacklo_epi8(blue, green);
            const __m128i bgHi = _mm_unpackhi_epi8(blue, green);
            const __m128i raLo = _mm_unpacklo_epi8(red, alpha);
            const __m128i raHi = _mm_unpackhi_epi8(red, alpha);

            _mm_storeu_si128((__m128i *)(rgbRow + (col * 4) + 0),
                             _mm_unpacklo_epi16(bgLo, raLo));
            _mm_storeu_si128((__m128i *)(rgbRow + (col * 4) + 16),
                             _mm_unpackhi_epi16(bgLo, raLo));
            _mm_storeu_si128((__m128i *)(rgbRow + (col * 4) + 32),
                             _mm_unpacklo_epi16(bgHi, raHi));
            _mm_storeu_si128((__m128i *)(rgbRow + (col * 4) + 48),
                             _mm_unpackhi_epi16(bgHi, raHi));
        }
    }
    return ERROR_NONE;
}

ErrorCode yuyvToGray(const unsigned char *__restrict yuyvBuffer,
                unsigned char *__restrict grayBuffer,
                const FrameDimensions *dimensions) {
    if (yuyvBuffer == NULL || grayBuffer == NULL || dimensions == NULL) {
        return ERROR_INVALID_ARGUMENT;
    }
    if (dimensions->width == 0 || dimensions->height == 0) {
        return ERROR_INVALID_ARGUMENT;
    }
    if (dimensions->width % 32 != 0) {
        return ERROR_INVALID_ARGUMENT;
    }

    const __m256i shuffleMask =
        _mm256_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -128, -128, -128, -128,
                         -128, -128, -128, -128, 16, 18, 20, 22, 24, 26, 28, 30,
                         -128, -128, -128, -128, -128, -128, -128, -128);

    for (size_t row = 0; row < dimensions->height; ++row) {
        const unsigned char *inputRow = yuyvBuffer + (row * dimensions->stride);
        unsigned char *outputRow = grayBuffer + (row * dimensions->width);
        for (size_t col = 0; col < dimensions->width; col += 32) {
            YuyvBlockToGray(inputRow + (col * 2), outputRow + col, shuffleMask);
            YuyvBlockToGray(inputRow + (col * 2) + ((ptrdiff_t)32),
                            outputRow + col + 16, shuffleMask);
        }
    }
    return ERROR_NONE;
}
