#include "phonebridge/gui.h"
#include "phonebridge/usb_device.h"
#include "phonebridge/protocol.h"
#include "phonebridge/video_decoder.h"
#include "phonebridge/virtual_camera.h"
#include "phonebridge/audio_handler.h"
#include "phonebridge/virtual_audio.h"
#include "phonebridge/config.h"
#include <gtk/gtk.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>

using Clock = std::chrono::steady_clock;

static phonebridge::UsbDevice* g_usb = nullptr;
static phonebridge::Gui* g_gui = nullptr;
static phonebridge::VideoDecoder* g_decoder = nullptr;
static phonebridge::VirtualCamera* g_camera = nullptr;
static phonebridge::AudioHandler* g_audioHandler = nullptr;
static phonebridge::VirtualAudio* g_virtualAudio = nullptr;
static phonebridge::Config* g_config = nullptr;

static bool g_streaming = false;
static std::vector<uint8_t> g_frameBuffer;
static uint32_t g_currentFrameId = 0;
static uint32_t g_currentFrameSize = 0;
static bool g_receivingFrame = false;

// Phase 10: Synchronization stats
static std::atomic<uint64_t> g_videoFramesReceived{0};
static std::atomic<uint64_t> g_audioFramesReceived{0};
static std::atomic<uint64_t> g_droppedFrames{0};
static std::atomic<int64_t> g_lastVideoLatencyUs{0};
static std::atomic<int64_t> g_lastAudioLatencyUs{0};
static std::atomic<uint64_t> g_lastVideoTimestamp{0};
static std::atomic<uint64_t> g_lastAudioTimestamp{0};
static std::atomic<uint64_t> g_lastLocalTimestamp{0};
static Clock::time_point g_lastFpsUpdate;
static int g_currentFps = 0;
static int g_framesSinceUpdate = 0;

static uint64_t getLocalTimeUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now().time_since_epoch()).count();
}

static void updateFpsCounter() {
    g_framesSinceUpdate++;
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastFpsUpdate).count();
    if (elapsed >= 1000) {
        g_currentFps = g_framesSinceUpdate;
        g_framesSinceUpdate = 0;
        g_lastFpsUpdate = now;
    }
}

static void sendError(phonebridge::ErrorCode code, const std::string& message) {
    if (!g_usb || !g_usb->isConnected()) return;

    std::vector<uint8_t> errPayload(6 + message.size() + 1);
    uint16_t codeVal = static_cast<uint16_t>(code);
    uint32_t relatedSeq = 0;
    memcpy(errPayload.data(), &codeVal, 2);
    memcpy(errPayload.data() + 2, &relatedSeq, 4);
    memcpy(errPayload.data() + 6, message.c_str(), message.size() + 1);

    uint32_t seq = g_usb->nextSequence();
    g_usb->sendPacket(phonebridge::MessageType::ERROR, 0, seq, errPayload.data(), errPayload.size());
}

static void handleHello(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    printf("[INFO] Received HELLO from Android device\n");

    if (payloadLen < 38) {
        fprintf(stderr, "[ERROR] HELLO payload too small (%zu bytes, expected >= 38)\n", payloadLen);
        sendError(phonebridge::ErrorCode::INVALID_PACKET, "HELLO payload too small");
        return;
    }

    uint32_t protoVersion = 0;
    memcpy(&protoVersion, payload, 4);

    char deviceName[33] = {};
    memcpy(deviceName, payload + 4, 32);

    uint8_t cameraCount = payload[36];
    uint8_t hasMic = payload[37];

    if (protoVersion != phonebridge::PROTOCOL_VERSION) {
        fprintf(stderr, "[ERROR] Protocol version mismatch: device=%u, expected=%u\n",
                protoVersion, phonebridge::PROTOCOL_VERSION);
        sendError(phonebridge::ErrorCode::VERSION_MISMATCH, "Protocol version mismatch");
        return;
    }

    printf("[INFO] Protocol version: %u\n", protoVersion);
    printf("[INFO] Device: %s\n", deviceName);
    printf("[INFO] Cameras: %u, Microphone: %s\n", cameraCount, hasMic ? "yes" : "no");

    phonebridge::GuiState state;
    state.status = "Connected";
    state.device = std::string(deviceName);
    state.camera = "Ready";
    state.microphone = hasMic ? "Ready" : "N/A";
    g_gui->updateState(state);

    uint32_t seq = g_usb->nextSequence();
    uint8_t ackPayload[12] = {};
    uint32_t acceptedVersion = 1;
    uint32_t maxPayload = 65536;
    uint32_t streamFlags = 0x03;
    memcpy(ackPayload + 0, &acceptedVersion, 4);
    memcpy(ackPayload + 4, &maxPayload, 4);
    memcpy(ackPayload + 8, &streamFlags, 4);

    g_usb->sendPacket(phonebridge::MessageType::HELLO_ACK, 0, seq, ackPayload, 12);
    printf("[INFO] Sent HELLO_ACK\n");
}

