#include <assert.h>
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "hand_music.h"

#define PIXELS_PER_AVX2_BLOCK 16U
#define BYTES_PER_YUYV_PIXEL 2U
#define SHUFFLE_INVALID_BYTE 0x80
#define PREFETCH_DISTANCE_1 64U
#define PREFETCH_DISTANCE_2 128U
#define PREFETCH_DISTANCE_3 192U
#define PREFETCH_DISTANCE_4 256U

void convert_yuv_to_gray(const unsigned char *__restrict yuv_frame_pointer,
                         unsigned char *__restrict gray_frame_pointer,
                         struct FrameDimensions frame_dimensions) {
    unsigned int row_index;
    unsigned int column_index;
    unsigned int image_width = frame_dimensions.width;
    unsigned int image_height = frame_dimensions.height;
    size_t input_stride_bytes = (size_t)frame_dimensions.stride_bytes;
    const __m256i shuffle_mask_256 = _mm256_setr_epi8(
        0, 2, 4, 6, 8, 10, 12, 14, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, 16,
        18, 20, 22, 24, 26, 28, 30, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE,
        SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE, SHUFFLE_INVALID_BYTE);

    /* Ensure width is multiple of 16 pixels for AVX2-only */
    assert(image_width % PIXELS_PER_AVX2_BLOCK == 0);

    for (row_index = 0; row_index < image_height; ++row_index) {
        const unsigned char *current_input_line =
            yuv_frame_pointer + ((size_t)row_index * input_stride_bytes);
        unsigned char *current_output_line =
            gray_frame_pointer + ((size_t)row_index * image_width);

        /* Unroll 4 AVX2 blocks per iteration */
        for (column_index = 0; column_index < image_width;
             column_index += 4 * PIXELS_PER_AVX2_BLOCK) {
            const unsigned char *input_block_pointers[4];
            unsigned char *output_block_pointers[4];
            __m256i ymm_inputs[4];
            __m256i ymm_shuffled[4];
            __m128i low_lanes[4];
            __m128i high_lanes[4];
            __m128i packed_blocks[4];
            unsigned int block_index;

            for (block_index = 0; block_index < 4; ++block_index) {
                input_block_pointers[block_index] =
                    current_input_line +
                    ((size_t)(column_index +
                              (block_index * PIXELS_PER_AVX2_BLOCK)) *
                     BYTES_PER_YUYV_PIXEL);
                output_block_pointers[block_index] =
                    current_output_line + column_index +
                    (size_t)(block_index * PIXELS_PER_AVX2_BLOCK);

                /* Prefetch future input data at multiple distances */
                switch (block_index) {
                    case 0:
                        _mm_prefetch(
                            (const char *)(input_block_pointers[block_index] +
                                           PREFETCH_DISTANCE_1),
                            _MM_HINT_T0);
                        break;
                    case 1:
                        _mm_prefetch(
                            (const char *)(input_block_pointers[block_index] +
                                           PREFETCH_DISTANCE_2),
                            _MM_HINT_T0);
                        break;
                    case 2:
                        _mm_prefetch(
                            (const char *)(input_block_pointers[block_index] +
                                           PREFETCH_DISTANCE_3),
                            _MM_HINT_T0);
                        break;
                    case 3:
                        _mm_prefetch(
                            (const char *)(input_block_pointers[block_index] +
                                           PREFETCH_DISTANCE_4),
                            _MM_HINT_T0);
                        break;
                    default:
                        break;
                }

                /* Load, shuffle, unpack */
                ymm_inputs[block_index] = _mm256_loadu_si256(
                    (const __m256i *)input_block_pointers[block_index]);
                ymm_shuffled[block_index] = _mm256_shuffle_epi8(
                    ymm_inputs[block_index], shuffle_mask_256);
                low_lanes[block_index] =
                    _mm256_castsi256_si128(ymm_shuffled[block_index]);
                high_lanes[block_index] =
                    _mm256_extracti128_si256(ymm_shuffled[block_index], 1);
                packed_blocks[block_index] = _mm_unpacklo_epi64(
                    low_lanes[block_index], high_lanes[block_index]);

                /* Conditional aligned store */
                if (((uintptr_t)output_block_pointers[block_index] %
                     PIXELS_PER_AVX2_BLOCK) == 0) {
                    _mm_store_si128(
                        (__m128i *)output_block_pointers[block_index],
                        packed_blocks[block_index]);
                } else {
                    _mm_storeu_si128(
                        (__m128i *)output_block_pointers[block_index],
                        packed_blocks[block_index]);
                }
            }
        }
    }
}
