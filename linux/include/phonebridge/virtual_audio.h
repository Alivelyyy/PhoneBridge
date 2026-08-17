#ifndef PHONEBRIDGE_VIRTUAL_AUDIO_H
#define PHONEBRIDGE_VIRTUAL_AUDIO_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

namespace phonebridge {

class VirtualAudio {
public:
    VirtualAudio();
    ~VirtualAudio();

    bool initialize(int sampleRate = 16000, int channels = 1);
    void shutdown();

    bool writeAudio(const uint8_t* data, size_t length);
    bool isConnected() const;

    // Callbacks
    std::function<void(const std::string&)> onError;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace phonebridge

#endif // PHONEBRIDGE_VIRTUAL_AUDIO_H
