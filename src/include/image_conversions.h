#ifndef IMAGE_CONVERSIONS_H
#define IMAGE_CONVERSIONS_H

#include "common_types.h"

void convert_yuv_to_rgb(const unsigned char *__restrict yuv_frame_pointer,
                        unsigned char *__restrict rgb_frame_pointer,
                        struct FrameDimensions frame_dimensions);

void convert_yuv_to_gray(const unsigned char *__restrict yuv_frame_pointer,
                         unsigned char *__restrict gray_frame_pointer,
                         struct FrameDimensions frame_dimensions);

void convert_yuyv_to_gray_avx2(const unsigned char *yuyv_buffer,
                               unsigned char *gray_buffer,
                               struct FrameDimensions frame_dimensions);

void convert_yuyv_to_rgb_avx2(const unsigned char *yuyv_buffer,
                              unsigned char *rgb_buffer,
                              struct FrameDimensions frame_dimensions);

#endif  // IMAGE_CONVERSIONS_H
