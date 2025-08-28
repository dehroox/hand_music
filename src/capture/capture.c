/*
    Simple no alloc, single buffer, per-frame capture, and capture device
    creation, using V4L2.
*/

#include "capture.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

CaptureDevice CaptureDevice_open(const char *devicePath,
                                 FrameDimensions dimensions) {
    CaptureDevice device = {.file_descriptor = -1,
                            .buffer = NULL,
                            .buffer_size = 0,
                            .dimensions = dimensions};
    device.file_descriptor = open(devicePath, O_RDWR);

    if (device.file_descriptor < 0) {
        return device;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = dimensions.width;
    fmt.fmt.pix.height = dimensions.height;
    fmt.fmt.pix.bytesperline = dimensions.stride;
    fmt.fmt.pix.sizeimage = dimensions.stride * dimensions.height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    ioctl(device.file_descriptor, VIDIOC_S_FMT, &fmt);

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(device.file_descriptor, VIDIOC_REQBUFS, &req);

    struct v4l2_buffer buffer = {0};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    ioctl(device.file_descriptor, VIDIOC_QUERYBUF, &buffer);

    device.buffer = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                         MAP_SHARED, device.file_descriptor, buffer.m.offset);
    device.buffer_size = buffer.length;
    ioctl(device.file_descriptor, VIDIOC_STREAMON, &buffer.type);

    return device;
}

void CaptureDevice_close(CaptureDevice *device) {
    struct v4l2_buffer buffer = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    ioctl(device->file_descriptor, VIDIOC_STREAMOFF, &buffer.type);
    munmap(device->buffer, device->buffer_size);
    close(device->file_descriptor);
    device->file_descriptor = -1;
    device->buffer = NULL;
}

unsigned char *CaptureDevice_getFrame(const CaptureDevice *device) {
    struct v4l2_buffer buffer = {0};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;

    ioctl(device->file_descriptor, VIDIOC_QBUF, &buffer);
    ioctl(device->file_descriptor, VIDIOC_DQBUF, &buffer);

    return device->buffer;
}
