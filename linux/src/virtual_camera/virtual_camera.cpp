#include "phonebridge/virtual_camera.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cstring>

namespace phonebridge {

struct VirtualCamera::Impl {
    int fd = -1;
    std::string devicePath;
    bool connected = false;
    int width = 640;
    int height = 480;
};

VirtualCamera::VirtualCamera() : impl_(new Impl()) {}

VirtualCamera::~VirtualCamera() {
    shutdown();
    delete impl_;
}

bool VirtualCamera::initialize(const std::string& devicePath, int width, int height) {
    impl_->devicePath = devicePath;
    impl_->width = width;
    impl_->height = height;

    impl_->fd = open(devicePath.c_str(), O_WRONLY | O_NONBLOCK);
    if (impl_->fd < 0) {
        if (onError) onError("Failed to open " + devicePath + ": " + strerror(errno));
        return false;
    }

    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.bytesperline = width * 3;
    fmt.fmt.pix.sizeimage = width * height * 3;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(impl_->fd, VIDIOC_S_FMT, &fmt) < 0) {
        if (onError) onError("Failed to set format: " + std::string(strerror(errno)));
        close(impl_->fd);
        impl_->fd = -1;
        return false;
    }

    impl_->connected = true;
    return true;
}

void VirtualCamera::shutdown() {
    if (impl_->fd >= 0) {
        close(impl_->fd);
        impl_->fd = -1;
    }
    impl_->connected = false;
}

bool VirtualCamera::writeFrame(const uint8_t* data, size_t length, int width, int height) {
    if (!impl_->connected) return false;

    ssize_t written = write(impl_->fd, data, length);
    if (written < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            if (onError) onError("Failed to write frame: " + std::string(strerror(errno)));
        }
        return false;
    }
    return true;
}

bool VirtualCamera::isConnected() const {
    return impl_->connected;
}

std::string VirtualCamera::getDevicePath() const {
    return impl_->devicePath;
}

} // namespace phonebridge
