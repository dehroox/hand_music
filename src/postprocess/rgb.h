#pragma once

#include "types.h"

ErrorCode flipRgbHorizontal(const unsigned char *rgbBuffer,
                       unsigned char *destBuffer,
                       const FrameDimensions *frame_dimensions);
