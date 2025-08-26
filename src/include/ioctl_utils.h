#ifndef IOCTL_UTILS_H
#define IOCTL_UTILS_H

#include <errno.h>
#include <stdbool.h>
#include <sys/ioctl.h>

/*
    when an ioctl call is interrupted by a signal (EINTR), it should be retried.
    this function wraps the ioctl call in a loop to handle such interruptions.
*/
static inline int continually_retry_ioctl(int file_descriptor,
                                          unsigned long request,
                                          void *argument) {
    int result = -1;
    while (true) {
        result = ioctl(file_descriptor, request, argument);
        if (result != -1 || errno != EINTR) {
            break;
        }
    }
    return result;
}

#endif  // IOCTL_UTILS_H
