#include "phonebridge/protocol.h"
#include <cstring>
#include <chrono>

namespace phonebridge {

uint32_t Protocol::calculateChecksum(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

uint64_t Protocol::getTimestampUs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

std::vector<uint8_t> Protocol::buildPacket(
    MessageType type,
    uint16_t flags,
    uint32_t sequence,
    const uint8_t* payload,
    uint32_t payloadLength
) {
    std::vector<uint8_t> packet(sizeof(PacketHeader) + payloadLength);

    PacketHeader header{};
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = type;
    header.flags = flags;
    header.payloadLength = payloadLength;
    header.sequence = sequence;
    header.timestamp = getTimestampUs();
    header.checksum = (payloadLength > 0) ? calculateChecksum(payload, payloadLength) : 0;

    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    if (payload && payloadLength > 0) {
        std::memcpy(packet.data() + sizeof(PacketHeader), payload, payloadLength);
    }

    return packet;
}

bool Protocol::validatePacket(const uint8_t* data, size_t length) {
    if (length < sizeof(PacketHeader)) return false;

    const auto* header = reinterpret_cast<const PacketHeader*>(data);

    if (header->magic != PROTOCOL_MAGIC) return false;
    if (header->version != PROTOCOL_VERSION) return false;

    // Security: Validate payload length doesn't exceed maximum
    if (header->payloadLength > MAX_PAYLOAD_SIZE) return false;

    // Security: Verify total packet size matches header
    size_t expectedSize = sizeof(PacketHeader) + header->payloadLength;
    if (length != expectedSize) return false;

    // Security: Validate message type is known
    uint8_t typeVal = static_cast<uint8_t>(header->type);
    if (typeVal == 0 || typeVal > 0x30) return false;

    if (header->payloadLength > 0) {
        uint32_t expectedChecksum = calculateChecksum(
            data + sizeof(PacketHeader),
            header->payloadLength
        );
        if (header->checksum != expectedChecksum) return false;
    }

    return true;
}

PacketHeader Protocol::parseHeader(const uint8_t* data) {
    PacketHeader header{};
    std::memcpy(&header, data, sizeof(PacketHeader));
    return header;
}

} // namespace phonebridge
