#ifndef PHONEBRIDGE_USB_DEVICE_H
#define PHONEBRIDGE_USB_DEVICE_H

#include "phonebridge/protocol.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace phonebridge {

constexpr uint16_t AOA_VID = 0x18D1;
constexpr uint16_t AOA_PID = 0x2D00;
constexpr uint16_t AOA_ACCESSORY_PID = 0x2D01;
constexpr uint16_t AOA_ADB_PID = 0x2D02;
constexpr uint16_t AOA_AUDIO_PID = 0x2D03;
constexpr uint16_t AOA_AUDIO_ADB_PID = 0x2D04;
constexpr uint16_t AOA_AUDIO_ACCESSORY_PID = 0x2D05;

constexpr uint8_t AOA_GET_PROTOCOL = 51;
constexpr uint8_t AOA_SEND_IDENT = 52;
constexpr uint8_t AOA_START_ACCESSORY = 53;

constexpr int USB_TIMEOUT_MS = 5000;
constexpr size_t USB_BUFFER_SIZE = 65536;

struct UsbDeviceInfo {
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::string manufacturer;
    std::string product;
    bool isAoaDevice = false;
};

class UsbDevice {
public:
    UsbDevice();
    ~UsbDevice();

    bool initialize();
    void shutdown();

    bool switchToAoaMode();
    bool scanForDevice();
    bool connect();
    void disconnect();

    bool isConnected() const;
    const UsbDeviceInfo& getDeviceInfo() const;

    bool sendPacket(MessageType type, uint16_t flags, uint32_t sequence,
                    const uint8_t* payload = nullptr, uint32_t payloadLength = 0);

    bool startReceiveLoop();
    void stopReceiveLoop();

    uint32_t nextSequence();

    std::function<void()> onConnected;
    std::function<void()> onDisconnected;
    std::function<void(const PacketHeader& header, const uint8_t* payload, size_t payloadLen)> onPacketReceived;
    std::function<void(const std::string&)> onError;

private:
    bool openDevice();
    bool findEndpoints();
    bool claimInterface();
    bool sendRaw(const uint8_t* data, size_t length);
    bool receiveRaw(uint8_t* data, size_t maxLength, size_t& actualLength, int timeoutMs = USB_TIMEOUT_MS);
    void receiveLoop();

    struct Impl;
    Impl* impl_;
};

} // namespace phonebridge

#endif // PHONEBRIDGE_USB_DEVICE_H
