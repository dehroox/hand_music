#pragma once
#include <stddef.h>

#include "types.h"

typedef struct {
    int file_descriptor;
    unsigned char *buffer;
    size_t buffer_size;
    FrameDimensions dimensions;
} __attribute__((aligned(64))) CaptureDevice;

ErrorCode CaptureDevice_open(CaptureDevice *device, const char *devicePath,
			     FrameDimensions dimensions);
void CaptureDevice_close(CaptureDevice *device);
unsigned char *CaptureDevice_getFrame(const CaptureDevice *device);
