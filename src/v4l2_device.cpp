#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <thread>
#include <vector>

#include "hand_music.hpp"

namespace V4l2Device {

auto open(const char* device_path) -> int {
    const int file_descriptor = ::open(device_path, O_RDWR);
    if (file_descriptor == -1) {
        std::cerr << "Failed to open video device " << device_path << "\n";
    }
    return file_descriptor;
}

void close_device(int file_descriptor) { ::close(file_descriptor); }

auto select_highest_resolution(int video_file_descriptor) -> FrameDimensions {
    FrameDimensions frame_dimensions{
        .width = 0U, .height = 0U, .stride_bytes = 0U};
    v4l2_frmsizeenum frame_size{};
    frame_size.index = 0;
    frame_size.pixel_format = V4L2_PIX_FMT_YUYV;

    while (ioctl(video_file_descriptor, VIDIOC_ENUM_FRAMESIZES, &frame_size) !=
           -1) {
        if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            frame_dimensions.width = frame_size.stepwise.max_width;
            frame_dimensions.height = frame_size.stepwise.max_height;
            break;
        }
        if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            uint32_t area =
                frame_size.discrete.width * frame_size.discrete.height;
            uint32_t current_area =
                frame_dimensions.width * frame_dimensions.height;
            if (area > current_area) {
                frame_dimensions.width = frame_size.discrete.width;
                frame_dimensions.height = frame_size.discrete.height;
            }
        }
        ++frame_size.index;
    }

    std::cout << "Using highest supported resolution: "
              << frame_dimensions.width << "x" << frame_dimensions.height
              << " (Aspect ratio: "
              << static_cast<double>(frame_dimensions.width) /
                     frame_dimensions.height
              << ")\n";

    return frame_dimensions;
}

auto configure_video_format(int video_file_descriptor,
                            FrameDimensions& frame_dimensions) -> bool {
    v4l2_format video_format{};
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.width = frame_dimensions.width;
    video_format.fmt.pix.height = frame_dimensions.height;
    video_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    video_format.fmt.pix.field = V4L2_FIELD_NONE;

    if (Utils::continually_retry_ioctl(video_file_descriptor, VIDIOC_S_FMT,
                                       &video_format) == -1) {
        return false;
    }
    frame_dimensions.stride_bytes = video_format.fmt.pix.bytesperline;
    return true;
}

auto setup_memory_mapped_buffers(Device& device, unsigned int buffer_count)
    -> bool {
    int file_descriptor = device.file_descriptor;
    v4l2_requestbuffers request_buffers{};
    request_buffers.count = buffer_count;
    request_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request_buffers.memory = V4L2_MEMORY_MMAP;

    if (Utils::continually_retry_ioctl(file_descriptor, VIDIOC_REQBUFS,
                                       &request_buffers) == -1 ||
        request_buffers.count < 2) {
        return false;
    }

    device.buffer_count = request_buffers.count;
    auto& mapped_buffers_ref = device.mapped_buffers;

    auto map_buffer = [&](unsigned int buffer_index) -> bool {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = buffer_index;

        if (Utils::continually_retry_ioctl(file_descriptor, VIDIOC_QUERYBUF,
                                           &buffer) == -1) {
            return false;
        }

        mapped_buffers_ref.at(buffer_index).start_address =
            mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                 file_descriptor, buffer.m.offset);
        if (mapped_buffers_ref.at(buffer_index).start_address == MAP_FAILED) {
            return false;
        }

        mapped_buffers_ref.at(buffer_index).length_bytes = buffer.length;

        return Utils::continually_retry_ioctl(file_descriptor, VIDIOC_QBUF,
                                              &buffer) != -1;
    };

    std::vector<std::thread> threads;
    std::vector<bool> results(device.buffer_count, false);

    threads.reserve(device.buffer_count);
    for (unsigned int i = 0; i < device.buffer_count; ++i) {
        threads.emplace_back([&, i] { results[i] = map_buffer(i); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return std::ranges::all_of(results, [](bool success) { return success; });
}

void unmap_buffers(Device& device) {
    auto& mapped_buffers_ref = device.mapped_buffers;
    for (unsigned int i = 0; i < device.buffer_count; ++i) {
        munmap(mapped_buffers_ref.at(i).start_address,
               mapped_buffers_ref.at(i).length_bytes);
    }
}

auto start_video_stream(int video_file_descriptor) -> bool {
    v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return Utils::continually_retry_ioctl(video_file_descriptor,
                                          VIDIOC_STREAMON, &capture_type) != -1;
}

void stop_video_stream(int video_file_descriptor) {
    v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    Utils::continually_retry_ioctl(video_file_descriptor, VIDIOC_STREAMOFF,
                                   &capture_type);
}

}  // namespace V4l2Device
