#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <linux/videodev2.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "hand_music.hpp"

auto main() -> int {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Cannot open display\n";
        return 1;
    }

    V4l2Device::Device device{};
    device.file_descriptor = V4l2Device::open("/dev/video0");
    if (device.file_descriptor == -1) {
        XCloseDisplay(display);
        return 1;
    }

    FrameDimensions frame_dimensions =
        V4l2Device::select_highest_resolution(device.file_descriptor);

    int screen = DefaultScreen(display);
    constexpr int WINDOW_POS_X = 10;
    constexpr int WINDOW_POS_Y = 10;

    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen), WINDOW_POS_X, WINDOW_POS_Y,
        static_cast<int>(frame_dimensions.width),
        static_cast<int>(frame_dimensions.height), 1,
        BlackPixel(display, screen), WhitePixel(display, screen));

    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);

    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    if (!V4l2Device::configure_video_format(device.file_descriptor,
                                            frame_dimensions)) {
        V4l2Device::close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    if (!V4l2Device::setup_memory_mapped_buffers(device,
                                                 V4l2Device::MAX_BUFFERS)) {
        V4l2Device::close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    if (!V4l2Device::start_video_stream(device.file_descriptor)) {
        V4l2Device::unmap_buffers(device);
        V4l2Device::close_device(device.file_descriptor);
        XCloseDisplay(display);
        return 1;
    }

    const int frame_bytes = static_cast<int>(frame_dimensions.width) *
                            static_cast<int>(frame_dimensions.height) * 4;
    std::vector<unsigned char> rgb_frame_buffer(
        static_cast<size_t>(frame_bytes), 0);

    const int gray_frame_bytes = static_cast<int>(frame_dimensions.width) *
                                 static_cast<int>(frame_dimensions.height);
    std::vector<unsigned char> gray_frame_buffer(
        static_cast<size_t>(gray_frame_bytes), 0);

    constexpr int bitmap_pad = 32;
    XImage* x_image = XCreateImage(display, DefaultVisual(display, screen),
                                   DefaultDepth(display, screen), ZPixmap, 0,
                                   nullptr, frame_dimensions.width,
                                   frame_dimensions.height, bitmap_pad, 0);
    if (x_image == nullptr) {
        XCloseDisplay(display);
        return 1;
    }
    std::cout << "XImage: " << x_image << '\n';
    x_image->data = reinterpret_cast<char*>(rgb_frame_buffer.data());
    x_image->f.destroy_image = False;

    std::atomic<bool> running{true};
    std::thread capture_thread([&device, display, window, screen, x_image,
                                &rgb_frame_buffer, &gray_frame_buffer,
                                &frame_dimensions, &running]() {
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

            XPutImage(display, window, DefaultGC(display, screen), x_image, 0,
                      0, 0, 0, width_int, height_int);
            XFlush(display);

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

    XEvent event;
    while (running.load(std::memory_order_relaxed)) {
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape || key == XK_q) {
                running = false;
            }
        } else if (event.type == ClientMessage) {
            Atom wmDeleteMessage =
                XInternAtom(display, "WM_DELETE_WINDOW", False);
            if (static_cast<Atom>(event.xclient.data.l[0]) == wmDeleteMessage) {
                running = false;
            }
        }
    }

    running = false;
    capture_thread.join();

    V4l2Device::stop_video_stream(device.file_descriptor);
    V4l2Device::unmap_buffers(device);
    V4l2Device::close_device(device.file_descriptor);

    XDestroyImage(x_image);
    XCloseDisplay(display);

    return 0;
}
