#include "phonebridge/gui.h"
#include "phonebridge/usb_device.h"
#include "phonebridge/protocol.h"
#include "phonebridge/video_decoder.h"
#include "phonebridge/virtual_camera.h"
#include "phonebridge/audio_handler.h"
#include "phonebridge/virtual_audio.h"
#include <cstdio>
#include <cstring>
#include <vector>

static phonebridge::UsbDevice* g_usb = nullptr;
static phonebridge::Gui* g_gui = nullptr;
static phonebridge::VideoDecoder* g_decoder = nullptr;
static phonebridge::VirtualCamera* g_camera = nullptr;
static phonebridge::AudioHandler* g_audioHandler = nullptr;
static phonebridge::VirtualAudio* g_virtualAudio = nullptr;

static bool g_streaming = false;
static std::vector<uint8_t> g_frameBuffer;
static uint32_t g_currentFrameId = 0;
static uint32_t g_currentFrameSize = 0;
static bool g_receivingFrame = false;

static void handleHello(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    printf("[INFO] Received HELLO from Android device\n");

    if (payloadLen >= 38) {
        uint32_t protoVersion = 0;
        memcpy(&protoVersion, payload, 4);

        char deviceName[33] = {};
        memcpy(deviceName, payload + 4, 32);

        uint8_t cameraCount = payload[36];
        uint8_t hasMic = payload[37];

        printf("[INFO] Protocol version: %u\n", protoVersion);
        printf("[INFO] Device: %s\n", deviceName);
        printf("[INFO] Cameras: %u, Microphone: %s\n", cameraCount, hasMic ? "yes" : "no");

        phonebridge::GuiState state;
        state.status = "Connected";
        state.device = std::string(deviceName);
        state.camera = "Ready";
        state.microphone = hasMic ? "Ready" : "N/A";
        g_gui->updateState(state);
    }

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
    if (payloadLen < 9) return;

    uint32_t frameId = 0;
    uint32_t frameSize = 0;
    uint8_t frameType = 0;
    memcpy(&frameId, payload, 4);
    memcpy(&frameSize, payload + 4, 4);
    frameType = payload[8];

    const uint8_t* nalData = payload + 9;
    size_t nalSize = payloadLen - 9;

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
            }
        }
        g_receivingFrame = false;
        g_frameBuffer.clear();
    }
}

static void handleVideoConfig(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 19) return;

    uint32_t width, height, fps, bitrate;
    memcpy(&width, payload, 4);
    memcpy(&height, payload + 4, 4);
    memcpy(&fps, payload + 8, 4);
    memcpy(&bitrate, payload + 12, 4);

    printf("[INFO] Video config: %ux%u @ %u fps, %u bps\n", width, height, fps, bitrate);

    phonebridge::VideoConfig config;
    config.width = static_cast<int>(width);
    config.height = static_cast<int>(height);
    config.fps = static_cast<int>(fps);
    config.bitrate = static_cast<int>(bitrate);
    config.codec = 0;

    if (g_decoder) {
        g_decoder->shutdown();
        g_decoder->initialize(config);
    }

    if (g_camera) {
        g_camera->shutdown();
        g_camera->initialize("/dev/video0", width, height);
    }

    g_streaming = true;

    uint32_t seq = g_usb->nextSequence();
    g_usb->sendPacket(phonebridge::MessageType::VIDEO_CONFIG_ACK, 0, seq);

    phonebridge::GuiState state;
    state.status = "Connected";
    state.device = g_gui ? "Android Device" : "None";
    char camInfo[64];
    snprintf(camInfo, sizeof(camInfo), "%ux%u @ %u fps", width, height, fps);
    state.camera = camInfo;
    state.microphone = "Ready";
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
}

static void handleAudioConfig(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 5) return;
    uint8_t sampleRate = payload[0];
    uint8_t channels = payload[1];
    printf("[INFO] Audio config: sampleRate=%s, channels=%d\n",
           sampleRate == 0 ? "16000" : "48000", channels);

    uint32_t seq = g_usb->nextSequence();
    g_usb->sendPacket(phonebridge::MessageType::AUDIO_CONFIG_ACK, 0, seq);
}

static void handleAudioFrame(const phonebridge::PacketHeader& header, const uint8_t* payload, size_t payloadLen) {
    if (payloadLen < 8) return;
    uint32_t frameId = 0;
    uint32_t frameSize = 0;
    memcpy(&frameId, payload, 4);
    memcpy(&frameSize, payload + 4, 4);

    const uint8_t* audioData = payload + 8;
    size_t audioSize = payloadLen - 8;

    if (g_virtualAudio && g_streaming) {
        g_virtualAudio->writeAudio(audioData, audioSize);
    }
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
        case MT::ERROR:        printf("[ERROR] ERROR from device\n"); break;
        default: break;
    }
}

int main(int argc, char* argv[]) {
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

    if (!gui.initialize(argc, argv)) {
        fprintf(stderr, "Failed to initialize GUI\n");
        return 1;
    }
    if (!usb.initialize()) {
        fprintf(stderr, "Failed to initialize USB\n");
        return 1;
    }

    phonebridge::VideoConfig defaultConfig;
    if (!decoder.initialize(defaultConfig)) {
        fprintf(stderr, "Warning: Video decoder init failed (will reinit on config)\n");
    }

    phonebridge::AudioConfig defaultAudioConfig;
    if (!virtualAudio.initialize(defaultAudioConfig.sampleRate, defaultAudioConfig.channels)) {
        fprintf(stderr, "Warning: PipeWire virtual audio init failed\n");
    }

    usb.onConnected = [&usb]() {
        printf("[INFO] USB device connected\n");
        usb.startReceiveLoop();
        printf("[INFO] Waiting for HELLO from Android...\n");
    };

    usb.onDisconnected = [&gui]() {
        printf("[INFO] USB device disconnected\n");
        g_streaming = false;
        phonebridge::GuiState state;
        state.status = "Disconnected";
        state.device = "None";
        state.camera = "Stopped";
        state.microphone = "Stopped";
        gui.updateState(state);
    };

    usb.onError = [](const std::string& error) {
        fprintf(stderr, "[ERROR] %s\n", error.c_str());
    };

    usb.onPacketReceived = onPacketReceived;

    gui.onConnect = [&usb]() {
        printf("[INFO] Scanning for device...\n");
        if (usb.scanForDevice()) {
            printf("[INFO] Device found, connecting...\n");
            usb.connect();
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
        usb.sendPacket(phonebridge::MessageType::STOP_STREAM, 0, usb.nextSequence());
    };

    gui.run();

    camera.shutdown();
    virtualAudio.shutdown();
    decoder.shutdown();
    usb.shutdown();

    return 0;
}
