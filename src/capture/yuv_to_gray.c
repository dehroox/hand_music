#include <assert.h>
#include <immintrin.h>

#include "../common/branch_prediction.h"
#include "../common/common_types.h"
#include "../common/constants.h"
#include "include/image_conversions.h"

void ImageConversions_convert_yuv_to_gray(
    const unsigned char *__restrict yuv_frame_pointer,
    unsigned char *__restrict gray_frame_pointer,
    FrameDimensions *frame_dimensions) {
    unsigned int row_index;
    unsigned int column_index;

    /* width must be multiple of 32 pixels (16 × 2 blocks) */
    assert(LIKELY(frame_dimensions->width % (PIXELS_PER_AVX2_BLOCK * 2U) == 0));

    const __m256i shuffle_mask_256 = _mm256_setr_epi8(
        0, 2, 4, 6, 8, 10, 12, 14, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, 16,
        18, 20, 22, 24, 26, 28, 30, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE);

    for (row_index = 0; row_index < frame_dimensions->height; ++row_index) {
        const unsigned char *current_input_line =
            yuv_frame_pointer +
            ((size_t)row_index * frame_dimensions->stride_bytes);
        unsigned char *current_output_line =
            gray_frame_pointer + ((size_t)row_index * frame_dimensions->width);

        for (column_index = 0; column_index < frame_dimensions->width;
             column_index += PIXELS_PER_AVX2_BLOCK * 2U) {
            /* first AVX2 block */
            const unsigned char *input_block_ptr_0 =
                current_input_line +
                ((size_t)column_index * BYTES_PER_YUYV_PIXEL);
            unsigned char *output_block_ptr_0 =
                current_output_line + column_index;

            __m256i ymm_input_0 =
                _mm256_loadu_si256((const __m256i *)input_block_ptr_0);
            __m256i ymm_shuffled_0 =
                _mm256_shuffle_epi8(ymm_input_0, shuffle_mask_256);

            __m128i low_128_0 = _mm256_castsi256_si128(ymm_shuffled_0);
            __m128i high_128_0 = _mm256_extracti128_si256(ymm_shuffled_0, 1);
            __m128i packed_128_0 = _mm_unpacklo_epi64(low_128_0, high_128_0);

            _mm_storeu_si128((__m128i *)output_block_ptr_0, packed_128_0);

            /* second AVX2 block */
            const unsigned char *input_block_ptr_1 =
                input_block_ptr_0 +
                (unsigned char)(PIXELS_PER_AVX2_BLOCK * BYTES_PER_YUYV_PIXEL);
            unsigned char *output_block_ptr_1 =
                output_block_ptr_0 + PIXELS_PER_AVX2_BLOCK;

            __m256i ymm_input_1 =
                _mm256_loadu_si256((const __m256i *)input_block_ptr_1);
            __m256i ymm_shuffled_1 =
                _mm256_shuffle_epi8(ymm_input_1, shuffle_mask_256);

            __m128i low_128_1 = _mm256_castsi256_si128(ymm_shuffled_1);
            __m128i high_128_1 = _mm256_extracti128_si256(ymm_shuffled_1, 1);
            __m128i packed_128_1 = _mm_unpacklo_epi64(low_128_1, high_128_1);

            _mm_storeu_si128((__m128i *)output_block_ptr_1, packed_128_1);
        }
    }
}
