#ifndef YUYV_TO_RGB_HPP
#define YUYV_TO_RGB_HPP

#include "common_types.hpp"

void convert_yuyv_to_rgb(const unsigned char* yuyv_frame_pointer,
                         unsigned char* rgb_frame_pointer,
                         FrameDimensions frame_dimensions);

#endif  // YUYV_TO_RGB_HPP
