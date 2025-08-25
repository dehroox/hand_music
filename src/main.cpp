#include <linux/videodev2.h>

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPixmap>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "common_types.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include "v4l2_device.hpp"
#include "yuyv_to_rgb.hpp"

auto main(int argc, char** argv) -> int {
    QApplication app(argc, argv);

    const int video_file_descriptor = V4l2Device::open("/dev/video0");
    if (video_file_descriptor == -1) {
        return 1;
    }

    // select the highest available resolution
    // (that complies with the camera's native aspect ratio)
    // and configure the video format.
    FrameDimensions frame_dimensions =
        V4l2Device::select_highest_resolution(video_file_descriptor);

    if (!V4l2Device::configure_video_format(video_file_descriptor,
                                            frame_dimensions)) {
        V4l2Device::close_device(video_file_descriptor);
        return 1;
    }

    // set up buffers for buffers of frams
    constexpr unsigned int BUFFER_COUNT = 4U;
    std::vector<MemoryMappedBuffer> mapped_buffers;
    if (!V4l2Device::setup_memory_mapped_buffers(
            video_file_descriptor, mapped_buffers, BUFFER_COUNT)) {
        V4l2Device::close_device(video_file_descriptor);
        return 1;
    }

    if (!V4l2Device::start_video_stream(video_file_descriptor)) {
        std::cerr << "VIDIOC_STREAMON failed\n";
        V4l2Device::unmap_buffers(mapped_buffers);
        V4l2Device::close_device(video_file_descriptor);
        return 1;
    }

    // buffer for the frame itself
    std::vector<unsigned char> rgb_frame_buffer(
        static_cast<size_t>(frame_dimensions.width * frame_dimensions.height *
                            Constants::K_RGB_COMPONENTS));

    QLabel display_label;
    display_label.resize(static_cast<int>(frame_dimensions.width),
                         static_cast<int>(frame_dimensions.height));
    display_label.show();

    std::atomic<bool> running{true};
    std::thread capture_thread([&]() {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        while (running) {
            // dequeue buffer
            if (Utils::continually_retry_ioctl(video_file_descriptor,
                                               VIDIOC_DQBUF, &buffer) == -1) {
                continue;
            }

            convert_yuyv_to_rgb(static_cast<unsigned char*>(
                                    mapped_buffers[buffer.index].start_address),
                                rgb_frame_buffer.data(), frame_dimensions);

            // update the display with the new frame, this is done on the main
            // thread to avoid GUI issues.
            QMetaObject::invokeMethod(
                &display_label,
                [&]() {
                    QImage frame_image(
                        rgb_frame_buffer.data(),
                        static_cast<int>(frame_dimensions.width),
                        static_cast<int>(frame_dimensions.height),
                        QImage::Format_RGB888);
                    display_label.setPixmap(QPixmap::fromImage(
                        frame_image.mirrored(true, false)));
                },
                Qt::QueuedConnection);

            // queue the buffer back to the driver for reuse.
            Utils::continually_retry_ioctl(video_file_descriptor, VIDIOC_QBUF,
                                           &buffer);
        }
    });

    int exit_code = QApplication::exec();

    running = false;
    capture_thread.join();

    V4l2Device::stop_video_stream(video_file_descriptor);
    V4l2Device::unmap_buffers(mapped_buffers);
    V4l2Device::close_device(video_file_descriptor);

    return exit_code;
}
