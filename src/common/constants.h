#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MAX_RGB_VALUE 255
#define V4L2_MAX_BUFFERS 4

#define VIDEO_DEVICE_PATH "/dev/video0"
#define MICROSECONDS_IN_SECOND 1000000LL
#define NANOSECONDS_IN_MICROSECOND 1000LL

// x11
#define WINDOW_BORDER_WIDTH 1
#define WINDOW_POSITION_X 10
#define WINDOW_POSITION_Y 10
#define BITMAP_PAD 32

// v4l2
#define MINIMUM_BUFFER_COUNT 2
#define PIXEL_FORMAT V4L2_PIX_FMT_YUYV
#define VIDEO_CAPTURE_TYPE V4L2_BUF_TYPE_VIDEO_CAPTURE

// YUYV to Grayscale
#define PIXELS_PER_AVX2_BLOCK 16U
#define PIXELS_PER_SSE_BLOCK 4U
#define BYTES_PER_YUYV_PIXEL 2U
#define SHUFFLE_INVALID_BYTE -128

// YUYV to RGB
#define PIXELS_PER_SIMD_BLOCK 16
#define TOTAL_BYTES_PER_SIMD_BLOCK \
    (PIXELS_PER_SIMD_BLOCK * BYTES_PER_YUYV_PIXEL)
#define RGB_CHANNELS 4
#define ALPHA_BYTE_VALUE -1
#define U_CHANNEL_OFFSET 128
#define V_CHANNEL_OFFSET 128
#define SHIFT_FIXED_POINT 8
#define RED_FROM_V 359
#define GREEN_FROM_U 88
#define GREEN_FROM_V 183
#define BLUE_FROM_U 454

#define SHUFFLE_YUV_MASK \
    _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15)
#define DUPLICATE_U_MASK                                                      \
    _mm_setr_epi8(0, 0, 2, 2, 4, 4, 6, 6, (char)0x80, (char)0x80, (char)0x80, \
                  (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80)
#define DUPLICATE_V_MASK                                                      \
    _mm_setr_epi8(1, 1, 3, 3, 5, 5, 7, 7, (char)0x80, (char)0x80, (char)0x80, \
                  (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80)

#endif  // CONSTANTS_H
