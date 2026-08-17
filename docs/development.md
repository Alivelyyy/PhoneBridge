# PhoneBridge Development Guide

## System Requirements

### Target Machine

- **OS:** Linux Mint 22.3 (Ubuntu 24.04 based)
- **CPU:** Intel Pentium N3700 (4 cores, 1.6 GHz)
- **RAM:** 4 GB (3.7 GB usable)
- **Architecture:** x86_64
- **Kernel:** 6.17.0-35-generic

## Development Environment

### Available Tools

| Tool | Version | Status |
|---|---|---|
| GCC | 13.3.0 | Installed |
| G++ | 13.3.0 | Installed |
| CMake | 3.28.3 | Installed |
| GNU Make | 4.3 | Installed |
| pkg-config | 1.8.1 | Installed |
| build-essential | 12.10 | Installed |
| dpkg-dev | 1.22.6 | Installed |
| fakeroot | 1.33 | Installed |
| DKMS | 3.0.11 | Installed |
| Linux headers | 6.17.0-35 | Installed |
| ADB | 34.0.4 | Installed |

### Runtime Libraries (Installed)

| Library | Version | Status |
|---|---|---|
| libusb-1.0 | 1.0.27 | Installed (runtime only) |
| libavcodec-extra | 6.1.1 | Installed (runtime only) |
| libavformat | 6.1.1 | Installed (runtime only) |
| libswscale | 6.1.1 | Installed (runtime only) |
| PipeWire | 1.0.5 | Installed |
| GTK4 | 4.14.5 | Installed (runtime only) |
| v4l2loopback | 0.15.0 | Module loaded |

### Missing Development Libraries

The following `-dev` packages are NOT installed and must be installed before building:

```bash
sudo apt install \
    libusb-1.0-0-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libswresample-dev \
    libpipewire-0.3-dev \
    libgtk-4-dev \
    v4l2loopback-dkms \
    v4l-utils
```

### Missing Android Build Tools

The following are NOT installed:

| Tool | Status |
|---|---|
| JDK 17 | Not installed |
| Android SDK | Not installed |
| Gradle | Not installed |
| Android command-line tools | Not installed |

To install Android build tools:

```bash
# Install JDK 17
sudo apt install openjdk-17-jdk

# Download Android command-line tools
mkdir -p ~/android-sdk/cmdline-tools
cd ~/android-sdk/cmdline-tools
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip commandlinetools-linux-*.zip
mv cmdline-tools latest

# Set environment variables (add to ~/.bashrc)
export ANDROID_HOME=$HOME/android-sdk
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin
export PATH=$PATH:$ANDROID_HOME/platform-tools

# Accept licenses and install SDK
sdkmanager --licenses
sdkmanager "platforms;android-34" "build-tools;34.0.0"
```

### Missing Packaging Tools

```bash
sudo apt install debhelper
```

## Build Commands

### Linux Build

```bash
cd phonebridge/linux
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Android Build

```bash
cd phonebridge/android
chmod +x gradlew
./gradlew assembleDebug
```

### Debian Package Build

```bash
cd phonebridge
dpkg-buildpackage -us -uc
```

## Project Structure

```
phonebridge/
├── android/                    # Android application
│   ├── app/
│   │   └── src/main/
│   │       ├── kotlin/         # Kotlin source files
│   │       └── res/            # Android resources
│   ├── gradle/
│   ├── build.gradle.kts
│   ├── settings.gradle.kts
│   └── gradlew
│
├── linux/                      # Linux application
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── usb/                # USB communication
│   │   ├── protocol/           # Protocol implementation
│   │   ├── video/              # Video decoding
│   │   ├── audio/              # Audio handling
│   │   ├── virtual_camera/     # v4l2loopback integration
│   │   ├── virtual_audio/      # PipeWire integration
│   │   └── gui/                # GTK4 interface
│   ├── include/                # Header files
│   └── tests/                  # Unit tests
│
├── protocol/                   # Protocol specification
│   └── protocol.md
│
├── docs/                       # Documentation
│   ├── architecture.md
│   ├── development.md
│   ├── protocol.md
│   └── troubleshooting.md
│
├── packaging/
│   └── debian/                 # Debian packaging files
│
├── scripts/                    # Build and utility scripts
│
├── README.md
└── LICENSE
```

## Development Workflow

### Phase-Based Development

PhoneBridge is developed in phases. Each phase must be completed, tested, and committed before moving to the next.

**Current phase:** Phase 0 — Architecture

### After Each Phase

1. Build the project
2. Run all tests
3. Verify functionality manually
4. Review logs for errors
5. Commit with phase tag

### Git Commit Convention

```
phase-XX-description

Examples:
phase-00-architecture
phase-01-android-setup
phase-02-linux-skeleton
phase-03-usb-protocol
```

## Testing

### Unit Tests

Linux unit tests are in `linux/tests/` and use a simple test framework. Run with:

```bash
cd phonebridge/linux/build
ctest --output-on-failure
```

### Manual Testing

1. **USB Handshake:** Connect phone, verify HELLO/HELLO_ACK exchange
2. **Camera Streaming:** Start camera, verify video appears in `/dev/videoX`
3. **Audio Streaming:** Start microphone, verify audio appears in PipeWire
4. **Application Integration:** Open OBS/Discord, select PhoneBridge devices
5. **Disconnect/Reconnect:** Unplug USB, verify graceful handling

### Performance Testing

Monitor resource usage during streaming:

```bash
# CPU and memory
top -p $(pgrep phonebridge)

# USB throughput
lsusb -t

# Video FPS
v4l2-ctl --device=/dev/video0 --list-formats-ext

# PipeWire status
pw-top
```

## Troubleshooting

See [troubleshooting.md](troubleshooting.md) for common issues and solutions.

## Dependencies Summary

### Linux Runtime Dependencies

| Package | Purpose |
|---|---|
| `libusb-1.0-0` | USB communication |
| `libavcodec60` | H.264 decoding |
| `libavformat60` | Media format handling |
| `libswscale7` | Image scaling/conversion |
| `libswresample4` | Audio resampling |
| `libpipewire-0.3-0` | Audio server |
| `libgtk-4-1` | GUI toolkit |
| `v4l2loopback` | Virtual video device |

### Linux Build Dependencies

| Package | Purpose |
|---|---|
| `build-essential` | GCC, G++, Make |
| `cmake` | Build system |
| `pkg-config` | Dependency detection |
| `libusb-1.0-0-dev` | USB headers |
| `libavcodec-dev` | FFmpeg video headers |
| `libavformat-dev` | FFmpeg format headers |
| `libswscale-dev` | FFmpeg scale headers |
| `libswresample-dev` | FFmpeg audio headers |
| `libpipewire-0.3-dev` | PipeWire headers |
| `libgtk-4-dev` | GTK4 headers |
| `debhelper` | Debian packaging |

### Android Build Dependencies

| Tool | Purpose |
|---|---|
| JDK 17 | Java compiler |
| Android SDK | Android build tools |
| Gradle | Build automation |
| Android command-line tools | SDK management |
