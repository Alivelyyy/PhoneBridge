#include "phonebridge/audio_handler.h"
#include <cstring>

namespace phonebridge {

struct AudioHandler::Impl {
    AudioConfig config;
    bool initialized = false;
};

AudioHandler::AudioHandler() : impl_(new Impl()) {}

AudioHandler::~AudioHandler() {
    shutdown();
    delete impl_;
}

bool AudioHandler::initialize(const AudioConfig& config) {
    impl_->config = config;
    impl_->initialized = true;
    return true;
}

void AudioHandler::shutdown() {
    impl_->initialized = false;
}

bool AudioHandler::processAudioData(const uint8_t* data, size_t length) {
    if (!impl_->initialized) return false;

    if (onAudioData) {
        onAudioData(data, length);
    }
    return true;
}

int AudioHandler::getSampleRate() const { return impl_->config.sampleRate; }
int AudioHandler::getChannels() const { return impl_->config.channels; }

} // namespace phonebridge
