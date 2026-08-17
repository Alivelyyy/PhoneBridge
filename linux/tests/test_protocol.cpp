#include "test.h"
#include "phonebridge/protocol.h"
#include <cstring>

using namespace phonebridge;

TEST("Protocol: checksum consistency") {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t c1 = Protocol::calculateChecksum(data, sizeof(data));
    uint32_t c2 = Protocol::calculateChecksum(data, sizeof(data));
    TEST_ASSERT_EQ(c1, c2, "Same data should produce same checksum");
    return true;
}

TEST("Protocol: different data produces different checksums") {
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x01, 0x02, 0x04};
    uint32_t c1 = Protocol::calculateChecksum(data1, sizeof(data1));
    uint32_t c2 = Protocol::calculateChecksum(data2, sizeof(data2));
    TEST_ASSERT(c1 != c2, "Different data should produce different checksums");
    return true;
}

TEST("Protocol: empty data checksum") {
    uint8_t dummy = 0;
    uint32_t c = Protocol::calculateChecksum(&dummy, 0);
    TEST_ASSERT_EQ(c, (uint32_t)0, "Zero-length checksum is 0");
    return true;
}

TEST("Protocol: buildPacket produces correct header") {
    uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    auto packet = Protocol::buildPacket(MessageType::HELLO, 0, 1, payload, sizeof(payload));

    TEST_ASSERT(packet.size() >= sizeof(PacketHeader) + 3, "Packet should contain header + payload");

    const auto* header = reinterpret_cast<const PacketHeader*>(packet.data());
    TEST_ASSERT_EQ((int)header->magic, (int)PROTOCOL_MAGIC, "Magic should match");
    TEST_ASSERT_EQ((int)header->version, (int)PROTOCOL_VERSION, "Version should match");
    TEST_ASSERT_EQ((int)header->type, (int)MessageType::HELLO, "Type should match");
    TEST_ASSERT_EQ((int)header->flags, 0, "Flags should be 0");
    TEST_ASSERT_EQ((int)header->payloadLength, 3, "Payload length should match");
    TEST_ASSERT_EQ((int)header->sequence, 1, "Sequence should match");
    TEST_ASSERT(header->timestamp > 0, "Timestamp should be set");
    TEST_ASSERT(header->checksum != 0, "Checksum should be non-zero for non-empty payload");

    TEST_ASSERT_EQ((int)packet.data()[sizeof(PacketHeader)], 0xAA, "Payload byte 0");
    TEST_ASSERT_EQ((int)packet.data()[sizeof(PacketHeader) + 1], 0xBB, "Payload byte 1");
    TEST_ASSERT_EQ((int)packet.data()[sizeof(PacketHeader) + 2], 0xCC, "Payload byte 2");

    return true;
}

TEST("Protocol: buildPacket with no payload") {
    auto packet = Protocol::buildPacket(MessageType::PING, 0, 42);

    TEST_ASSERT_EQ((int)packet.size(), (int)sizeof(PacketHeader), "Packet should be header-only");

    const auto* header = reinterpret_cast<const PacketHeader*>(packet.data());
    TEST_ASSERT_EQ((int)header->payloadLength, 0, "Payload length should be 0");
    TEST_ASSERT_EQ((int)header->checksum, 0, "Checksum should be 0 for empty payload");

    return true;
}

TEST("Protocol: validatePacket accepts valid packet") {
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    auto packet = Protocol::buildPacket(MessageType::VIDEO_FRAME, FLAG_KEYFRAME, 10, payload, sizeof(payload));

    TEST_ASSERT_TRUE(Protocol::validatePacket(packet.data(), packet.size()), "Valid packet should pass validation");
    return true;
}

TEST("Protocol: validatePacket rejects wrong magic") {
    uint8_t payload[] = {0x01};
    auto packet = Protocol::buildPacket(MessageType::HELLO, 0, 1, payload, sizeof(payload));

    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFF;
    packet[3] = 0xFF;

    TEST_ASSERT_FALSE(Protocol::validatePacket(packet.data(), packet.size()), "Wrong magic should fail");
    return true;
}

TEST("Protocol: validatePacket rejects truncated packet") {
    uint8_t packet[] = {0x30, 0x44, 0x42, 0x50, 0x01};
    TEST_ASSERT_FALSE(Protocol::validatePacket(packet, sizeof(packet)), "Truncated packet should fail");
    return true;
}