static void handleVideoFrame(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 9) {
        sendError(phonebridge::ErrorCode::INVALID_PACKET, "VIDEO_FRAME payload too small");
        return;
    }

    uint32_t frameId = 0;
    uint32_t frameSize = 0;
    uint8_t frameType = 0;
    memcpy(&frameId, payload, 4);
    memcpy(&frameSize, payload + 4, 4);
    frameType = payload[8];

    const uint8_t* nalData = payload + 9;
    size_t nalSize = payloadLen - 9;

    // Phase 10: Track latency
    uint64_t now = getLocalTimeUs();
    uint64_t sendTs = header.timestamp;
    if (sendTs > 0 && now > sendTs) {
        g_lastVideoLatencyUs.store(static_cast<int64_t>(now - sendTs));
    }
    g_lastVideoTimestamp.store(sendTs);
    g_lastLocalTimestamp.store(now);

    if (!g_receivingFrame) {
        g_currentFrameId = frameId;
        g_currentFrameSize = frameSize;
        g_frameBuffer.clear();
        g_frameBuffer.reserve(frameSize);
        g_receivingFrame = true;
    }

    g_frameBuffer.insert(g_frameBuffer.end(), nalData, nalData + nalSize);

    if (g_frameBuffer.size() >= g_currentFrameSize || nalSize >= g_currentFrameSize) {
        if (g_decoder && g_streaming) {
            bool isKeyframe = (frameType == 1);
            if (g_decoder->decodeFrame(g_frameBuffer.data(), g_frameBuffer.size(), isKeyframe)) {
                uint8_t rgbBuffer[640 * 480 * 3];
                size_t rgbSize = 0;
                if (g_decoder->getDecodedFrame(rgbBuffer, rgbSize)) {
                    g_camera->writeFrame(rgbBuffer, rgbSize, g_decoder->getWidth(), g_decoder->getHeight());
                }
            } else {
                g_droppedFrames.fetch_add(1);
            }
        }
        g_videoFramesReceived.fetch_add(1);
        updateFpsCounter();
        g_receivingFrame = false;
        g_frameBuffer.clear();
    }
}

