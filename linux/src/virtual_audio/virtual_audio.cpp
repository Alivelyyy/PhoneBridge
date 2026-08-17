#include "phonebridge/virtual_audio.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-utils.h>
#include <spa/param/param.h>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <vector>

namespace phonebridge {

struct VirtualAudio::Impl {
    pw_thread_loop* threadLoop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_stream* stream = nullptr;
    spa_hook stream_listener{};

    int sampleRate = 16000;
    int channels = 1;
    std::atomic<bool> initialized{false};
    std::atomic<bool> streaming{false};

    std::mutex ringMutex;
    std::vector<uint8_t> ringBuffer;
    size_t readPos = 0;
    size_t writePos = 0;
    size_t buffered = 0;

    void pushRing(const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(ringMutex);
        size_t cap = ringBuffer.size();
        if (cap == 0) return;
        for (size_t i = 0; i < len; i++) {
            if (buffered < cap) {
                ringBuffer[writePos] = data[i];
                writePos = (writePos + 1) % cap;
                buffered++;
            } else {
                ringBuffer[writePos] = data[i];
                writePos = (writePos + 1) % cap;
                readPos = (readPos + 1) % cap;
            }
        }
    }

    size_t popRing(uint8_t* dest, size_t maxLen) {
        std::lock_guard<std::mutex> lock(ringMutex);
        size_t toRead = (buffered < maxLen) ? buffered : maxLen;
        for (size_t i = 0; i < toRead; i++) {
            dest[i] = ringBuffer[readPos];
            readPos = (readPos + 1) % ringBuffer.size();
        }
        buffered -= toRead;
        return toRead;
    }

    static void onStreamProcess(void* userdata) {
        auto* impl = static_cast<Impl*>(userdata);
        pw_buffer* buf;
        if ((buf = pw_stream_dequeue_buffer(impl->stream)) == nullptr) return;

        spa_data& d = buf->buffer->datas[0];
        uint32_t want = d.chunk->size;
        if (want == 0 || d.data == nullptr) {
            pw_stream_queue_buffer(impl->stream, buf);
            return;
        }

        uint8_t* dest = static_cast<uint8_t*>(d.data);
        size_t got = impl->popRing(dest, want);
        if (got < (size_t)want) {
            memset(dest + got, 0, want - got);
        }
        d.chunk->size = want;
        d.chunk->offset = 0;

        pw_stream_queue_buffer(impl->stream, buf);
    }

    static void onStreamParamChanged(void* userdata, uint32_t id, const spa_pod* param) {
        (void)userdata; (void)id; (void)param;
    }

    static void onStreamStateChanged(void* userdata, pw_stream_state old_state, pw_stream_state state, const char* error) {
        auto* impl = static_cast<Impl*>(userdata);
        (void)old_state;
        if (state == PW_STREAM_STATE_STREAMING) {
            impl->streaming = true;
            printf("[VirtualAudio] PipeWire stream active (streaming)\n");
        } else if (state == PW_STREAM_STATE_ERROR) {
            printf("[VirtualAudio] PipeWire stream error: %s\n", error ? error : "unknown");
        } else if (state == PW_STREAM_STATE_UNCONNECTED) {
            impl->streaming = false;
        }
    }
};

VirtualAudio::VirtualAudio() : impl_(new Impl()) {}

VirtualAudio::~VirtualAudio() { shutdown(); delete impl_; }

bool VirtualAudio::initialize(int sampleRate, int channels) {
    if (impl_->initialized) return true;

    impl_->sampleRate = sampleRate;
    impl_->channels = channels;
    impl_->ringBuffer.resize(sampleRate * channels * 2);
    impl_->readPos = 0;
    impl_->writePos = 0;
    impl_->buffered = 0;

    pw_init(nullptr, nullptr);

    // Create thread loop first - context needs its loop
    impl_->threadLoop = pw_thread_loop_new("phonebridge-audio", nullptr);
    if (!impl_->threadLoop) {
        printf("[VirtualAudio] Failed to create PipeWire thread loop\n");
        return false;
    }

    impl_->context = pw_context_new(pw_thread_loop_get_loop(impl_->threadLoop), nullptr, 0);
    if (!impl_->context) {
        printf("[VirtualAudio] Failed to create PipeWire context\n");
        return false;
    }

    impl_->core = pw_context_connect(impl_->context, nullptr, 0);
    if (!impl_->core) {
        printf("[VirtualAudio] Failed to connect PipeWire core\n");
        return false;
    }

    impl_->stream = pw_stream_new(impl_->core, "PhoneBridge Microphone", nullptr);
    if (!impl_->stream) {
        printf("[VirtualAudio] Failed to create PipeWire stream\n");
        return false;
    }

    static pw_stream_events stream_events;
    memset(&stream_events, 0, sizeof(stream_events));
    stream_events.version = 3;
    stream_events.param_changed = Impl::onStreamParamChanged;
    stream_events.state_changed = Impl::onStreamStateChanged;
    stream_events.process = Impl::onStreamProcess;
    pw_stream_add_listener(impl_->stream, &impl_->stream_listener, &stream_events, impl_);

    pw_thread_loop_lock(impl_->threadLoop);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    spa_audio_info_raw info = {};
    info.format = SPA_AUDIO_FORMAT_S16_LE;
    info.channels = channels;
    info.rate = sampleRate;
    info.position[0] = SPA_AUDIO_CHANNEL_MONO;

    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    pw_stream_connect(impl_->stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1);

    pw_thread_loop_unlock(impl_->threadLoop);
    pw_thread_loop_start(impl_->threadLoop);

    impl_->initialized = true;
    printf("[VirtualAudio] PipeWire initialized: %d Hz, %d ch (S16LE)\n", sampleRate, channels);
    return true;
}

void VirtualAudio::shutdown() {
    if (!impl_->initialized) return;
    impl_->streaming = false;
    if (impl_->threadLoop) pw_thread_loop_stop(impl_->threadLoop);
    if (impl_->stream) { pw_stream_destroy(impl_->stream); impl_->stream = nullptr; }
    if (impl_->core) { pw_core_disconnect(impl_->core); impl_->core = nullptr; }
    if (impl_->context) { pw_context_destroy(impl_->context); impl_->context = nullptr; }
    if (impl_->threadLoop) { pw_thread_loop_destroy(impl_->threadLoop); impl_->threadLoop = nullptr; }
    impl_->initialized = false;
    printf("[VirtualAudio] Shut down\n");
}

bool VirtualAudio::writeAudio(const uint8_t* data, size_t length) {
    if (!impl_->initialized) return false;
    impl_->pushRing(data, length);
    return true;
}

bool VirtualAudio::isConnected() const {
    return impl_->initialized && impl_->streaming;
}

} // namespace phonebridge
