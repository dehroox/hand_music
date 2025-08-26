#include <immintrin.h>

#include <cstddef>

#include "hand_music.hpp"

void convert_yuv_to_gray(const unsigned char *__restrict yuv_frame_pointer,
                         unsigned char *__restrict gray_frame_pointer,
                         FrameDimensions frame_dimensions) {
    auto width = frame_dimensions.width;
    auto height = frame_dimensions.height;
    auto input_stride = static_cast<size_t>(frame_dimensions.stride_bytes);

    constexpr unsigned int pixels_per_avx2_block = 16U;

    const __m256i shuffle_mask_256 =
        _mm256_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, static_cast<char>(0x80),
                         static_cast<char>(0x80), static_cast<char>(0x80),
                         static_cast<char>(0x80), static_cast<char>(0x80),
                         static_cast<char>(0x80), static_cast<char>(0x80),
                         static_cast<char>(0x80), 16, 18, 20, 22, 24, 26, 28,
                         30, static_cast<char>(0x80), static_cast<char>(0x80),
                         static_cast<char>(0x80), static_cast<char>(0x80),
                         static_cast<char>(0x80), static_cast<char>(0x80),
                         static_cast<char>(0x80), static_cast<char>(0x80));

    for (unsigned int row = 0; row < height; ++row) {
        const auto *in_line = yuv_frame_pointer + (row * input_stride);
        auto *out_line =
            gray_frame_pointer + (row * static_cast<size_t>(width));

        unsigned int pixel_index = 0;
        for (; pixel_index + pixels_per_avx2_block <= width;
             pixel_index += pixels_per_avx2_block) {
            const auto *input_pointer_for_block =
                in_line + (static_cast<size_t>(pixel_index) * 2U);
            auto *output_pointer_for_block = out_line + pixel_index;

            __m256i ymm_input = _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(input_pointer_for_block));

            __m256i ymm_shuffled =
                _mm256_shuffle_epi8(ymm_input, shuffle_mask_256);

            __m128i low_lane_128 = _mm256_castsi256_si128(ymm_shuffled);
            __m128i high_lane_128 = _mm256_extracti128_si256(ymm_shuffled, 1);

            __m128i packed_16_bytes =
                _mm_unpacklo_epi64(low_lane_128, high_lane_128);

            _mm_storeu_si128(
                reinterpret_cast<__m128i *>(output_pointer_for_block),
                packed_16_bytes);
        }

        for (; pixel_index < width; ++pixel_index) {
            out_line[pixel_index] =
                in_line[static_cast<size_t>(pixel_index * 2U)];
        }
    }
}
