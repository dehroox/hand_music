#pragma once

#include "types.h"

ErrorCode boxBlurGray(const unsigned char* grayInput,
                      unsigned char* blurredOutput,
                      const FrameDimensions* dimensions);
