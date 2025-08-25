#ifndef V4L2_DEVICE_HPP
#define V4L2_DEVICE_HPP

#include <vector>

#include "common_types.hpp"

namespace V4l2Device {

auto open(const char* device_path) -> int;
void close_device(int file_descriptor);

auto select_highest_resolution(int video_file_descriptor) -> FrameDimensions;

auto configure_video_format(int video_file_descriptor,
                            FrameDimensions& frame_dimensions) -> bool;

auto setup_memory_mapped_buffers(
    int video_file_descriptor, std::vector<MemoryMappedBuffer>& mapped_buffers,
    unsigned int buffer_count) -> bool;

void unmap_buffers(std::vector<MemoryMappedBuffer>& mapped_buffers);

auto start_video_stream(int video_file_descriptor) -> bool;

void stop_video_stream(int video_file_descriptor);

}  // namespace V4l2Device

#endif  // V4L2_DEVICE_HPP
