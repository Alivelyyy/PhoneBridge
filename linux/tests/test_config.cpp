#include "test.h"
#include "phonebridge/config.h"

using namespace phonebridge;

TEST("Config: default values are correct") {
    Config cfg = Config::defaultConfig();
    TEST_ASSERT_EQ(cfg.video.width, 640, "Default width");
    TEST_ASSERT_EQ(cfg.video.height, 480, "Default height");
    TEST_ASSERT_EQ(cfg.video.fps, 15, "Default fps");
    TEST_ASSERT_EQ(cfg.video.bitrate, 1000000, "Default bitrate");
    TEST_ASSERT_EQ(cfg.audio.sampleRate, 16000, "Default sample rate");
    TEST_ASSERT_EQ(cfg.audio.channels, 1, "Default channels");
    TEST_ASSERT_STREQ(cfg.virtualCamera.devicePath, "/dev/video0", "Default device path");
    return true;
}

TEST("Config: parseArgs sets width") {
    char arg0[] = "phonebridge";
    char arg1[] = "--width";
    char arg2[] = "1280";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_EQ(cfg.video.width, 1280, "Parsed width");
    return true;
}

TEST("Config: parseArgs sets height") {
    char arg0[] = "phonebridge";
    char arg1[] = "--height";
    char arg2[] = "720";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_EQ(cfg.video.height, 720, "Parsed height");
    return true;
}

TEST("Config: parseArgs sets fps") {
    char arg0[] = "phonebridge";
    char arg1[] = "--fps";
    char arg2[] = "30";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_EQ(cfg.video.fps, 30, "Parsed fps");
    return true;
}

TEST("Config: parseArgs sets bitrate") {
    char arg0[] = "phonebridge";
    char arg1[] = "--bitrate";
    char arg2[] = "2000000";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_EQ(cfg.video.bitrate, 2000000, "Parsed bitrate");
    return true;
}

TEST("Config: parseArgs sets sample rate") {
    char arg0[] = "phonebridge";
    char arg1[] = "--sample-rate";
    char arg2[] = "48000";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_EQ(cfg.audio.sampleRate, 48000, "Parsed sample rate");
    return true;
}

TEST("Config: parseArgs sets channels") {
    char arg0[] = "phonebridge";
    char arg1[] = "--channels";
    char arg2[] = "2";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_EQ(cfg.audio.channels, 2, "Parsed channels");
    return true;
}

TEST("Config: parseArgs sets device path") {
    char arg0[] = "phonebridge";
    char arg1[] = "--device";
    char arg2[] = "/dev/video2";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Config cfg = Config::parseArgs(3, argv);
    TEST_ASSERT_STREQ(cfg.virtualCamera.devicePath, "/dev/video2", "Parsed device path");
    return true;
}

TEST("Config: parseArgs multiple options") {
    char arg0[] = "phonebridge";
    char arg1[] = "--width";
    char arg2[] = "1920";
    char arg3[] = "--height";
    char arg4[] = "1080";
    char arg5[] = "--fps";
    char arg6[] = "30";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, nullptr};
    Config cfg = Config::parseArgs(7, argv);
    TEST_ASSERT_EQ(cfg.video.width, 1920, "Multi width");
    TEST_ASSERT_EQ(cfg.video.height, 1080, "Multi height");
    TEST_ASSERT_EQ(cfg.video.fps, 30, "Multi fps");
    return true;
}

TEST("Config: parseArgs no args returns defaults") {
    char arg0[] = "phonebridge";
    char* argv[] = {arg0, nullptr};
    Config cfg = Config::parseArgs(1, argv);
    TEST_ASSERT_EQ(cfg.video.width, 640, "Default width with no args");
    TEST_ASSERT_EQ(cfg.video.height, 480, "Default height with no args");
    TEST_ASSERT_EQ(cfg.video.fps, 15, "Default fps with no args");
    return true;
}
