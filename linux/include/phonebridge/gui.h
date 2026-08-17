#ifndef PHONEBRIDGE_GUI_H
#define PHONEBRIDGE_GUI_H

#include <string>
#include <functional>
#include <cstdint>

namespace phonebridge {

struct GuiState {
    std::string status = "Disconnected";
    std::string device = "None";
    std::string camera = "Stopped";
    std::string microphone = "Stopped";
    int videoFps = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    int bitrate = 0;
    double latencyMs = 0.0;
    uint64_t videoFrames = 0;
    uint64_t audioFrames = 0;
    uint64_t droppedFrames = 0;
};

class Gui {
public:
    Gui();
    ~Gui();

    bool initialize(int argc, char* argv[]);
    void run();
    void quit();

    void updateState(const GuiState& state);

    std::function<void()> onConnect;
    std::function<void()> onStart;
    std::function<void()> onStop;

    struct Impl;
    Impl* impl_;
};

} // namespace phonebridge

#endif // PHONEBRIDGE_GUI_H
