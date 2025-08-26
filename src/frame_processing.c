#define _POSIX_C_SOURCE 200809L

#include "include/frame_processing.h"

#include <time.h>

#include "include/constants.h"

void FrameProcessing_flip_rgb_horizontal(
    const unsigned char *source_rgb_buffer,
    unsigned char *destination_rgb_buffer,
    struct FrameDimensions frame_dimensions) {
    const unsigned int number_of_bytes_per_pixel_in_rgba_format = 4;
    const size_t number_of_bytes_per_row_of_pixels =
        (size_t)frame_dimensions.width *
        number_of_bytes_per_pixel_in_rgba_format;

    for (unsigned int current_row_index = 0;
         current_row_index < frame_dimensions.height; ++current_row_index) {
        const unsigned char *source_row_pointer_for_current_row =
            source_rgb_buffer +
            ((size_t)current_row_index * number_of_bytes_per_row_of_pixels);

        unsigned char *destination_row_pointer_for_current_row_flipped =
            destination_rgb_buffer +
            ((size_t)current_row_index * number_of_bytes_per_row_of_pixels);

        for (unsigned int current_column_index = 0;
             current_column_index < frame_dimensions.width;
             ++current_column_index) {
            const unsigned int source_pixel_column_index = current_column_index;
            const unsigned int destination_pixel_column_index_flipped =
                frame_dimensions.width - 1 - current_column_index;

            const unsigned char *source_pixel_pointer =
                source_row_pointer_for_current_row +
                ((size_t)source_pixel_column_index *
                 number_of_bytes_per_pixel_in_rgba_format);

            unsigned char *destination_pixel_pointer_flipped =
                destination_row_pointer_for_current_row_flipped +
                ((size_t)destination_pixel_column_index_flipped *
                 number_of_bytes_per_pixel_in_rgba_format);

            destination_pixel_pointer_flipped[0] = source_pixel_pointer[0];
            destination_pixel_pointer_flipped[1] = source_pixel_pointer[1];
            destination_pixel_pointer_flipped[2] = source_pixel_pointer[2];
            destination_pixel_pointer_flipped[3] = source_pixel_pointer[3];
        }
    }
}

long long FrameProcessing_measure_conversion_time(
    void (*convert_func)(const unsigned char *, unsigned char *,
                         struct FrameDimensions),
    const unsigned char *source_frame, unsigned char *destination_frame,
    struct FrameDimensions frame_dimensions) {
    struct timespec start_time;
    struct timespec end_time;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    convert_func(source_frame, destination_frame, frame_dimensions);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    return ((end_time.tv_sec - start_time.tv_sec) * MICROSECONDS_IN_SECOND) +
           ((end_time.tv_nsec - start_time.tv_nsec) /
            NANOSECONDS_IN_MICROSECOND);
}
