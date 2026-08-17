#ifndef PHONEBRIDGE_VIDEO_DECODER_H
#define PHONEBRIDGE_VIDEO_DECODER_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace phonebridge {

struct VideoConfig {
    int width = 640;
    int height = 480;
    int fps = 15;
    int bitrate = 1000000;
    int codec = 0; // 0 = H.264
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool initialize(const VideoConfig& config);
    void shutdown();

    bool decodeFrame(const uint8_t* data, size_t length, bool isKeyframe);
    bool getDecodedFrame(uint8_t* output, size_t& outputSize);

    int getWidth() const;
    int getHeight() const;

    // Callbacks
    std::function<void(const uint8_t*, size_t, int, int)> onFrameDecoded;
    std::function<void(const std::string&)> onError;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace phonebridge

#endif // PHONEBRIDGE_VIDEO_DECODER_H
