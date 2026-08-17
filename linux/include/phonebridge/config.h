#ifndef PHONEBRIDGE_CONFIG_H
#define PHONEBRIDGE_CONFIG_H

#include <string>

namespace phonebridge {

struct Config {
    struct Video {
        int width = 640;
        int height = 480;
        int fps = 15;
        int bitrate = 1000000;
        int codec = 0;
        int keyframeInterval = 30;
    } video;

    struct Audio {
        int sampleRate = 16000;
        int channels = 1;
        int bitDepth = 16;
        int format = 0;
    } audio;

    struct Usb {
        int receiveTimeoutMs = 100;
        int sendTimeoutMs = 5000;
        int pingIntervalMs = 5000;
    } usb;

    struct VirtualCamera {
        std::string devicePath = "/dev/video0";
    } virtualCamera;

    static Config defaultConfig();
    static Config parseArgs(int argc, char* argv[]);
};

} // namespace phonebridge

#endif // PHONEBRIDGE_CONFIG_H
