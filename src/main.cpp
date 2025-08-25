#include <sys/types.h>

#include <iostream>
#include <opencv2/opencv.hpp>

auto main() -> int {
    cv::VideoCapture cap(0, cv::CAP_ANY);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open webcam.\n";
        return EXIT_FAILURE;
    }

    cv::Mat frame;
    cv::Mat flipped_frame;

    for (;;) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: Failed to capture frame.\n";
            break;
        }

        cv::flip(frame, flipped_frame, 1);
        cv::imshow("Husik", flipped_frame);

        const u_int8_t LIMITER = 0xFF;
        const u_int8_t ESC = 27;
        const u_int8_t QUIT = 'q';
        const u_int8_t KEY = static_cast<unsigned>(cv::waitKey(1)) & LIMITER;

        if (KEY == QUIT || KEY == ESC) {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return EXIT_SUCCESS;
}