static void handleVideoConfig(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 19) {
        fprintf(stderr, "[ERROR] VIDEO_CONFIG payload too small\n");
        sendError(phonebridge::ErrorCode::INVALID_PACKET, "VIDEO_CONFIG payload too small");
        return;
    }

    uint32_t width, height, fps, bitrate;
    memcpy(&width, payload, 4);
    memcpy(&height, payload + 4, 4);
    memcpy(&fps, payload + 8, 4);
    memcpy(&bitrate, payload + 12, 4);
    uint8_t codec = payload[16];

    if (codec != 0) {
        fprintf(stderr, "[ERROR] Unsupported video codec: %d (only H.264 supported)\n", codec);
        sendError(phonebridge::ErrorCode::UNSUPPORTED_CODEC, "Only H.264 video codec supported");
        return;
    }

    printf("[INFO] Video config: %ux%u @ %u fps, %u bps, codec=%d\n", width, height, fps, bitrate, codec);

    phonebridge::VideoConfig config;
    config.width = static_cast<int>(width);
    config.height = static_cast<int>(height);
    config.fps = static_cast<int>(fps);
    config.bitrate = static_cast<int>(bitrate);
    config.codec = codec;

    if (g_decoder) {
        g_decoder->shutdown();
        if (!g_decoder->initialize(config)) {
            fprintf(stderr, "[ERROR] Failed to initialize video decoder\n");
            sendError(phonebridge::ErrorCode::DECODER_ERROR, "Failed to initialize H.264 decoder");
            return;
        }
    }

    std::string devicePath = g_config ? g_config->virtualCamera.devicePath : "/dev/video0";
    if (g_camera) {
        g_camera->shutdown();
        if (!g_camera->initialize(devicePath, width, height)) {
            fprintf(stderr, "[ERROR] Failed to initialize virtual camera at %s\n", devicePath.c_str());
            sendError(phonebridge::ErrorCode::CAMERA_ERROR, "Failed to open virtual camera device");
            return;
        }
    }

    g_streaming = true;
    g_videoFramesReceived.store(0);
    g_droppedFrames.store(0);
    g_lastFpsUpdate = Clock::now();
    g_framesSinceUpdate = 0;

    uint32_t seq = g_usb->nextSequence();
    g_usb->sendPacket(phonebridge::MessageType::VIDEO_CONFIG_ACK, 0, seq);

    phonebridge::GuiState state;
    state.status = "Connected";
    state.device = g_gui ? "Android Device" : "None";
    char camInfo[64];
    snprintf(camInfo, sizeof(camInfo), "%ux%u @ %u fps", width, height, fps);
    state.camera = camInfo;
    state.microphone = "Ready";
    state.videoWidth = width;
    state.videoHeight = height;
    state.bitrate = bitrate;
    g_gui->updateState(state);

    printf("[INFO] Video streaming started\n");
}

static void handleStartStream(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    printf("[INFO] Stream started by Android device\n");
    g_streaming = true;
}

static void handleStopStream(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    printf("[INFO] Stream stopped by Android device\n");
    g_streaming = false;
    g_receivingFrame = false;
    g_frameBuffer.clear();

    if (g_camera) g_camera->shutdown();
    if (g_decoder) g_decoder->shutdown();

    phonebridge::GuiState state;
    state.status = "Connected";
    state.device = "Android Device";
    state.camera = "Stopped";
    state.microphone = "Stopped";
    g_gui->updateState(state);
}

static void handleAudioConfig(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 5) {
        fprintf(stderr, "[ERROR] AUDIO_CONFIG payload too small\n");
        sendError(phonebridge::ErrorCode::INVALID_PACKET, "AUDIO_CONFIG payload too small");
        return;
    }
    uint8_t sampleRateCode = payload[0];
    uint8_t channels = payload[1];
    uint16_t bitDepth = 0;
    memcpy(&bitDepth, payload + 2, 2);
    uint8_t format = payload[4];

    int sampleRate = (sampleRateCode == 0) ? 16000 : 48000;
    printf("[INFO] Audio config: %d Hz, %d ch, %d-bit, format=%d\n", sampleRate, channels, bitDepth, format);

    if (format != 0) {
        fprintf(stderr, "[ERROR] Unsupported audio format: %d (only PCM supported)\n", format);
        sendError(phonebridge::ErrorCode::UNSUPPORTED_CODEC, "Only PCM audio format supported");
        return;
    }

    if (g_virtualAudio) {
        g_virtualAudio->shutdown();
        if (!g_virtualAudio->initialize(sampleRate, channels)) {
            fprintf(stderr, "[ERROR] Failed to initialize PipeWire virtual microphone\n");
            sendError(phonebridge::ErrorCode::RESOURCE_ERROR, "Failed to create PipeWire audio source");
            return;
        }
    }

    g_audioFramesReceived.store(0);

    uint32_t seq = g_usb->nextSequence();
    g_usb->sendPacket(phonebridge::MessageType::AUDIO_CONFIG_ACK, 0, seq);

    phonebridge::GuiState state;
    state.status = "Connected";
    state.device = g_gui ? "Android Device" : "None";
    state.camera = g_streaming ? "Streaming" : "Ready";
    char micInfo[64];
    snprintf(micInfo, sizeof(micInfo), "%d Hz, %d ch", sampleRate, channels);
    state.microphone = micInfo;
    g_gui->updateState(state);
}

