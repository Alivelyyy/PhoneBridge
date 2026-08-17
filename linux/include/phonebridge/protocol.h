#ifndef PHONEBRIDGE_PROTOCOL_H
#define PHONEBRIDGE_PROTOCOL_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace phonebridge {

constexpr uint32_t PROTOCOL_MAGIC = 0x50424430; // "PBD0"
constexpr uint8_t PROTOCOL_VERSION = 1;

enum class MessageType : uint8_t {
    HELLO = 0x01,
    HELLO_ACK = 0x02,
    DEVICE_INFO = 0x03,
    DEVICE_INFO_ACK = 0x04,

    VIDEO_CONFIG = 0x10,
    VIDEO_CONFIG_ACK = 0x11,
    VIDEO_FRAME = 0x12,
    AUDIO_CONFIG = 0x13,
    AUDIO_CONFIG_ACK = 0x14,
    AUDIO_FRAME = 0x15,
    START_STREAM = 0x16,
    STOP_STREAM = 0x17,

    PING = 0x20,
    PONG = 0x21,

    ERROR = 0x30
};

enum class ErrorCode : uint16_t {
    VERSION_MISMATCH = 0x01,
    INVALID_PACKET = 0x02,
    CHECKSUM_ERROR = 0x03,
    UNSUPPORTED_CODEC = 0x04,
    CAMERA_ERROR = 0x05,
    MICROPHONE_ERROR = 0x06,
    DECODER_ERROR = 0x07,
    RESOURCE_ERROR = 0x08
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic;
    uint8_t version;
    MessageType type;
    uint16_t flags;
    uint32_t payloadLength;
    uint32_t sequence;
    uint64_t timestamp;
    uint32_t checksum;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 28, "Packet header must be 28 bytes");

constexpr uint16_t FLAG_FRAGMENT = 0x0001;
constexpr uint16_t FLAG_MORE = 0x0002;
constexpr uint16_t FLAG_KEYFRAME = 0x0004;

class Protocol {
public:
    static uint32_t calculateChecksum(const uint8_t* data, size_t length);
    static uint64_t getTimestampUs();

    static std::vector<uint8_t> buildPacket(
        MessageType type,
        uint16_t flags,
        uint32_t sequence,
        const uint8_t* payload,
        uint32_t payloadLength
    );

    static bool validatePacket(const uint8_t* data, size_t length);
    static PacketHeader parseHeader(const uint8_t* data);
};

} // namespace phonebridge

#endif // PHONEBRIDGE_PROTOCOL_H
