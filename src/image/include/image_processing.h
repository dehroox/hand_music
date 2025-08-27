#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include "../../common/common_types.h"

void ImageProcessing_flip_rgb_horizontal(
    const unsigned char *source_rgb_buffer,
    unsigned char *destination_rgb_buffer,
    const FrameDimensions *frame_dimensions);

void ImageProcessing_expand_grayscale(const unsigned char *source_gray_buffer,
                                      unsigned char *destination_rgb_buffer,
                                      const FrameDimensions *frame_dimensions);

#endif  // IMAGE_PROCESSING_H
