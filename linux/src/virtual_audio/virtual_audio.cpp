#include "phonebridge/virtual_audio.h"
#include <cstdio>
#include <cstring>

namespace phonebridge {

struct VirtualAudio::Impl {
    bool initialized = false;
    int sampleRate = 16000;
    int channels = 1;
};

VirtualAudio::VirtualAudio() : impl_(new Impl()) {}

VirtualAudio::~VirtualAudio() {
    shutdown();
    delete impl_;
}

bool VirtualAudio::initialize(int sampleRate, int channels) {
    impl_->sampleRate = sampleRate;
    impl_->channels = channels;
    impl_->initialized = true;
    printf("[VirtualAudio] Initialized: %d Hz, %d ch\n", sampleRate, channels);
    return true;
}

void VirtualAudio::shutdown() {
    if (impl_->initialized) {
        printf("[VirtualAudio] Shut down\n");
    }
    impl_->initialized = false;
}

bool VirtualAudio::writeAudio(const uint8_t* data, size_t length) {
    if (!impl_->initialized) return false;
    return true;
}

bool VirtualAudio::isConnected() const {
    return impl_->initialized;
}

} // namespace phonebridge
