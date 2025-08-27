// what the fuck, fuck fuck??

#include <assert.h>
#include <immintrin.h>
#include <stdint.h>
#include <string.h>

#include "../common/common_types.h"
#include "../common/constants.h"
#include "include/image_conversions.h"

struct RGBLane {
    __m128i r_lane;
    __m128i g_lane;
    __m128i b_lane;
};

/* --- Convert a single SIMD lane of YUV to RGB --- */
static inline void convert_yuv_lane_to_rgb(const __m128i lane_bytes,
                                           struct RGBLane *out_rgb_lane) {
    __m128i zero_vector = _mm_setzero_si128();
    __m128i u_offset_vector = _mm_set1_epi16(U_CHANNEL_OFFSET);
    __m128i v_offset_vector = _mm_set1_epi16(V_CHANNEL_OFFSET);
    __m128i red_multiplier_vector = _mm_set1_epi16(RED_FROM_V);
    __m128i green_multiplier_u_vector = _mm_set1_epi16(GREEN_FROM_U);
    __m128i green_multiplier_v_vector = _mm_set1_epi16(GREEN_FROM_V);
    __m128i blue_multiplier_vector = _mm_set1_epi16(BLUE_FROM_U);
    __m128i max_rgb_vector = _mm_set1_epi16(MAX_RGB_VALUE);

    __m128i shuffled_bytes = _mm_shuffle_epi8(lane_bytes, SHUFFLE_YUV_MASK);
    __m128i y_bytes_16 = _mm_cvtepu8_epi16(shuffled_bytes);
    __m128i uv_bytes = _mm_srli_si128(shuffled_bytes, 8);
    __m128i u_bytes_duplicated = _mm_shuffle_epi8(uv_bytes, DUPLICATE_U_MASK);
    __m128i v_bytes_duplicated = _mm_shuffle_epi8(uv_bytes, DUPLICATE_V_MASK);
    __m128i u_bytes_16 = _mm_cvtepu8_epi16(u_bytes_duplicated);
    __m128i v_bytes_16 = _mm_cvtepu8_epi16(v_bytes_duplicated);

    __m128i u_signed = _mm_sub_epi16(u_bytes_16, u_offset_vector);
    __m128i v_signed = _mm_sub_epi16(v_bytes_16, v_offset_vector);
    __m128i y_signed = y_bytes_16;

    __m128i r_calculated = _mm_add_epi16(
        y_signed,
        _mm_srai_epi16(_mm_mullo_epi16(v_signed, red_multiplier_vector),
                       SHIFT_FIXED_POINT));

    __m128i green_temp =
        _mm_add_epi16(_mm_mullo_epi16(u_signed, green_multiplier_u_vector),
                      _mm_mullo_epi16(v_signed, green_multiplier_v_vector));

    __m128i g_calculated =
        _mm_sub_epi16(y_signed, _mm_srai_epi16(green_temp, SHIFT_FIXED_POINT));

    __m128i b_calculated = _mm_add_epi16(
        y_signed,
        _mm_srai_epi16(_mm_mullo_epi16(u_signed, blue_multiplier_vector),
                       SHIFT_FIXED_POINT));

    r_calculated =
        _mm_max_epi16(zero_vector, _mm_min_epi16(r_calculated, max_rgb_vector));
    g_calculated =
        _mm_max_epi16(zero_vector, _mm_min_epi16(g_calculated, max_rgb_vector));
    b_calculated =
        _mm_max_epi16(zero_vector, _mm_min_epi16(b_calculated, max_rgb_vector));

    out_rgb_lane->r_lane = _mm_packus_epi16(r_calculated, zero_vector);
    out_rgb_lane->g_lane = _mm_packus_epi16(g_calculated, zero_vector);
    out_rgb_lane->b_lane = _mm_packus_epi16(b_calculated, zero_vector);
}

/* --- Convert entire YUV frame to RGBA --- */
void ImageConversions_convert_yuv_to_rgb(
    const unsigned char *__restrict yuv_frame_pointer,
    unsigned char *__restrict rgb_frame_pointer,
    FrameDimensions *frame_dimensions) {
    assert(frame_dimensions->width % (PIXELS_PER_SIMD_BLOCK) == 0);

    for (size_t row_index = 0; row_index < frame_dimensions->height;
         ++row_index) {
        const uint8_t *yuyv_row_ptr =
            yuv_frame_pointer + (row_index * frame_dimensions->stride_bytes);
        uint8_t *rgb_row_ptr =
            rgb_frame_pointer +
            (row_index * frame_dimensions->width * RGB_CHANNELS);

        for (size_t column_index = 0; column_index < frame_dimensions->width;
             column_index += PIXELS_PER_SIMD_BLOCK) {
            __m128i lane0 = _mm_loadu_si128(
                (const __m128i *)(yuyv_row_ptr +
                                  (column_index * BYTES_PER_YUYV_PIXEL)));
            __m128i lane1 = _mm_loadu_si128(
                (const __m128i *)(yuyv_row_ptr +
                                  (column_index * BYTES_PER_YUYV_PIXEL) +
                                  (TOTAL_BYTES_PER_SIMD_BLOCK / 2)));

            struct RGBLane lane0_rgb = {0};
            struct RGBLane lane1_rgb = {0};
            convert_yuv_lane_to_rgb(lane0, &lane0_rgb);
            convert_yuv_lane_to_rgb(lane1, &lane1_rgb);

            // NOLINTBEGIN
            __m128i r = _mm_unpacklo_epi64(lane0_rgb.r_lane, lane1_rgb.r_lane);
            __m128i g = _mm_unpacklo_epi64(lane0_rgb.g_lane, lane1_rgb.g_lane);
            __m128i b = _mm_unpacklo_epi64(lane0_rgb.b_lane, lane1_rgb.b_lane);
            __m128i a = _mm_set1_epi8(ALPHA_BYTE_VALUE);
            // NOLINTEND

            __m128i bg_lo = _mm_unpacklo_epi8(b, g);
            __m128i bg_hi = _mm_unpackhi_epi8(b, g);
            __m128i ra_lo = _mm_unpacklo_epi8(r, a);
            __m128i ra_hi = _mm_unpackhi_epi8(r, a);

            __m128i bgra0 = _mm_unpacklo_epi16(bg_lo, ra_lo);
            __m128i bgra1 = _mm_unpackhi_epi16(bg_lo, ra_lo);
            __m128i bgra2 = _mm_unpacklo_epi16(bg_hi, ra_hi);
            __m128i bgra3 = _mm_unpackhi_epi16(bg_hi, ra_hi);

            uint8_t *output_pixel_ptr =
                rgb_row_ptr + (column_index * RGB_CHANNELS);

            // NOLINTBEGIN
            _mm_storeu_si128((__m128i *)(output_pixel_ptr + 0), bgra0);
            _mm_storeu_si128((__m128i *)(output_pixel_ptr + 16), bgra1);
            _mm_storeu_si128((__m128i *)(output_pixel_ptr + 32), bgra2);
            _mm_storeu_si128((__m128i *)(output_pixel_ptr + 48), bgra3);
            // NOLINTEND
        }
    }
}
