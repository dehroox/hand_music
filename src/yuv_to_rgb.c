#include <immintrin.h>
#include <stdint.h>
#include <string.h>

#include "hand_music.h"

struct RGBLane {
    __m128i r;
    __m128i g;
    __m128i b;
};

static inline void process_rgb_lane(const __m128i lane_bytes,
                                    struct RGBLane *out) {
    const int16_t U_CHANNEL_OFFSET = 128;
    const int16_t V_CHANNEL_OFFSET = 128;
    const int16_t MAX_RGB_COLOR_VALUE = 255;
    const int SCALE_SHIFT_FOR_FIXED_POINT = 8;

    const int16_t RED_MULTIPLIER_FROM_V_CHANNEL = 359;
    const int16_t GREEN_MULTIPLIER_FROM_U_CHANNEL = 88;
    const int16_t GREEN_MULTIPLIER_FROM_V_CHANNEL = 183;
    const int16_t BLUE_MULTIPLIER_FROM_U_CHANNEL = 454;

    const __m128i shuffle_yuv_128 =
        _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);

    const __m128i dup_u_mask_128 = _mm_setr_epi8(
        0, 0, 2, 2, 4, 4, 6, 6, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
        (char)0x80, (char)0x80, (char)0x80, (char)0x80);
    const __m128i dup_v_mask_128 = _mm_setr_epi8(
        1, 1, 3, 3, 5, 5, 7, 7, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
        (char)0x80, (char)0x80, (char)0x80, (char)0x80);

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

    const __m128i shuffled = _mm_shuffle_epi8(lane_bytes, shuffle_yuv_128);
    const __m128i ys_16 = _mm_cvtepu8_epi16(shuffled);
    const __m128i uv_bytes = _mm_srli_si128(shuffled, 8);
    const __m128i u_dup_bytes = _mm_shuffle_epi8(uv_bytes, dup_u_mask_128);
    const __m128i v_dup_bytes = _mm_shuffle_epi8(uv_bytes, dup_v_mask_128);
    const __m128i u_16 = _mm_cvtepu8_epi16(u_dup_bytes);
    const __m128i v_16 = _mm_cvtepu8_epi16(v_dup_bytes);
    const __m128i u_signed = _mm_sub_epi16(u_16, u_offset_128);
    const __m128i v_signed = _mm_sub_epi16(v_16, v_offset_128);
    const __m128i y_signed = ys_16;

    __m128i r_calc = _mm_add_epi16(
        y_signed, _mm_srai_epi16(_mm_mullo_epi16(v_signed, red_mul_128),
                                 SCALE_SHIFT_FOR_FIXED_POINT));
    __m128i green_tmp =
        _mm_add_epi16(_mm_mullo_epi16(u_signed, green_mul_u_128),
                      _mm_mullo_epi16(v_signed, green_mul_v_128));
    __m128i g_calc = _mm_sub_epi16(
        y_signed, _mm_srai_epi16(green_tmp, SCALE_SHIFT_FOR_FIXED_POINT));
    __m128i b_calc = _mm_add_epi16(
        y_signed, _mm_srai_epi16(_mm_mullo_epi16(u_signed, blue_mul_128),
                                 SCALE_SHIFT_FOR_FIXED_POINT));

    r_calc = _mm_max_epi16(zero128, _mm_min_epi16(r_calc, max_color_128));
    g_calc = _mm_max_epi16(zero128, _mm_min_epi16(g_calc, max_color_128));
    b_calc = _mm_max_epi16(zero128, _mm_min_epi16(b_calc, max_color_128));

    out->r = _mm_packus_epi16(r_calc, zero128);
    out->g = _mm_packus_epi16(g_calc, zero128);
    out->b = _mm_packus_epi16(b_calc, zero128);
}

#define PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING 16
#define BYTES_PER_PIXEL_YUYV 2
#define YUYV_TOTAL_BYTES_PER_BLOCK \
    (PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING * BYTES_PER_PIXEL_YUYV)
#define RGB_CHANNELS_PER_PIXEL 4
#define ALPHA_VALUE 0xFF

// what the fuck, evil black magic
void convert_yuv_to_rgb(const unsigned char *__restrict yuv_frame_pointer,
                        unsigned char *__restrict rgb_frame_pointer,
                        struct FrameDimensions frame_dimensions) {
    const size_t frame_width_in_pixels = (size_t)frame_dimensions.width;
    const size_t frame_height_in_pixels = (size_t)frame_dimensions.height;
    const size_t frame_stride_in_bytes = (size_t)frame_dimensions.stride_bytes;

    for (size_t row_index_in_pixels = 0;
         row_index_in_pixels < frame_height_in_pixels; ++row_index_in_pixels) {
        const uint8_t *yuyv_row_pointer =
            yuv_frame_pointer + (row_index_in_pixels * frame_stride_in_bytes);
        uint8_t *rgb_row_pointer =
            rgb_frame_pointer + (row_index_in_pixels * frame_width_in_pixels *
                                 RGB_CHANNELS_PER_PIXEL);

        size_t column_index_in_pixels = 0;
        while (column_index_in_pixels < frame_width_in_pixels) {
            const size_t pixels_remaining_in_row =
                (frame_width_in_pixels - column_index_in_pixels <
                 PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING)
                    ? (frame_width_in_pixels - column_index_in_pixels)
                    : PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING;

            uint8_t input_block[YUYV_TOTAL_BYTES_PER_BLOCK];
            memcpy(input_block,
                   yuyv_row_pointer +
                       (column_index_in_pixels * BYTES_PER_PIXEL_YUYV),
                   pixels_remaining_in_row * BYTES_PER_PIXEL_YUYV);

            const __m128i lane0 = _mm_loadu_si128((const __m128i *)input_block);
            const __m128i lane1 = _mm_loadu_si128(
                (const __m128i *)(input_block +
                                  (PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING *
                                   BYTES_PER_PIXEL_YUYV / 2)));

            struct RGBLane lane0_rgb = {0};
            struct RGBLane lane1_rgb = {0};
            process_rgb_lane(lane0, &lane0_rgb);
            process_rgb_lane(lane1, &lane1_rgb);

            uint8_t r_tmp[PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING];
            uint8_t g_tmp[PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING];
            uint8_t b_tmp[PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING];
            _mm_store_si128((__m128i *)r_tmp,
                            _mm_unpacklo_epi64(lane0_rgb.r, lane1_rgb.r));
            _mm_store_si128((__m128i *)g_tmp,
                            _mm_unpacklo_epi64(lane0_rgb.g, lane1_rgb.g));
            _mm_store_si128((__m128i *)b_tmp,
                            _mm_unpacklo_epi64(lane0_rgb.b, lane1_rgb.b));

            uint8_t *output_pixel_pointer_base =
                rgb_row_pointer +
                (column_index_in_pixels * RGB_CHANNELS_PER_PIXEL);
            for (size_t i = 0; i < pixels_remaining_in_row; i++) {
                output_pixel_pointer_base[(4 * i) + 0] = b_tmp[i];  // blue
                output_pixel_pointer_base[(4 * i) + 1] = g_tmp[i];  // green
                output_pixel_pointer_base[(4 * i) + 2] = r_tmp[i];  // red
                output_pixel_pointer_base[(4 * i) + 3] = ALPHA_VALUE;
            }

            column_index_in_pixels += PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING;
        }
    }
}