static void handleAudioFrame(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 8) {
        sendError(phonebridge::ErrorCode::INVALID_PACKET, "AUDIO_FRAME payload too small");
        return;
    }
    uint32_t frameId = 0;
    uint32_t frameSize = 0;
    memcpy(&frameId, payload, 4);
    memcpy(&frameSize, payload + 4, 4);

    const uint8_t* audioData = payload + 8;
    size_t audioSize = payloadLen - 8;

    // Phase 10: Track audio latency
    uint64_t now = getLocalTimeUs();
    uint64_t sendTs = header.timestamp;
    if (sendTs > 0 && now > sendTs) {
        g_lastAudioLatencyUs.store(static_cast<int64_t>(now - sendTs));
    }

    if (g_virtualAudio && g_streaming) {
        g_virtualAudio->writeAudio(audioData, audioSize);
    }
    g_audioFramesReceived.fetch_add(1);
}

static void handlePing(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    uint32_t seq = g_usb->nextSequence();
    g_usb->sendPacket(phonebridge::MessageType::PONG, 0, seq);
}

static void onPacketReceived(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    using MT = phonebridge::MessageType;

    switch (header.type) {
        case MT::HELLO:        handleHello(header, payload, payloadLen); break;
        case MT::VIDEO_CONFIG: handleVideoConfig(header, payload, payloadLen); break;
        case MT::VIDEO_FRAME:  handleVideoFrame(header, payload, payloadLen); break;
        case MT::AUDIO_CONFIG: handleAudioConfig(header, payload, payloadLen); break;
        case MT::AUDIO_FRAME:  handleAudioFrame(header, payload, payloadLen); break;
        case MT::START_STREAM: handleStartStream(header, payload, payloadLen); break;
        case MT::STOP_STREAM:  handleStopStream(header, payload, payloadLen); break;
        case MT::PING:         handlePing(header, payload, payloadLen); break;
        case MT::PONG:         printf("[INFO] PONG received\n"); break;
        case MT::ERROR:        printf("[ERROR] ERROR from device (type=0x%02X)\n", header.type); break;
        default:
            printf("[WARN] Unknown message type: 0x%02X\n", static_cast<uint8_t>(header.type));
            break;
    }
}

