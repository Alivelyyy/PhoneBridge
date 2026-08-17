#include "test.h"
#include "phonebridge/video_decoder.h"

using namespace phonebridge;

TEST("VideoDecoder: initialize with default config") {
    VideoDecoder decoder;
    VideoConfig config;
    TEST_ASSERT_TRUE(decoder.initialize(config), "Default init should succeed");
    TEST_ASSERT_EQ(decoder.getWidth(), 640, "Default width");
    TEST_ASSERT_EQ(decoder.getHeight(), 480, "Default height");
    decoder.shutdown();
    return true;
}

TEST("VideoDecoder: initialize with custom resolution") {
    VideoDecoder decoder;
    VideoConfig config;
    config.width = 1280;
    config.height = 720;
    TEST_ASSERT_TRUE(decoder.initialize(config), "Custom init should succeed");
    TEST_ASSERT_EQ(decoder.getWidth(), 1280, "Custom width");
    TEST_ASSERT_EQ(decoder.getHeight(), 720, "Custom height");
    decoder.shutdown();
    return true;
}

TEST("VideoDecoder: decodeFrame before init fails") {
    VideoDecoder decoder;
    uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x65};
    TEST_ASSERT_FALSE(decoder.decodeFrame(data, sizeof(data), true), "Should fail before init");
    return true;
}

TEST("VideoDecoder: decodeFrame after init handles invalid data") {
    VideoDecoder decoder;
    VideoConfig config;
    decoder.initialize(config);

    uint8_t garbage[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB};
    bool result = decoder.decodeFrame(garbage, sizeof(garbage), false);

    TEST_ASSERT_FALSE(result, "Invalid data should not decode");
    TEST_ASSERT_FALSE(decoder.getDecodedFrame(nullptr, *((size_t*)nullptr)), "No frame ready");
    decoder.shutdown();
    return true;
}

TEST("VideoDecoder: shutdown allows reinit") {
    VideoDecoder decoder;
    VideoConfig config;
    decoder.initialize(config);
    decoder.shutdown();
    TEST_ASSERT_TRUE(decoder.initialize(config), "Reinit after shutdown should succeed");
    decoder.shutdown();
    return true;
}

TEST("VideoDecoder: multiple init/shutdown cycles") {
    VideoDecoder decoder;
    VideoConfig config;
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(decoder.initialize(config), "Init cycle");
        decoder.shutdown();
    }
    return true;
}
