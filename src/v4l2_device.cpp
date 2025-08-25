#include "v4l2_device.hpp"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <vector>

#include "utils.hpp"

namespace V4l2Device {

auto open(const char* device_path) -> int {
    const int FILE_DESCRIPTOR = ::open(device_path, O_RDWR);
    if (FILE_DESCRIPTOR == -1) {
        std::cerr << "Failed to open video device " << device_path << "\n";
        return -1;
    }
    return FILE_DESCRIPTOR;
}

void close_device(int file_descriptor) { ::close(file_descriptor); }

// queries the device for all supported YUYV resolutions and selects the
// largest one.
auto select_highest_resolution(int video_file_descriptor) -> FrameDimensions {
    FrameDimensions frame_dimensions{
        .width = 0U, .height = 0U, .stride_bytes = 0U};

    v4l2_fmtdesc format_description{};
    format_description.index = 0;
    format_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // iterate through all available formats.
    while (ioctl(video_file_descriptor, VIDIOC_ENUM_FMT, &format_description) !=
           -1) {
        // we are only interested in the YUYV format.
        if (format_description.pixelformat != V4L2_PIX_FMT_YUYV) {
            ++format_description.index;
            continue;
        }

        v4l2_frmsizeenum frame_size{};
        frame_size.index = 0;
        frame_size.pixel_format = format_description.pixelformat;

        while (ioctl(video_file_descriptor, VIDIOC_ENUM_FRAMESIZES,
                     &frame_size) != -1) {
            if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                if (frame_size.discrete.width * frame_size.discrete.height >
                    frame_dimensions.width * frame_dimensions.height) {
                    frame_dimensions.width = frame_size.discrete.width;
                    frame_dimensions.height = frame_size.discrete.height;
                }
            } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                // for stepwise, just select the maximum supported resolution.
                frame_dimensions.width = frame_size.stepwise.max_width;
                frame_dimensions.height = frame_size.stepwise.max_height;
            }
            ++frame_size.index;
        }
        ++format_description.index;
    }

    std::cout << "Using highest supported resolution: "
              << frame_dimensions.width << "x" << frame_dimensions.height
              << " (Aspect ratio: "
              << double(frame_dimensions.width) / frame_dimensions.height
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
        std::cerr << "VIDIOC_S_FMT failed\n";
        return false;
    }

    frame_dimensions.stride_bytes = video_format.fmt.pix.bytesperline;
    return true;
}

// requests buffers from the device, maps them into application memory, and
// queues them for capturing.
auto setup_memory_mapped_buffers(
    int video_file_descriptor, std::vector<MemoryMappedBuffer>& mapped_buffers,
    unsigned int buffer_count) -> bool {
    v4l2_requestbuffers request_buffers{};
    request_buffers.count = buffer_count;
    request_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request_buffers.memory = V4L2_MEMORY_MMAP;

    // request the buffers from the driver.
    if (Utils::continually_retry_ioctl(video_file_descriptor, VIDIOC_REQBUFS,
                                       &request_buffers) == -1 ||
        request_buffers.count < 2) {
        std::cerr << "VIDIOC_REQBUFS failed\n";
        return false;
    }

    mapped_buffers.reserve(request_buffers.count);

    for (unsigned int buffer_index = 0; buffer_index < request_buffers.count;
         ++buffer_index) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = buffer_index;

        // query buffer information.
        if (Utils::continually_retry_ioctl(video_file_descriptor,
                                           VIDIOC_QUERYBUF, &buffer) == -1) {
            std::cerr << "VIDIOC_QUERYBUF failed\n";
            return false;
        }

        // map the buffer into the application's address space.
        void* start_address =
            mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                 video_file_descriptor, buffer.m.offset);
        if (start_address == MAP_FAILED) {
            std::cerr << "Memory mapping failed\n";
            return false;
        }

        mapped_buffers.push_back({start_address, buffer.length});

        // queue the buffer for capturing.
        if (Utils::continually_retry_ioctl(video_file_descriptor, VIDIOC_QBUF,
                                           &buffer) == -1) {
            std::cerr << "VIDIOC_QBUF failed\n";
            return false;
        }
    }

    return true;
}

void unmap_buffers(std::vector<MemoryMappedBuffer>& mapped_buffers) {
    for (auto& buffer : mapped_buffers) {
        munmap(buffer.start_address, buffer.length_bytes);
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
