#include <immintrin.h>

#include "hand_music.h"

#define PIXELS_PER_AVX2_BLOCK 16U

void convert_yuv_to_gray(const unsigned char *__restrict yuv_frame_pointer,
                         unsigned char *__restrict gray_frame_pointer,
                         struct FrameDimensions frame_dimensions) {
    unsigned int width = frame_dimensions.width;
    unsigned int height = frame_dimensions.height;
    size_t input_stride = (size_t)frame_dimensions.stride_bytes;

    const __m256i shuffle_mask_256 = _mm256_setr_epi8(
        0, 2, 4, 6, 8, 10, 12, 14, (char)0x80, (char)0x80, (char)0x80,
        (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80, 16, 18, 20,
        22, 24, 26, 28, 30, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
        (char)0x80, (char)0x80, (char)0x80, (char)0x80);

    for (unsigned int row = 0; row < height; ++row) {
        const unsigned char *in_line = yuv_frame_pointer + (row * input_stride);
        unsigned char *out_line = gray_frame_pointer + (row * (size_t)width);

        unsigned int pixel_index = 0;
        for (; pixel_index + PIXELS_PER_AVX2_BLOCK <= width;
             pixel_index += PIXELS_PER_AVX2_BLOCK) {
            const unsigned char *input_pointer_for_block =
                in_line + ((size_t)pixel_index * 2U);
            unsigned char *output_pointer_for_block = out_line + pixel_index;

            __m256i ymm_input =
                _mm256_loadu_si256((const __m256i *)input_pointer_for_block);

            __m256i ymm_shuffled =
                _mm256_shuffle_epi8(ymm_input, shuffle_mask_256);

            __m128i low_lane_128 = _mm256_castsi256_si128(ymm_shuffled);
            __m128i high_lane_128 = _mm256_extracti128_si256(ymm_shuffled, 1);

            __m128i packed_16_bytes =
                _mm_unpacklo_epi64(low_lane_128, high_lane_128);

            _mm_storeu_si128((__m128i *)output_pointer_for_block,
                             packed_16_bytes);
        }

        for (; pixel_index < width; ++pixel_index) {
            out_line[pixel_index] = in_line[(size_t)pixel_index * 2U];
        }
    }
}
