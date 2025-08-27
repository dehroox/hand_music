#ifndef RECOGNITION_THREAD_H
#define RECOGNITION_THREAD_H

#include "common_types.h"
typedef struct {
    unsigned char *processed_gray_frame_buffer;
    FrameDimensions frame_dimensions;
} RecognitionThreadArguments;

void *RecognitionThread_run(void *arguments);

#endif
