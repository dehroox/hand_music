#include <assert.h>
#include <immintrin.h>

#include "../common/branch_prediction.h"
#include "../common/common_types.h"
#include "../common/constants.h"
#include "include/image_conversions.h"

static inline void process_yuv_to_gray_block(
    const unsigned char *input_block_ptr, unsigned char *output_block_ptr,
    const __m256i shuffle_mask_256) {
    assert(input_block_ptr != NULL && "input_block_ptr cannot be NULL");
    assert(output_block_ptr != NULL && "output_block_ptr cannot be NULL");

    const __m256i ymm_input =
        _mm256_loadu_si256((const __m256i *)input_block_ptr);
    const __m256i ymm_shuffled =
        _mm256_shuffle_epi8(ymm_input, shuffle_mask_256);

    const __m128i low_128 = _mm256_castsi256_si128(ymm_shuffled);
    const __m128i high_128 = _mm256_extracti128_si256(ymm_shuffled, 1);
    const __m128i packed_128 = _mm_unpacklo_epi64(low_128, high_128);

    _mm_storeu_si128((__m128i *)output_block_ptr, packed_128);
}

void ImageConversions_convert_yuv_to_gray(
    const unsigned char *__restrict yuv_frame_pointer,
    unsigned char *__restrict gray_frame_pointer,
    const FrameDimensions *frame_dimensions) {
    assert(yuv_frame_pointer != NULL && "yuv_frame_pointer cannot be NULL");
    assert(gray_frame_pointer != NULL && "gray_frame_pointer cannot be NULL");
    assert(frame_dimensions != NULL && "frame_dimensions cannot be NULL");
    assert(frame_dimensions->width > 0 &&
           "frame_dimensions->width must be greater than 0");
    assert(frame_dimensions->height > 0 &&
           "frame_dimensions->height must be greater than 0");

    static const unsigned char PIXELS_PER_AVX2_BLOCK = 16;

    /* width must be multiple of 32 pixels (16 × 2 blocks) */
    assert(LIKELY(frame_dimensions->width % (PIXELS_PER_AVX2_BLOCK * 2U) == 0));
    static const char SHUFFLE_INVALID_BYTE = -128;

    const __m256i shuffle_mask_256 = _mm256_setr_epi8(
        0, 2, 4, 6, 8, 10, 12, 14, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, 16,
        18, 20, 22, 24, 26, 28, 30, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE);

    for (unsigned int row_index = 0; row_index < frame_dimensions->height;
         ++row_index) {
        const unsigned char *current_input_line =
            yuv_frame_pointer +
            ((size_t)row_index * frame_dimensions->stride_bytes);
        unsigned char *current_output_line =
            gray_frame_pointer + ((size_t)row_index * frame_dimensions->width);

        for (unsigned int column_index = 0;
             column_index < frame_dimensions->width;
             column_index += PIXELS_PER_AVX2_BLOCK * 2U) {
            /* first AVX2 block */
            const unsigned char *input_block_ptr_0 =
                current_input_line +
                ((size_t)column_index * BYTES_PER_YUYV_PIXEL);
            unsigned char *output_block_ptr_0 =
                current_output_line + column_index;
            process_yuv_to_gray_block(input_block_ptr_0, output_block_ptr_0,
                                      shuffle_mask_256);

            /* second AVX2 block */
            const unsigned char *input_block_ptr_1 =
                input_block_ptr_0 +
                (unsigned char)(PIXELS_PER_AVX2_BLOCK * BYTES_PER_YUYV_PIXEL);
            unsigned char *output_block_ptr_1 =
                output_block_ptr_0 + PIXELS_PER_AVX2_BLOCK;
            process_yuv_to_gray_block(input_block_ptr_1, output_block_ptr_1,
                                      shuffle_mask_256);
        }
    }
}
