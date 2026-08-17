#include "test.h"
#include "phonebridge/audio_handler.h"
#include <cstring>

using namespace phonebridge;

TEST("AudioHandler: initialize with default config") {
    AudioHandler handler;
    AudioConfig config;
    TEST_ASSERT_TRUE(handler.initialize(config), "Default init should succeed");
    TEST_ASSERT_EQ(handler.getSampleRate(), 16000, "Default sample rate");
    TEST_ASSERT_EQ(handler.getChannels(), 1, "Default channels");
    handler.shutdown();
    return true;
}

TEST("AudioHandler: initialize with custom config") {
    AudioHandler handler;
    AudioConfig config;
    config.sampleRate = 48000;
    config.channels = 2;
    config.bitDepth = 16;
    TEST_ASSERT_TRUE(handler.initialize(config), "Custom init should succeed");
    TEST_ASSERT_EQ(handler.getSampleRate(), 48000, "Custom sample rate");
    TEST_ASSERT_EQ(handler.getChannels(), 2, "Custom channels");
    handler.shutdown();
    return true;
}

TEST("AudioHandler: processAudioData before init fails") {
    AudioHandler handler;
    uint8_t data[] = {0x01, 0x02};
    TEST_ASSERT_FALSE(handler.processAudioData(data, sizeof(data)), "Should fail before init");
    return true;
}

TEST("AudioHandler: processAudioData after init succeeds") {
    AudioHandler handler;
    AudioConfig config;
    handler.initialize(config);
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_TRUE(handler.processAudioData(data, sizeof(data)), "Should succeed after init");
    handler.shutdown();
    return true;
}

TEST("AudioHandler: callback receives data") {
    AudioHandler handler;
    AudioConfig config;
    handler.initialize(config);

    bool callbackCalled = false;
    size_t callbackLength = 0;
    handler.onAudioData = [&](const uint8_t*, size_t length) {
        callbackCalled = true;
        callbackLength = length;
    };

    uint8_t data[] = {0x10, 0x20, 0x30};
    handler.processAudioData(data, sizeof(data));

    TEST_ASSERT_TRUE(callbackCalled, "Callback should be called");
    TEST_ASSERT_EQ((int)callbackLength, 3, "Callback length should match");
    handler.shutdown();
    return true;
}

TEST("AudioHandler: shutdown allows reinit") {
    AudioHandler handler;
    AudioConfig config;
    handler.initialize(config);
    handler.shutdown();
    TEST_ASSERT_TRUE(handler.initialize(config), "Reinit after shutdown should succeed");
    handler.shutdown();
    return true;
}
