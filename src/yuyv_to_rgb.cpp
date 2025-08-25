#include "yuyv_to_rgb.hpp"

#include <cstddef>
#include <cstdint>

#include "constants.hpp"

namespace {

inline auto clamp_rgb_value(int value) -> unsigned char {
    auto unsigned_value = static_cast<unsigned int>(value);
    if (unsigned_value >
        static_cast<unsigned int>(Constants::K_MAX_RGB_VALUE)) {
        unsigned_value = Constants::K_MAX_RGB_VALUE;
    }
    return static_cast<unsigned char>(unsigned_value);
}

}  // namespace

void convert_yuyv_to_rgb(const unsigned char* yuyv_frame_pointer,
                         unsigned char* rgb_frame_pointer,
                         FrameDimensions frame_dimensions) {
    const auto* __restrict__ yuyv_data = yuyv_frame_pointer;
    auto* __restrict__ rgb_data = rgb_frame_pointer;

    const int64_t red_v_mult = Constants::RED_V_MULTIPLIER;
    const int64_t green_u_mult = Constants::GREEN_U_MULTIPLIER;
    const int64_t green_v_mult = Constants::GREEN_V_MULTIPLIER;
    const int64_t blue_u_mult = Constants::BLUE_U_MULTIPLIER;
    const int64_t rgb_mul = Constants::RGB_MULTIPLIER;

    for (unsigned int row_index = 0; row_index < frame_dimensions.height;
         ++row_index) {
        const unsigned char* yuyv_row_start =
            yuyv_data +
            (static_cast<size_t>(row_index * frame_dimensions.stride_bytes));
        unsigned int pixel_index_in_row = row_index * frame_dimensions.width;

        for (unsigned int column_index = 0;
             column_index + 1 < frame_dimensions.width;
             column_index += 2, pixel_index_in_row += 2) {
            const unsigned int yuyv_byte_offset = column_index * 2;

            const int y_pixel0 = yuyv_row_start[yuyv_byte_offset + 0];
            const int u_component =
                yuyv_row_start[yuyv_byte_offset + 1] - Constants::K_U_OFFSET;
            const int y_pixel1 = yuyv_row_start[yuyv_byte_offset + 2];
            const int v_component =
                yuyv_row_start[yuyv_byte_offset + 3] - Constants::K_U_OFFSET;

            const int64_t red_contribution = red_v_mult * v_component;
            const int64_t green_contribution =
                (green_u_mult * u_component) + (green_v_mult * v_component);
            const int64_t blue_contribution = blue_u_mult * u_component;

            const unsigned int rgb_pixel_base_index =
                pixel_index_in_row * Constants::K_RGB_COMPONENTS;

            rgb_data[rgb_pixel_base_index + 0] = clamp_rgb_value(
                int((y_pixel0 * rgb_mul + red_contribution) / rgb_mul));
            rgb_data[rgb_pixel_base_index + 1] = clamp_rgb_value(
                int((y_pixel0 * rgb_mul - green_contribution) / rgb_mul));
            rgb_data[rgb_pixel_base_index + 2] = clamp_rgb_value(
                int((y_pixel0 * rgb_mul + blue_contribution) / rgb_mul));

            rgb_data[rgb_pixel_base_index + 3] = clamp_rgb_value(
                int((y_pixel1 * rgb_mul + red_contribution) / rgb_mul));
            rgb_data[rgb_pixel_base_index + 4] =
                clamp_rgb_value(int((y_pixel1 * rgb_mul - green_contribution) /
                                    rgb_mul));  // NOLINTNEXTLINE
            rgb_data[rgb_pixel_base_index + 5] = clamp_rgb_value(
                int((y_pixel1 * rgb_mul + blue_contribution) / rgb_mul));
        }
    }
}
