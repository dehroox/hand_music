#pragma once

#include "types.h"

void boxBlurGray(const unsigned char* grayInput, unsigned char* blurredOutput,
                 FrameDimensions dimensions);
