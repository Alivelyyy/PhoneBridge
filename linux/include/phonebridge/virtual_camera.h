#ifndef PHONEBRIDGE_VIRTUAL_CAMERA_H
#define PHONEBRIDGE_VIRTUAL_CAMERA_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

namespace phonebridge {

class VirtualCamera {
public:
    VirtualCamera();
    ~VirtualCamera();

    bool initialize(const std::string& devicePath = "/dev/video0", int width = 640, int height = 480);
    void shutdown();

    bool writeFrame(const uint8_t* data, size_t length, int width, int height);
    bool isConnected() const;

    std::string getDevicePath() const;

    // Callbacks
    std::function<void(const std::string&)> onError;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace phonebridge

#endif // PHONEBRIDGE_VIRTUAL_CAMERA_H
