#include <stdint.h>
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <immintrin.h>
#include <stddef.h>

#include "../common/constants.h"
#include "include/image_processing.h"

#define PIXELS_PER_SSE_BLOCK 4U

void ImageProcessing_flip_rgb_horizontal(const unsigned char *source_rgb_buffer,
                                         unsigned char *destination_rgb_buffer,
                                         FrameDimensions *frame_dimensions) {
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

void ImageProcessing_expand_grayscale(const unsigned char *source_gray_buffer,
                                      unsigned char *destination_rgb_buffer,
                                      FrameDimensions *frame_dimensions) {
    const size_t total_pixels =
        (size_t)frame_dimensions->width * frame_dimensions->height;
    const __m128i alpha_val_vec = _mm_set1_epi8((char)ALPHA_BYTE_VALUE);

    assert(frame_dimensions->width % 16 == 0 &&
           "Width must be a multiple of 16 for SIMD optimization.");

    for (size_t i = 0; i < total_pixels; i += 16) {  // NOLINT
        // Load 16 gray pixels (16 bytes)
        __m128i gray_data =
            _mm_loadu_si128((const __m128i *)(source_gray_buffer + i));

        __m128i r_channel = gray_data;
        __m128i g_channel = gray_data;
        __m128i b_channel = gray_data;

        __m128i bg_lo = _mm_unpacklo_epi8(b_channel, g_channel);
        __m128i bg_hi = _mm_unpackhi_epi8(b_channel, g_channel);

        __m128i ra_lo = _mm_unpacklo_epi8(r_channel, alpha_val_vec);
        __m128i ra_hi = _mm_unpackhi_epi8(r_channel, alpha_val_vec);

        __m128i bgra0 = _mm_unpacklo_epi16(bg_lo, ra_lo);
        __m128i bgra1 = _mm_unpackhi_epi16(bg_lo, ra_lo);
        __m128i bgra2 = _mm_unpacklo_epi16(bg_hi, ra_hi);
        __m128i bgra3 = _mm_unpackhi_epi16(bg_hi, ra_hi);

        // NOLINTBEGIN
        uint8_t *output_pixel_ptr = destination_rgb_buffer + (i * 4);
        _mm_storeu_si128((__m128i *)(output_pixel_ptr + 0), bgra0);
        _mm_storeu_si128((__m128i *)(output_pixel_ptr + 16), bgra1);
        _mm_storeu_si128((__m128i *)(output_pixel_ptr + 32), bgra2);
        _mm_storeu_si128((__m128i *)(output_pixel_ptr + 48), bgra3);
        // NOLINTEND
    }
}
