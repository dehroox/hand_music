#ifndef IMAGE_CONVERSIONS_H
#define IMAGE_CONVERSIONS_H

#include "../../common/common_types.h"

void ImageConversions_convert_yuv_to_rgb(
    const unsigned char *__restrict yuv_frame_pointer,
    unsigned char *__restrict rgb_frame_pointer,
    const FrameDimensions *frame_dimensions);

void ImageConversions_convert_yuv_to_gray(
    const unsigned char *__restrict yuv_frame_pointer,
    unsigned char *__restrict gray_frame_pointer,
    const FrameDimensions *frame_dimensions);

#endif  // IMAGE_CONVERSIONS_H
