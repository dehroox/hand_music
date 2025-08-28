#include <stdio.h>
#include <stdlib.h>

#include "capture/capture.h"
#include "common/types.h"
#include "display/window.h"
#include "postprocess/rgb.h"
#include "postprocess/yuyv.h"

#define DEVICE_PATH "/dev/video0"

static const unsigned short int FRAME_WIDTH = 640;
static const unsigned short int FRAME_HEIGHT = 480;
static const unsigned short int FRAME_STRIDE = 640 * 2;

int main(void) {
    FrameDimensions dimensions = {.width = FRAME_WIDTH,
                                  .height = FRAME_HEIGHT,
                                  .stride = FRAME_STRIDE,
                                  .pixels = FRAME_WIDTH * FRAME_HEIGHT};

    CaptureDevice captureDevice;
    ErrorCode capture_err =
        CaptureDevice_open(&captureDevice, DEVICE_PATH, dimensions);

    if (capture_err != ERROR_NONE) {
        (void)fprintf(stderr,
                      "Failed to open capture device %s: ErrorCode %d\n",
                      DEVICE_PATH, capture_err);
        return EXIT_FAILURE;
    }

    WindowState windowState;
    ErrorCode window_err =
        Window_create(&windowState, "Hand Music", dimensions);

    if (window_err != ERROR_NONE) {
        (void)fprintf(stderr, "Failed to create window: ErrorCode %d\n",
                      window_err);
        CaptureDevice_close(&captureDevice);
        return EXIT_FAILURE;
    }

    unsigned char *rgbBuffer =
        (unsigned char *)malloc((unsigned long)FRAME_WIDTH * FRAME_HEIGHT * 4);

    if (rgbBuffer == NULL) {
        (void)fprintf(stderr, "Failed to allocate RGB buffer\n");
        Window_destroy(&windowState);
        CaptureDevice_close(&captureDevice);
        return EXIT_FAILURE;
    }

    unsigned char *flippedRgbBuffer =
        (unsigned char *)malloc((unsigned long)FRAME_WIDTH * FRAME_HEIGHT * 4);
        
    if (flippedRgbBuffer == NULL) {
        (void)fprintf(stderr, "Failed to allocate flipped RGB buffer\n");
        free(rgbBuffer);
        Window_destroy(&windowState);
        CaptureDevice_close(&captureDevice);
        return EXIT_FAILURE;
    }

    bool quit = false;
    while (!quit) {
        unsigned char *yuyvFrame = CaptureDevice_getFrame(&captureDevice);
        if (yuyvFrame == NULL) {
            (void)fprintf(stderr, "Failed to get frame from capture device\n");
            quit = true;
            continue;
        }

        yuyvToRgb(yuyvFrame, rgbBuffer, &dimensions);
        flipRgbHorizontal(rgbBuffer, flippedRgbBuffer, &dimensions);
        Window_draw(&windowState, flippedRgbBuffer);

        quit = Window_pollEvents(&windowState);
    }

    free(rgbBuffer);
    free(flippedRgbBuffer);
    Window_destroy(&windowState);
    CaptureDevice_close(&captureDevice);

    return EXIT_SUCCESS;
}
