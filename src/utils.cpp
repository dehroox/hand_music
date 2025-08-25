#include "utils.hpp"

#include <sys/ioctl.h>

#include <cerrno>

namespace Utils {

// when an ioctl call is interrupted by a signal (EINTR), it should be retried.
// this function wraps the ioctl call in a loop to handle such interruptions.
auto continually_retry_ioctl(int file_descriptor, unsigned long request,
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