TEST("Protocol: validatePacket rejects corrupted payload") {
    uint8_t payload[] = {0x01, 0x02, 0x03};
    auto packet = Protocol::buildPacket(MessageType::HELLO, 0, 1, payload, sizeof(payload));

    packet[sizeof(PacketHeader)] = 0xFF;

    TEST_ASSERT_FALSE(Protocol::validatePacket(packet.data(), packet.size()), "Corrupted payload should fail");
    return true;
}

TEST("Protocol: parseHeader extracts fields correctly") {
    uint8_t payload[] = {0x10, 0x20, 0x30};
    auto packet = Protocol::buildPacket(MessageType::AUDIO_FRAME, FLAG_FRAGMENT, 99, payload, sizeof(payload));

    PacketHeader header = Protocol::parseHeader(packet.data());
    TEST_ASSERT_EQ((int)header.magic, (int)PROTOCOL_MAGIC, "Magic");
    TEST_ASSERT_EQ((int)header.version, (int)PROTOCOL_VERSION, "Version");
    TEST_ASSERT_EQ((int)header.type, (int)MessageType::AUDIO_FRAME, "Type");
    TEST_ASSERT_EQ((int)header.flags, (int)FLAG_FRAGMENT, "Flags");
    TEST_ASSERT_EQ((int)header.payloadLength, 3, "Payload length");
    TEST_ASSERT_EQ((int)header.sequence, 99, "Sequence");

    return true;
}

TEST("Protocol: flags combinations") {
    uint8_t payload[] = {0x01};
    uint16_t flags = FLAG_FRAGMENT | FLAG_MORE | FLAG_KEYFRAME;
    auto packet = Protocol::buildPacket(MessageType::VIDEO_FRAME, flags, 1, payload, sizeof(payload));

    const auto* header = reinterpret_cast<const PacketHeader*>(packet.data());
    TEST_ASSERT_EQ((int)header->flags, (int)flags, "All flags should be preserved");

    return true;
}

TEST("Protocol: sequence numbers increment") {
    auto p1 = Protocol::buildPacket(MessageType::PING, 0, 100);
    auto p2 = Protocol::buildPacket(MessageType::PING, 0, 101);

    const auto* h1 = reinterpret_cast<const PacketHeader*>(p1.data());
    const auto* h2 = reinterpret_cast<const PacketHeader*>(p2.data());
    TEST_ASSERT(h2->sequence > h1->sequence, "Sequence should increment");

    return true;
}

TEST("Protocol: large payload") {
    std::vector<uint8_t> payload(65536);
    for (size_t i = 0; i < payload.size(); i++) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto packet = Protocol::buildPacket(MessageType::VIDEO_FRAME, 0, 1, payload.data(), payload.size());
    TEST_ASSERT_EQ((int)packet.size(), (int)(sizeof(PacketHeader) + 65536), "Large packet size");

    TEST_ASSERT_TRUE(Protocol::validatePacket(packet.data(), packet.size()), "Large valid packet");

    const auto* header = reinterpret_cast<const PacketHeader*>(packet.data());
    TEST_ASSERT_EQ((int)header->payloadLength, 65536, "Large payload length");

    return true;
}

TEST("Protocol: all message types build correctly") {
    MessageType types[] = {
        MessageType::HELLO, MessageType::HELLO_ACK,
        MessageType::DEVICE_INFO, MessageType::DEVICE_INFO_ACK,
        MessageType::VIDEO_CONFIG, MessageType::VIDEO_CONFIG_ACK,
        MessageType::VIDEO_FRAME, MessageType::AUDIO_CONFIG,
        MessageType::AUDIO_CONFIG_ACK, MessageType::AUDIO_FRAME,
        MessageType::START_STREAM, MessageType::STOP_STREAM,
        MessageType::PING, MessageType::PONG,
        MessageType::ERROR
    };

    for (auto type : types) {
        auto packet = Protocol::buildPacket(type, 0, 1);
        TEST_ASSERT(packet.size() >= sizeof(PacketHeader), "Each message type builds a valid packet");
        const auto* header = reinterpret_cast<const PacketHeader*>(packet.data());
        TEST_ASSERT_EQ((int)header->type, (int)type, "Type field matches");
    }

    return true;
}