int main(int argc, char* argv[]) {
    phonebridge::Config config = phonebridge::Config::parseArgs(argc, argv);

    phonebridge::Gui gui;
    phonebridge::UsbDevice usb;
    phonebridge::VideoDecoder decoder;
    phonebridge::VirtualCamera camera;
    phonebridge::AudioHandler audioHandler;
    phonebridge::VirtualAudio virtualAudio;

    g_usb = &usb;
    g_gui = &gui;
    g_decoder = &decoder;
    g_camera = &camera;
    g_audioHandler = &audioHandler;
    g_virtualAudio = &virtualAudio;
    g_config = &config;

    if (!gui.initialize(argc, argv)) {
        fprintf(stderr, "[FATAL] Failed to initialize GUI\n");
        return 1;
    }
    if (!usb.initialize()) {
        fprintf(stderr, "[FATAL] Failed to initialize USB (is libusb installed?)\n");
        return 1;
    }

    phonebridge::VideoConfig defaultConfig;
    defaultConfig.width = config.video.width;
    defaultConfig.height = config.video.height;
    defaultConfig.fps = config.video.fps;
    defaultConfig.bitrate = config.video.bitrate;

    if (!decoder.initialize(defaultConfig)) {
        fprintf(stderr, "[WARN] Video decoder init failed (will reinit on VIDEO_CONFIG)\n");
    }

    if (!virtualAudio.initialize(config.audio.sampleRate, config.audio.channels)) {
        fprintf(stderr, "[WARN] PipeWire virtual audio init failed\n");
    }

    usb.onConnected = [&usb]() {
        printf("[INFO] USB device connected\n");
        usb.startReceiveLoop();
        printf("[INFO] Waiting for HELLO from Android...\n");
    };

    usb.onDisconnected = [&gui, &decoder, &camera, &virtualAudio]() {
        printf("[INFO] USB device disconnected\n");
        g_streaming = false;
        g_receivingFrame = false;
        g_frameBuffer.clear();

        if (g_camera) { g_camera->shutdown(); }
        if (g_decoder) { g_decoder->shutdown(); }
        if (g_virtualAudio) { g_virtualAudio->shutdown(); }

        g_videoFramesReceived.store(0);
        g_audioFramesReceived.store(0);
        g_droppedFrames.store(0);
        g_currentFps = 0;

        phonebridge::GuiState state;
        state.status = "Disconnected";
        state.device = "None";
        state.camera = "Stopped";
        state.microphone = "Stopped";
        gui.updateState(state);

        printf("[INFO] All streams stopped. Waiting for reconnection...\n");
    };

    usb.onError = [](const std::string& error) {
        fprintf(stderr, "[ERROR] USB: %s\n", error.c_str());
    };

    usb.onPacketReceived = onPacketReceived;

    gui.onConnect = [&usb]() {
        printf("[INFO] Scanning for device...\n");
        if (usb.scanForDevice()) {
            printf("[INFO] Device found, connecting...\n");
            if (!usb.connect()) {
                fprintf(stderr, "[ERROR] Failed to establish USB connection\n");
            }
        } else {
            printf("[INFO] No device found\n");
        }
    };

    gui.onStart = [&usb]() {
        printf("[INFO] Start streaming\n");
    };

    gui.onStop = [&usb]() {
        printf("[INFO] Stop streaming\n");
        g_streaming = false;
        g_receivingFrame = false;
        g_frameBuffer.clear();
        usb.sendPacket(phonebridge::MessageType::STOP_STREAM, 0, usb.nextSequence());

        if (g_camera) g_camera->shutdown();
        if (g_decoder) g_decoder->shutdown();
    };

    // Phase 12: Stats update timer (runs in GTK main loop via timeout)
    g_timeout_add(500, [](gpointer data) -> gboolean {
        auto* guiPtr = static_cast<phonebridge::Gui*>(data);
        phonebridge::GuiState state;
        state.status = g_usb && g_usb->isConnected() ? "Connected" : "Disconnected";
        state.device = g_usb && g_usb->isConnected() ? "Android Device" : "None";
        state.videoFps = g_currentFps;
        state.videoFrames = g_videoFramesReceived.load();
        state.audioFrames = g_audioFramesReceived.load();
        state.droppedFrames = g_droppedFrames.load();

        int64_t vLat = g_lastVideoLatencyUs.load();
        int64_t aLat = g_lastAudioLatencyUs.load();
        int64_t maxLat = (vLat > aLat) ? vLat : aLat;
        state.latencyMs = maxLat > 0 ? static_cast<double>(maxLat) / 1000.0 : 0.0;

        if (g_streaming) {
            state.camera = "Streaming";
            char buf[64];
            snprintf(buf, sizeof(buf), "%d Hz, %d ch",
                     g_config ? g_config->audio.sampleRate : 16000,
                     g_config ? g_config->audio.channels : 1);
            state.microphone = buf;
        }

        guiPtr->updateState(state);
        return G_SOURCE_CONTINUE;
    }, &gui);

    gui.run();

    camera.shutdown();
    virtualAudio.shutdown();
    decoder.shutdown();
    usb.shutdown();

    return 0;
}
