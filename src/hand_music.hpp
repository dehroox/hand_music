#ifndef HAND_MUSIC_HPP
#define HAND_MUSIC_HPP

#include <sys/ioctl.h>

#include <array>
#include <cerrno>
#include <cstddef>

struct MemoryMappedBuffer {
    void* start_address;
    size_t length_bytes;
};

struct FrameDimensions {
    unsigned int width;
    unsigned int height;
    unsigned int stride_bytes;
};

namespace Constants {

// --- Constants ---
constexpr int K_MAX_RGB_VALUE = 255;
constexpr int K_U_OFFSET = 128;
constexpr int K_RGB_COMPONENTS = 3;

constexpr int RGB_MULTIPLIER = 1000;
constexpr int RED_V_MULTIPLIER = 1402;
constexpr int GREEN_U_MULTIPLIER = 344;
constexpr int GREEN_V_MULTIPLIER = 714;
constexpr int BLUE_U_MULTIPLIER = 1772;

}  // namespace Constants

namespace Utils {

// when an ioctl call is interrupted by a signal (EINTR), it should be retried.
// this function wraps the ioctl call in a loop to handle such interruptions.
static inline auto continually_retry_ioctl(int file_descriptor,
                                           unsigned long request,
                                           void* argument) -> int {
    int result = -1;
    while (true) {
        result = ioctl(file_descriptor, request, argument);
        if (result != -1 || errno != EINTR) {
            break;
        }
    }
    return result;
}

}  // namespace Utils

namespace V4l2Device {

constexpr unsigned int MAX_BUFFERS = 4;

struct Device {
    int file_descriptor;
    std::array<MemoryMappedBuffer, MAX_BUFFERS> mapped_buffers;
    unsigned int buffer_count;
};

auto open(const char* device_path) -> int;
void close_device(int file_descriptor);

auto select_highest_resolution(int video_file_descriptor) -> FrameDimensions;

auto configure_video_format(int video_file_descriptor,
                            FrameDimensions& frame_dimensions) -> bool;

auto setup_memory_mapped_buffers(Device& device, unsigned int buffer_count)
    -> bool;

void unmap_buffers(Device& device);

auto start_video_stream(int video_file_descriptor) -> bool;

void stop_video_stream(int video_file_descriptor);

}  // namespace V4l2Device

// clamps a value to the valid 8-bit RGB range (0-255).
static inline auto clamp_rgb_value(int value) -> unsigned char {
    auto unsigned_value = static_cast<unsigned char>(value);
    if (unsigned_value >
        static_cast<unsigned int>(Constants::K_MAX_RGB_VALUE)) {
        unsigned_value = Constants::K_MAX_RGB_VALUE;
    }
    return unsigned_value;
}

void convert_yuv_to_rgb(const unsigned char* __restrict yuv_frame_pointer,
                        unsigned char* __restrict rgb_frame_pointer,
                        FrameDimensions frame_dimensions);

void convert_yuv_to_gray(const unsigned char* __restrict yuv_frame_pointer,
                         unsigned char* __restrict gray_frame_pointer,
                         FrameDimensions frame_dimensions);

#endif  // HAND_MUSIC_HPP