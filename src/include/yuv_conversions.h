#ifndef YUV_CONVERSIONS_H
#define YUV_CONVERSIONS_H

#include "common_types.h"

void convert_yuv_to_rgb(const unsigned char *__restrict yuv_frame_pointer,
                        unsigned char *__restrict rgb_frame_pointer,
                        struct FrameDimensions frame_dimensions);

void convert_yuv_to_gray(const unsigned char *__restrict yuv_frame_pointer,
                         unsigned char *__restrict gray_frame_pointer,
                         struct FrameDimensions frame_dimensions);

#endif  // YUV_CONVERSIONS_H
