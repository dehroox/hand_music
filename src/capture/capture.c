/*
    Simple no alloc, single buffer, per-frame capture, and capture device
    creation, using V4L2.
*/

#include "capture.h"

#include <assert.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "types.h"

ErrorCode CaptureDevice_open(CaptureDevice *device, const char *devicePath,
                             FrameDimensions dimensions) {
    device->file_descriptor = -1;
    device->buffer = NULL;
    device->buffer_size = 0;
    device->dimensions = dimensions;

    device->file_descriptor = open(devicePath, O_RDWR);
    if (device->file_descriptor < 0) {
        return ERROR_FILE_OPEN_FAILED;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = dimensions.width;
    fmt.fmt.pix.height = dimensions.height;
    fmt.fmt.pix.bytesperline = dimensions.stride;
    fmt.fmt.pix.sizeimage = dimensions.stride * dimensions.height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(device->file_descriptor, VIDIOC_S_FMT, &fmt) < 0) {
        goto error_close_fd;
    }

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(device->file_descriptor, VIDIOC_REQBUFS, &req) < 0) {
        goto error_close_fd;
    }

    struct v4l2_buffer buffer = {0};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    if (ioctl(device->file_descriptor, VIDIOC_QUERYBUF, &buffer) < 0) {
        goto error_close_fd;
    }

    device->buffer = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, device->file_descriptor, buffer.m.offset);
    if (device->buffer == MAP_FAILED) {
        goto error_close_fd;
    }
    device->buffer_size = buffer.length;

    if (ioctl(device->file_descriptor, VIDIOC_STREAMON, &buffer.type) < 0) {
        goto error_unmap_buffer;
    }

    return ERROR_NONE;

error_unmap_buffer:
    if (munmap(device->buffer, device->buffer_size) < 0) {
        // Log or handle munmap error if necessary
    }
error_close_fd:
    if (close(device->file_descriptor) < 0) {
        // Log or handle close error if necessary
    }
    return ERROR_IOCTL_FAILED;
}

void CaptureDevice_close(CaptureDevice *device) {
    struct v4l2_buffer buffer = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    ioctl(device->file_descriptor, VIDIOC_STREAMOFF, &buffer.type);
    munmap(device->buffer, device->buffer_size);
    close(device->file_descriptor);
    device->file_descriptor = -1;
    device->buffer = NULL;
    device->buffer_size = 0;
}

unsigned char *CaptureDevice_getFrame(const CaptureDevice *device) {
    struct v4l2_buffer buffer = {0};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;

    if (ioctl(device->file_descriptor, VIDIOC_QBUF, &buffer) < 0) {
        return NULL;
    }

    if (ioctl(device->file_descriptor, VIDIOC_DQBUF, &buffer) < 0) {
        return NULL;
    }

    return device->buffer;
}
