#define _POSIX_C_SOURCE 200809L

#include "include/frame_processing.h"

#include <immintrin.h>
#include <stddef.h>
#include <time.h>

#include "../common/constants.h"

void FrameProcessing_flip_rgb_horizontal(
    const unsigned char *source_rgb_buffer,
    unsigned char *destination_rgb_buffer,
    struct FrameDimensions *frame_dimensions) {
    const unsigned int bytes_per_pixel = RGB_CHANNELS;
    const size_t bytes_per_row =
        (size_t)frame_dimensions->width * bytes_per_pixel;

    for (unsigned int row_index = 0; row_index < frame_dimensions->height;
         ++row_index) {
        const unsigned char *source_row_ptr =
            source_rgb_buffer + (row_index * bytes_per_row);
        unsigned char *dest_row_ptr =
            destination_rgb_buffer + (row_index * bytes_per_row);

        unsigned int num_sse_blocks =
            frame_dimensions->width / PIXELS_PER_SSE_BLOCK;
        for (unsigned int block_idx = 0; block_idx < num_sse_blocks;
             ++block_idx) {
            unsigned int source_col_start = block_idx * PIXELS_PER_SSE_BLOCK;
            unsigned int dest_col_start_flipped = frame_dimensions->width -
                                                  PIXELS_PER_SSE_BLOCK -
                                                  source_col_start;

            const __m128i *src_vec_ptr =
                (const __m128i *)(source_row_ptr + ((size_t)(source_col_start *
                                                             bytes_per_pixel)));
            __m128i *dest_vec_ptr =
                (__m128i *)(dest_row_ptr + ((size_t)(dest_col_start_flipped *
                                                     bytes_per_pixel)));

            __m128i loaded_pixels = _mm_loadu_si128(src_vec_ptr);
            __m128i flipped_pixels =
                _mm_shuffle_epi32(loaded_pixels, _MM_SHUFFLE(0, 1, 2, 3));

            _mm_storeu_si128(dest_vec_ptr, flipped_pixels);
        }
    }
}

long long FrameProcessing_measure_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         struct FrameDimensions *),
    const unsigned char *source_frame, unsigned char *destination_frame,
    struct FrameDimensions *frame_dimensions) {
    struct timespec start_time;
    struct timespec end_time;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    convert_func(source_frame, destination_frame, frame_dimensions);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    return ((end_time.tv_sec - start_time.tv_sec) * MICROSECONDS_IN_SECOND) +
           ((end_time.tv_nsec - start_time.tv_nsec) /
            NANOSECONDS_IN_MICROSECOND);
}
