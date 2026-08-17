#include "phonebridge/config.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace phonebridge {

Config Config::defaultConfig() {
    return Config{};
}

Config Config::parseArgs(int argc, char* argv[]) {
    Config cfg;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg.video.width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg.video.height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            cfg.video.fps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bitrate") == 0 && i + 1 < argc) {
            cfg.video.bitrate = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            cfg.audio.sampleRate = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--channels") == 0 && i + 1 < argc) {
            cfg.audio.channels = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            cfg.virtualCamera.devicePath = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("PhoneBridge - USB Webcam/Microphone\n\n");
            printf("Usage: phonebridge [options]\n\n");
            printf("Video options:\n");
            printf("  --width <n>        Video width (default: 640)\n");
            printf("  --height <n>       Video height (default: 480)\n");
            printf("  --fps <n>          Video FPS (default: 15)\n");
            printf("  --bitrate <n>      Video bitrate in bps (default: 1000000)\n");
            printf("\nAudio options:\n");
            printf("  --sample-rate <n>  Audio sample rate (default: 16000)\n");
            printf("  --channels <n>     Audio channels (default: 1)\n");
            printf("\nDevice options:\n");
            printf("  --device <path>    Virtual camera device (default: /dev/video0)\n");
            printf("\nOther:\n");
            printf("  -h, --help         Show this help\n");
            exit(0);
        }
    }

    return cfg;
}

} // namespace phonebridge
