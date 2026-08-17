#ifndef PHONEBRIDGE_AUDIO_HANDLER_H
#define PHONEBRIDGE_AUDIO_HANDLER_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace phonebridge {

struct AudioConfig {
    int sampleRate = 16000;
    int channels = 1;
    int bitDepth = 16;
    int format = 0; // 0 = PCM
};

class AudioHandler {
public:
    AudioHandler();
    ~AudioHandler();

    bool initialize(const AudioConfig& config);
    void shutdown();

    bool processAudioData(const uint8_t* data, size_t length);

    int getSampleRate() const;
    int getChannels() const;

    // Callbacks
    std::function<void(const uint8_t*, size_t)> onAudioData;
    std::function<void(const std::string&)> onError;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace phonebridge

#endif // PHONEBRIDGE_AUDIO_HANDLER_H
