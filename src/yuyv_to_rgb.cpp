#include <immintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "hand_music.hpp"

namespace Convert {

constexpr int PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING = 16;
constexpr int BYTES_PER_PIXEL_YUYV = 2;  // YUYV = 2 bytes/pixel
constexpr int YUYV_TOTAL_BYTES_PER_BLOCK =
    PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING * BYTES_PER_PIXEL_YUYV;  // 32 bytes
constexpr int RGB_CHANNELS_PER_PIXEL = 3;

constexpr int PIXELS_IN_YUYV_PAIR = 2;

constexpr int16_t U_CHANNEL_OFFSET = 128;
constexpr int16_t V_CHANNEL_OFFSET = 128;

constexpr int16_t MAX_RGB_COLOR_VALUE = 255;
constexpr int16_t LOW_BYTE_MASK = 0x00FF;

// fixed-point coefficients (Q8) approximating BT.601-ish values scaled by 256
constexpr int16_t RED_MULTIPLIER_FROM_V_CHANNEL = 359;    // ~1.402 * 256
constexpr int16_t GREEN_MULTIPLIER_FROM_U_CHANNEL = 88;   // ~0.344 * 256
constexpr int16_t GREEN_MULTIPLIER_FROM_V_CHANNEL = 183;  // ~0.714 * 256
constexpr int16_t BLUE_MULTIPLIER_FROM_U_CHANNEL = 454;   // ~1.772 * 256
constexpr int SCALE_SHIFT_FOR_FIXED_POINT = 8;

}  // namespace Convert

