#include <stdio.h>
#include <stdlib.h>

#include "branch.h"
#include "capture.h"
#include "rgb.h"
#include "types.h"
#include "window.h"
#include "yuyv.h"

#define DEVICE_PATH "/dev/video0"

static const unsigned short int FRAME_WIDTH = 640;
static const unsigned short int FRAME_HEIGHT = 480;

int main(void) {
    FrameDimensions dimensions = {
	.width = FRAME_WIDTH,
	.height = FRAME_HEIGHT,
	.stride = FRAME_WIDTH * 2,  // YUYV format is 2 bytes per pixel
	.pixels = FRAME_WIDTH * FRAME_HEIGHT};

    CaptureDevice captureDevice = {0};
    ErrorCode capture_err = ERROR_NONE;
    WindowState windowState = {0};
    ErrorCode window_err = ERROR_NONE;
    unsigned char *rgbBuffer = NULL;
    unsigned char *flippedRgbBuffer = NULL;
    unsigned char *yuyvFrame = NULL;
    bool quit = false;

    capture_err = CaptureDevice_open(&captureDevice, DEVICE_PATH, dimensions);
    if (UNLIKELY(capture_err != ERROR_NONE)) {
	(void)fprintf(stderr,
		      "Failed to open capture device %s: ErrorCode %d\n",
		      DEVICE_PATH, capture_err);
	goto cleanup;
    }

    window_err = Window_create(&windowState, "Hand Music", dimensions);
    if (UNLIKELY(window_err != ERROR_NONE)) {
	(void)fprintf(stderr, "Failed to create window: ErrorCode %d\n",
		      window_err);
	goto cleanup;
    }

    rgbBuffer = (unsigned char *)malloc((size_t)FRAME_WIDTH * FRAME_HEIGHT * 4);
    if (UNLIKELY(rgbBuffer == NULL)) {
	(void)fprintf(stderr, "Failed to allocate RGB buffer\n");
	goto cleanup;
    }

    flippedRgbBuffer =
	(unsigned char *)malloc((size_t)FRAME_WIDTH * FRAME_HEIGHT * 4);
    if (UNLIKELY(flippedRgbBuffer == NULL)) {
	(void)fprintf(stderr, "Failed to allocate flipped RGB buffer\n");
	goto cleanup;
    }

    while (!quit) {
	yuyvFrame = CaptureDevice_getFrame(&captureDevice);
	if (yuyvFrame == NULL) {
	    (void)fprintf(stderr, "Failed to get frame from capture device\n");
	    quit = true;
	    continue;
	}

	ErrorCode process_err = yuyvToRgb(yuyvFrame, rgbBuffer, &dimensions);
	if (process_err != ERROR_NONE) {
	    (void)fprintf(stderr,
			  "Failed to convert YUYV to RGB: ErrorCode %d\n",
			  process_err);
	    quit = true;
	    continue;
	}

	process_err =
	    flipRgbHorizontal(rgbBuffer, flippedRgbBuffer, &dimensions);
	if (process_err != ERROR_NONE) {
	    (void)fprintf(stderr,
			  "Failed to flip RGB horizontally: ErrorCode %d\n",
			  process_err);
	    quit = true;
	    continue;
	}
	Window_draw(&windowState, flippedRgbBuffer);

	quit = Window_pollEvents(&windowState);
    }

cleanup:
    if (LIKELY(flippedRgbBuffer != NULL)) {
	free(flippedRgbBuffer);
    }
    if (LIKELY(rgbBuffer != NULL)) {
	free(rgbBuffer);
    }

    if (LIKELY(window_err == ERROR_NONE)) {
	Window_destroy(&windowState);
    }

    if (LIKELY(capture_err == ERROR_NONE)) {
	CaptureDevice_close(&captureDevice);
    }

    return (capture_err != ERROR_NONE && window_err != ERROR_NONE &&
	    rgbBuffer == NULL && flippedRgbBuffer == NULL);
}
