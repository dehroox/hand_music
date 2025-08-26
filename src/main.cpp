#include <linux/videodev2.h>

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPixmap>
#include <atomic>
#include <chrono>
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
        V4l2Device::unmap_buffers(device);
        V4l2Device::close_device(device.file_descriptor);
        return 1;
    }

    const int frame_bytes = static_cast<int>(frame_dimensions.width) *
                            static_cast<int>(frame_dimensions.height) *
                            static_cast<int>(Constants::K_RGB_COMPONENTS);
    std::vector<unsigned char> rgb_frame_buffer(
        static_cast<size_t>(frame_bytes), 0);

    const int gray_frame_bytes = static_cast<int>(frame_dimensions.width) *
                                 static_cast<int>(frame_dimensions.height);
    std::vector<unsigned char> gray_frame_buffer(
        static_cast<size_t>(gray_frame_bytes), 0);

    QImage frame_image(
        rgb_frame_buffer.data(), static_cast<int>(frame_dimensions.width),
        static_cast<int>(frame_dimensions.height), QImage::Format_RGB888);

    QLabel display_label;
    display_label.resize(static_cast<int>(frame_dimensions.width),
                         static_cast<int>(frame_dimensions.height));
    display_label.show();

    std::atomic<bool> running{true};
    std::thread capture_thread([&device, &display_label, &rgb_frame_buffer,
                                &gray_frame_buffer, &frame_dimensions,
                                &running]() {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        const int width_int = static_cast<int>(frame_dimensions.width);
        const int height_int = static_cast<int>(frame_dimensions.height);

        long long total_duration_rgb = 0;
        long long total_duration_gray = 0;
        int frame_count = 0;

        while (running.load(std::memory_order_relaxed)) {
            if (Utils::continually_retry_ioctl(device.file_descriptor,
                                               VIDIOC_DQBUF, &buffer) == -1) {
                continue;
            }

            auto* yuv_data = static_cast<unsigned char*>(
                device.mapped_buffers.at(buffer.index).start_address);

            auto start_rgb = std::chrono::high_resolution_clock::now();
            convert_yuv_to_rgb(yuv_data, rgb_frame_buffer.data(),
                               frame_dimensions);
            auto end_rgb = std::chrono::high_resolution_clock::now();
            total_duration_rgb +=
                std::chrono::duration_cast<std::chrono::microseconds>(end_rgb -
                                                                      start_rgb)
                    .count();

            auto start_gray = std::chrono::high_resolution_clock::now();
            convert_yuv_to_gray(yuv_data, gray_frame_buffer.data(),
                                frame_dimensions);
            auto end_gray = std::chrono::high_resolution_clock::now();
            total_duration_gray +=
                std::chrono::duration_cast<std::chrono::microseconds>(
                    end_gray - start_gray)
                    .count();

            frame_count++;

            QMetaObject::invokeMethod(
                &display_label,
                [ptr = rgb_frame_buffer.data(), width_copy = width_int,
                 height_copy = height_int, &display_label]() {
                    QImage tmp_image(ptr, width_copy, height_copy,
                                     QImage::Format_RGB888);
                    display_label.setPixmap(
                        QPixmap::fromImage(tmp_image.mirrored(true, false)));
                },
                Qt::QueuedConnection);

            Utils::continually_retry_ioctl(device.file_descriptor, VIDIOC_QBUF,
                                           &buffer);
        }

        if (frame_count > 0) {
            std::cout << "Average YUV to RGB conversion time: "
                      << total_duration_rgb / frame_count << " microseconds."
                      << '\n';
            std::cout << "Average YUV to Gray conversion time: "
                      << total_duration_gray / frame_count << " microseconds."
                      << '\n';
        }
    });

    const int exit_code = QApplication::exec();
    running = false;
    capture_thread.join();

    V4l2Device::stop_video_stream(device.file_descriptor);
    V4l2Device::unmap_buffers(device);
    V4l2Device::close_device(device.file_descriptor);

    return exit_code;
}