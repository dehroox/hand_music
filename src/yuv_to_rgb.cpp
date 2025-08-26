#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "hand_music.hpp"

struct RGBLane {
    __m128i r = _mm_setzero_si128();
    __m128i g = _mm_setzero_si128();
    __m128i b = _mm_setzero_si128();
};

void convert_yuv_to_rgb(const unsigned char* __restrict yuv_frame_pointer,
                        unsigned char* __restrict rgb_frame_pointer,
                        FrameDimensions frame_dimensions) {
    constexpr int PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING = 16;
    constexpr int BYTES_PER_PIXEL_YUYV = 2;
    constexpr int YUYV_TOTAL_BYTES_PER_BLOCK =
        PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING * BYTES_PER_PIXEL_YUYV;
    constexpr int RGB_CHANNELS_PER_PIXEL = 3;

    constexpr int16_t U_CHANNEL_OFFSET = 128;
    constexpr int16_t V_CHANNEL_OFFSET = 128;
    constexpr int16_t MAX_RGB_COLOR_VALUE = 255;
    constexpr int SCALE_SHIFT_FOR_FIXED_POINT = 8;

    constexpr int16_t RED_MULTIPLIER_FROM_V_CHANNEL = 359;
    constexpr int16_t GREEN_MULTIPLIER_FROM_U_CHANNEL = 88;
    constexpr int16_t GREEN_MULTIPLIER_FROM_V_CHANNEL = 183;
    constexpr int16_t BLUE_MULTIPLIER_FROM_U_CHANNEL = 454;

    const auto frame_width_in_pixels =
        static_cast<std::size_t>(frame_dimensions.width);
    const auto frame_height_in_pixels =
        static_cast<std::size_t>(frame_dimensions.height);
    const auto frame_stride_in_bytes =
        static_cast<std::size_t>(frame_dimensions.stride_bytes);

    const __m128i shuffle_yuv_128 =
        _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);

    const __m128i dup_u_mask_128 = _mm_setr_epi8(
        0, 0, 2, 2, 4, 4, 6, 6, char(0x80), char(0x80), char(0x80), char(0x80),
        char(0x80), char(0x80), char(0x80), char(0x80));
    const __m128i dup_v_mask_128 = _mm_setr_epi8(
        1, 1, 3, 3, 5, 5, 7, 7, char(0x80), char(0x80), char(0x80), char(0x80),
        char(0x80), char(0x80), char(0x80), char(0x80));

    const __m128i zero128 = _mm_setzero_si128();
    const __m128i u_offset_128 = _mm_set1_epi16(U_CHANNEL_OFFSET);
    const __m128i v_offset_128 = _mm_set1_epi16(V_CHANNEL_OFFSET);
    const __m128i red_mul_128 = _mm_set1_epi16(RED_MULTIPLIER_FROM_V_CHANNEL);
    const __m128i green_mul_u_128 =
        _mm_set1_epi16(GREEN_MULTIPLIER_FROM_U_CHANNEL);
    const __m128i green_mul_v_128 =
        _mm_set1_epi16(GREEN_MULTIPLIER_FROM_V_CHANNEL);
    const __m128i blue_mul_128 = _mm_set1_epi16(BLUE_MULTIPLIER_FROM_U_CHANNEL);
    const __m128i max_color_128 = _mm_set1_epi16(MAX_RGB_COLOR_VALUE);

    for (std::size_t row_index_in_pixels = 0;
         row_index_in_pixels < frame_height_in_pixels; ++row_index_in_pixels) {
        const uint8_t* yuyv_row_pointer =
            yuv_frame_pointer + (row_index_in_pixels * frame_stride_in_bytes);
        uint8_t* rgb_row_pointer =
            rgb_frame_pointer + (row_index_in_pixels * frame_width_in_pixels *
                                 RGB_CHANNELS_PER_PIXEL);

        std::size_t column_index_in_pixels = 0;
        while (column_index_in_pixels < frame_width_in_pixels) {
            const std::size_t pixels_remaining_in_row =
                (frame_width_in_pixels - column_index_in_pixels <
                 PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING)
                    ? (frame_width_in_pixels - column_index_in_pixels)
                    : PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING;

            std::array<uint8_t, YUYV_TOTAL_BYTES_PER_BLOCK> input_block{};
            std::memcpy(input_block.data(),
                        yuyv_row_pointer +
                            (column_index_in_pixels * BYTES_PER_PIXEL_YUYV),
                        pixels_remaining_in_row * BYTES_PER_PIXEL_YUYV);

            const __m128i lane0 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(input_block.data()));
            const __m128i lane1 =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                    input_block.data() + (PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING *
                                          BYTES_PER_PIXEL_YUYV / 2)));

            auto process_lane = [&](const __m128i lane_bytes, RGBLane& out) {
                const __m128i shuffled =
                    _mm_shuffle_epi8(lane_bytes, shuffle_yuv_128);
                const __m128i ys_16 = _mm_cvtepu8_epi16(shuffled);
                const __m128i uv_bytes = _mm_srli_si128(shuffled, 8);
                const __m128i u_dup_bytes =
                    _mm_shuffle_epi8(uv_bytes, dup_u_mask_128);
                const __m128i v_dup_bytes =
                    _mm_shuffle_epi8(uv_bytes, dup_v_mask_128);
                const __m128i u_16 = _mm_cvtepu8_epi16(u_dup_bytes);
                const __m128i v_16 = _mm_cvtepu8_epi16(v_dup_bytes);
                const __m128i u_signed = _mm_sub_epi16(u_16, u_offset_128);
                const __m128i v_signed = _mm_sub_epi16(v_16, v_offset_128);
                const __m128i y_signed = ys_16;

                __m128i r_calc = _mm_add_epi16(
                    y_signed,
                    _mm_srai_epi16(_mm_mullo_epi16(v_signed, red_mul_128),
                                   SCALE_SHIFT_FOR_FIXED_POINT));
                __m128i green_tmp =
                    _mm_add_epi16(_mm_mullo_epi16(u_signed, green_mul_u_128),
                                  _mm_mullo_epi16(v_signed, green_mul_v_128));
                __m128i g_calc = _mm_sub_epi16(
                    y_signed,
                    _mm_srai_epi16(green_tmp, SCALE_SHIFT_FOR_FIXED_POINT));
                __m128i b_calc = _mm_add_epi16(
                    y_signed,
                    _mm_srai_epi16(_mm_mullo_epi16(u_signed, blue_mul_128),
                                   SCALE_SHIFT_FOR_FIXED_POINT));

                r_calc = _mm_max_epi16(zero128,
                                       _mm_min_epi16(r_calc, max_color_128));
                g_calc = _mm_max_epi16(zero128,
                                       _mm_min_epi16(g_calc, max_color_128));
                b_calc = _mm_max_epi16(zero128,
                                       _mm_min_epi16(b_calc, max_color_128));

                out.r = _mm_packus_epi16(r_calc, zero128);
                out.g = _mm_packus_epi16(g_calc, zero128);
                out.b = _mm_packus_epi16(b_calc, zero128);
            };

            RGBLane lane0_rgb{};
            RGBLane lane1_rgb{};
            process_lane(lane0, lane0_rgb);
            process_lane(lane1, lane1_rgb);

            std::array<uint8_t, PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING> r_tmp{};
            std::array<uint8_t, PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING> g_tmp{};
            std::array<uint8_t, PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING> b_tmp{};
            _mm_store_si128(reinterpret_cast<__m128i*>(r_tmp.data()),
                            _mm_unpacklo_epi64(lane0_rgb.r, lane1_rgb.r));
            _mm_store_si128(reinterpret_cast<__m128i*>(g_tmp.data()),
                            _mm_unpacklo_epi64(lane0_rgb.g, lane1_rgb.g));
            _mm_store_si128(reinterpret_cast<__m128i*>(b_tmp.data()),
                            _mm_unpacklo_epi64(lane0_rgb.b, lane1_rgb.b));

            uint8_t* output_pixel_pointer_base =
                rgb_row_pointer +
                (column_index_in_pixels * RGB_CHANNELS_PER_PIXEL);
            for (std::size_t i = 0; i < pixels_remaining_in_row; i++) {
                output_pixel_pointer_base[(3 * i) + 0] = r_tmp.at(i);
                output_pixel_pointer_base[(3 * i) + 1] = g_tmp.at(i);
                output_pixel_pointer_base[(3 * i) + 2] = b_tmp.at(i);
            }

            column_index_in_pixels += PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING;
        }
    }
}
