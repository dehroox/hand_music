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

#include "hand_music.hpp"

auto main(int argc, char** argv) -> int {
    QApplication app(argc, argv);

    V4l2Device::Device device{};
    device.file_descriptor = V4l2Device::open("/dev/video0");
    if (device.file_descriptor == -1) {
        return 1;
    }

    FrameDimensions frame_dimensions =
        V4l2Device::select_highest_resolution(device.file_descriptor);

    if (!V4l2Device::configure_video_format(device.file_descriptor,
                                            frame_dimensions)) {
        V4l2Device::close_device(device.file_descriptor);
        return 1;
    }

    if (!V4l2Device::setup_memory_mapped_buffers(device,
                                                 V4l2Device::MAX_BUFFERS)) {
        V4l2Device::close_device(device.file_descriptor);
        return 1;
    }

    if (!V4l2Device::start_video_stream(device.file_descriptor)) {
        std::cerr << "VIDIOC_STREAMON failed\n";
        V4l2Device::unmap_buffers(device);
        V4l2Device::close_device(device.file_descriptor);
        return 1;
    }

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
            if (Utils::continually_retry_ioctl(device.file_descriptor,
                                               VIDIOC_DQBUF, &buffer) == -1) {
                continue;
            }

            convert_yuyv_to_rgb(
                static_cast<unsigned char*>(
                    device.mapped_buffers.at(buffer.index).start_address),
                rgb_frame_buffer.data(), frame_dimensions);

            QMetaObject::invokeMethod(
                &display_label,
                [&]() {
                    QImage frame_image(
                        rgb_frame_buffer.data(),
                        static_cast<int>(frame_dimensions.width),
                        static_cast<int>(frame_dimensions.height),
                        QImage::Format_RGB888);
                    display_label.setPixmap(
                        QPixmap::fromImage(frame_image.mirrored(true, false)));
                },
                Qt::QueuedConnection);

            Utils::continually_retry_ioctl(device.file_descriptor, VIDIOC_QBUF,
                                           &buffer);
        }
    });

    int exit_code = QApplication::exec();

    running = false;
    capture_thread.join();

    V4l2Device::stop_video_stream(device.file_descriptor);
    V4l2Device::unmap_buffers(device);
    V4l2Device::close_device(device.file_descriptor);

    return exit_code;
}