// there is magic here, probably not the fastest, but clang shuts up when this
// is how it is
void convert_yuyv_to_rgb(const uint8_t* __restrict yuyv_frame_pointer,
                         uint8_t* __restrict rgb_frame_pointer,
                         FrameDimensions frame_dimensions) {
    const auto frame_width_in_pixels =
        static_cast<std::size_t>(frame_dimensions.width);
    const auto frame_height_in_pixels =
        static_cast<std::size_t>(frame_dimensions.height);
    const auto frame_stride_in_bytes =
        static_cast<std::size_t>(frame_dimensions.stride_bytes);

    const __m256i zero_vector_16bit = _mm256_setzero_si256();
    const __m256i u_channel_offset_vector_16bit =
        _mm256_set1_epi16(Convert::U_CHANNEL_OFFSET);
    const __m256i v_channel_offset_vector_16bit =
        _mm256_set1_epi16(Convert::V_CHANNEL_OFFSET);

    const __m256i red_multiplier_from_v_vector_16bit =
        _mm256_set1_epi16(Convert::RED_MULTIPLIER_FROM_V_CHANNEL);
    const __m256i green_multiplier_from_u_vector_16bit =
        _mm256_set1_epi16(Convert::GREEN_MULTIPLIER_FROM_U_CHANNEL);
    const __m256i green_multiplier_from_v_vector_16bit =
        _mm256_set1_epi16(Convert::GREEN_MULTIPLIER_FROM_V_CHANNEL);
    const __m256i blue_multiplier_from_u_vector_16bit =
        _mm256_set1_epi16(Convert::BLUE_MULTIPLIER_FROM_U_CHANNEL);

    const __m256i low_byte_mask_vector_16bit =
        _mm256_set1_epi16(Convert::LOW_BYTE_MASK);
    const __m256i max_color_vector_16bit =
        _mm256_set1_epi16(Convert::MAX_RGB_COLOR_VALUE);

    for (std::size_t row_index_in_pixels = 0;
         row_index_in_pixels < frame_height_in_pixels; ++row_index_in_pixels) {
        const uint8_t* yuyv_row_pointer =
            yuyv_frame_pointer + (row_index_in_pixels * frame_stride_in_bytes);
        uint8_t* rgb_row_pointer =
            rgb_frame_pointer + (row_index_in_pixels * frame_width_in_pixels *
                                 Convert::RGB_CHANNELS_PER_PIXEL);

        for (std::size_t column_index_in_pixels = 0;
             column_index_in_pixels < frame_width_in_pixels;
             column_index_in_pixels +=
             Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING) {
            const std::size_t pixels_remaining_in_row =
                (column_index_in_pixels +
                     Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING <=
                 frame_width_in_pixels)
                    ? Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING
                    : (frame_width_in_pixels - column_index_in_pixels);

            __m256i yuyv_block_vector_256bit;
            const std::size_t byte_offset_for_current_block =
                column_index_in_pixels *
                static_cast<std::size_t>(Convert::BYTES_PER_PIXEL_YUYV);
            std::memset(&yuyv_block_vector_256bit, 0,
                        sizeof(yuyv_block_vector_256bit));

            if (pixels_remaining_in_row * Convert::BYTES_PER_PIXEL_YUYV >=
                sizeof(yuyv_block_vector_256bit)) {
                std::memcpy(&yuyv_block_vector_256bit,
                            yuyv_row_pointer + byte_offset_for_current_block,
                            sizeof(yuyv_block_vector_256bit));
            } else {
                std::memcpy(
                    &yuyv_block_vector_256bit,
                    yuyv_row_pointer + byte_offset_for_current_block,
                    pixels_remaining_in_row * Convert::BYTES_PER_PIXEL_YUYV);
            }

            const __m256i y_values_vector_16bit = _mm256_and_si256(
                yuyv_block_vector_256bit, low_byte_mask_vector_16bit);
            const __m256i uv_values_vector_16bit = _mm256_srli_epi16(
                yuyv_block_vector_256bit, Convert::SCALE_SHIFT_FOR_FIXED_POINT);

            alignas(Convert::YUYV_TOTAL_BYTES_PER_BLOCK)
                std::array<int16_t,
                           Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING>
                    uv_values_array{};
            std::memcpy(uv_values_array.data(), &uv_values_vector_16bit,
                        sizeof(uv_values_vector_16bit));

            alignas(Convert::YUYV_TOTAL_BYTES_PER_BLOCK)
                std::array<int16_t,
                           Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING>
                    u_values_array{};
            alignas(Convert::YUYV_TOTAL_BYTES_PER_BLOCK)
                std::array<int16_t,
                           Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING>
                    v_values_array{};
            for (int pair_index = 0;
                 pair_index < Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING;
                 pair_index += Convert::PIXELS_IN_YUYV_PAIR) {
                const int16_t u_value_for_pair =
                    *(uv_values_array.data() +
                      static_cast<std::ptrdiff_t>(pair_index));
                const int16_t v_value_for_pair =
                    *(uv_values_array.data() +
                      static_cast<std::ptrdiff_t>(pair_index + 1));

                *(u_values_array.data() +
                  static_cast<std::ptrdiff_t>(pair_index)) = u_value_for_pair;
                *(u_values_array.data() +
                  static_cast<std::ptrdiff_t>(pair_index + 1)) =
                    u_value_for_pair;
                *(v_values_array.data() +
                  static_cast<std::ptrdiff_t>(pair_index)) = v_value_for_pair;
                *(v_values_array.data() +
                  static_cast<std::ptrdiff_t>(pair_index + 1)) =
                    v_value_for_pair;
            }

            __m256i u_values_vector_16bit;
            __m256i v_values_vector_16bit;
            std::memcpy(&u_values_vector_16bit, u_values_array.data(),
                        sizeof(u_values_vector_16bit));
            std::memcpy(&v_values_vector_16bit, v_values_array.data(),
                        sizeof(v_values_vector_16bit));

            u_values_vector_16bit = _mm256_sub_epi16(
                u_values_vector_16bit, u_channel_offset_vector_16bit);
            v_values_vector_16bit = _mm256_sub_epi16(
                v_values_vector_16bit, v_channel_offset_vector_16bit);

            const __m256i y_values_signed_vector_16bit = y_values_vector_16bit;

            __m256i red_values_vector_16bit = _mm256_add_epi16(
                y_values_signed_vector_16bit,
                _mm256_srai_epi16(
                    _mm256_mullo_epi16(v_values_vector_16bit,
                                       red_multiplier_from_v_vector_16bit),
                    Convert::SCALE_SHIFT_FOR_FIXED_POINT));

            __m256i green_values_vector_16bit = _mm256_sub_epi16(
                y_values_signed_vector_16bit,
                _mm256_srai_epi16(
                    _mm256_add_epi16(_mm256_mullo_epi16(
                                         u_values_vector_16bit,
                                         green_multiplier_from_u_vector_16bit),
                                     _mm256_mullo_epi16(
                                         v_values_vector_16bit,
                                         green_multiplier_from_v_vector_16bit)),
                    Convert::SCALE_SHIFT_FOR_FIXED_POINT));

            __m256i blue_values_vector_16bit = _mm256_add_epi16(
                y_values_signed_vector_16bit,
                _mm256_srai_epi16(
                    _mm256_mullo_epi16(u_values_vector_16bit,
                                       blue_multiplier_from_u_vector_16bit),
                    Convert::SCALE_SHIFT_FOR_FIXED_POINT));

            red_values_vector_16bit = _mm256_max_epi16(
                zero_vector_16bit, _mm256_min_epi16(red_values_vector_16bit,
                                                    max_color_vector_16bit));
            green_values_vector_16bit = _mm256_max_epi16(
                zero_vector_16bit, _mm256_min_epi16(green_values_vector_16bit,
                                                    max_color_vector_16bit));
            blue_values_vector_16bit = _mm256_max_epi16(
                zero_vector_16bit, _mm256_min_epi16(blue_values_vector_16bit,
                                                    max_color_vector_16bit));

            alignas(Convert::YUYV_TOTAL_BYTES_PER_BLOCK)
                std::array<int16_t,
                           Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING>
                    red_values_array{};
            alignas(Convert::YUYV_TOTAL_BYTES_PER_BLOCK)
                std::array<int16_t,
                           Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING>
                    green_values_array{};
            alignas(Convert::YUYV_TOTAL_BYTES_PER_BLOCK)
                std::array<int16_t,
                           Convert::PIXELS_PER_BLOCK_FOR_SIMD_PROCESSING>
                    blue_values_array{};

            std::memcpy(red_values_array.data(), &red_values_vector_16bit,
                        sizeof(red_values_vector_16bit));
            std::memcpy(green_values_array.data(), &green_values_vector_16bit,
                        sizeof(green_values_vector_16bit));
            std::memcpy(blue_values_array.data(), &blue_values_vector_16bit,
                        sizeof(blue_values_vector_16bit));

            uint8_t* output_pixel_pointer_base =
                rgb_row_pointer +
                (column_index_in_pixels * Convert::RGB_CHANNELS_PER_PIXEL);
            for (std::size_t pixel_index = 0;
                 pixel_index <
                 static_cast<std::size_t>(pixels_remaining_in_row);
                 ++pixel_index) {
                uint8_t* output_pixel_pointer =
                    output_pixel_pointer_base +
                    (pixel_index * Convert::RGB_CHANNELS_PER_PIXEL);
                const int16_t red_value_for_pixel =
                    *(red_values_array.data() +
                      static_cast<std::ptrdiff_t>(pixel_index));
                const int16_t green_value_for_pixel =
                    *(green_values_array.data() +
                      static_cast<std::ptrdiff_t>(pixel_index));
                const int16_t blue_value_for_pixel =
                    *(blue_values_array.data() +
                      static_cast<std::ptrdiff_t>(pixel_index));

                *output_pixel_pointer =
                    static_cast<uint8_t>(red_value_for_pixel);
                *(output_pixel_pointer + 1) =
                    static_cast<uint8_t>(green_value_for_pixel);
                *(output_pixel_pointer + 2) =
                    static_cast<uint8_t>(blue_value_for_pixel);
            }
        }
    }
}
