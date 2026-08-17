# Development Guide

## Prerequisites

### Linux

```bash
sudo apt install \
  cmake g++ pkg-config \
  libusb-1.0-0-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
  libgtk-4-dev \
  libpipewire-0.3-dev \
  v4l2loopback-dkms
```

### Android

- JDK 17
- Android SDK (API 34)
- Android Build Tools 34.0.0
- Gradle 8.2.2

No Android Studio required. All builds are command-line only.

## Build Commands

### Linux

```bash
cd linux
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Binary: `build/phonebridge`

### Android

```bash
cd android
./gradlew assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

### Debian Package

```bash
dpkg-buildpackage -us -uc
```

Output: `../phonebridge_1.0.0_amd64.deb`

## Project Structure

```
phonebridge/
├── android/              # Android app
│   └── app/src/main/kotlin/com/phonebridge/
│       ├── MainActivity.kt    # Main activity, USB handling
│       ├── Protocol.kt        # Packet build/parse/checksum
│       ├── UsbConnection.kt   # USB bulk I/O
│       ├── VideoStream.kt     # CameraX + MediaCodec encoder
│       └── AudioStream.kt     # AudioRecord capture
├── linux/                # Linux app
│   ├── include/phonebridge/
│   │   ├── config.h           # Configuration system
│   │   ├── gui.h              # GTK4 GUI
│   │   ├── protocol.h         # Packet types and protocol
│   │   ├── usb_device.h       # USB device communication
│   │   ├── video_decoder.h    # FFmpeg H.264 decoder
│   │   ├── virtual_audio.h    # PipeWire virtual mic
│   │   └── virtual_camera.h   # v4l2loopback output
│   └── src/
│       ├── main.cpp           # Application entry point
│       ├── config/config.cpp  # CLI argument parsing
│       ├── gui/gui.cpp        # GTK4 interface
│       ├── protocol/protocol.cpp
│       ├── usb/usb_device.cpp
│       ├── video/video_decoder.cpp
│       ├── virtual_audio/virtual_audio.cpp
│       └── virtual_camera/virtual_camera.cpp
├── protocol/
│   └── protocol.md      # Binary protocol specification
├── docs/
│   ├── architecture.md  # System architecture
│   ├── troubleshooting.md
│   └── development.md
└── debian/              # Debian packaging
```

## Running

### Linux

```bash
# Load v4l2loopback first
sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera"

# Run with default settings
./build/phonebridge

# Run with custom settings
./build/phonebridge --width 1280 --height 720 --fps 30 --bitrate 2000000
```

### Android

Install the APK and open PhoneBridge. Tap "Connect" to start USB discovery.

## Testing

### Virtual Camera Test

```bash
# Check device exists
ls -la /dev/video0

# Test with ffmpeg
ffmpeg -f v4l2 -i /dev/video0 -frames 1 test.jpg
```

### Virtual Microphone Test

```bash
# Check PipeWire source
pw-cli ls Node | grep PhoneBridge

# Test with pactl
pactl list sources short | grep PhoneBridge
```

### USB Protocol Test

Run both apps connected via USB and check logs for successful handshake:

Linux:
```
[INFO] USB device connected
[INFO] Waiting for HELLO from Android...
[INFO] Received HELLO from Android device
[INFO] Sent HELLO_ACK
```

Android:
```
I/PhoneBridge: USB connection established
I/PhoneBridge: Sent HELLO
I/PhoneBridge: Received HELLO_ACK
```

## Dependencies

### Runtime (Linux)

| Package | Purpose |
|---------|---------|
| libusb-1.0 | USB communication |
| libavcodec | H.264 decoding |
| libavutil | FFmpeg utilities |
| libswscale | Color space conversion |
| libswresample | Audio resampling |
| GTK4 | GUI |
| PipeWire | Virtual audio |
| v4l2loopback | Virtual camera |

### Runtime (Android)

| API | Purpose |
|-----|---------|
| CameraX | Camera access |
| MediaCodec | H.264 encoding |
| AudioRecord | Microphone access |
| UsbManager | USB communication |
