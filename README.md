# PhoneBridge

Turn your Android phone into a USB webcam and microphone for your Linux PC.

PhoneBridge captures camera and microphone data on your Android device, streams it over a physical USB connection (Android Open Accessory mode), and exposes it to Linux applications as a standard virtual webcam (`/dev/videoX`) and virtual microphone (via PipeWire).

---

## Why PhoneBridge

Most USB webcam apps require Wi-Fi, cloud accounts, or heavy desktop frameworks. PhoneBridge is:

- **USB-only** — no network, no cloud, no accounts
- **Lightweight** — designed for low-end PCs (Intel Pentium, 4 GB RAM)
- **Local** — all processing stays on your devices
- **Standard** — exposes a regular v4l2 webcam and PipeWire mic source that any Linux app can use

---

## How It Works

```
                 USB CABLE
┌───────────────────────────────┐
│       ANDROID PHONE           │
│                               │
│  Camera ──┐                   │
│           ├─> Capture         │
│  Mic ─────┘                   │
│             │                 │
│             ▼                 │
│       H.264 / PCM            │
│          Encoder              │
│             │                 │
│             ▼                 │
│     USB Accessory (AOA)       │
└─────────────┬─────────────────┘
              │
              │ USB
              ▼
┌───────────────────────────────┐
│          LINUX PC             │
│                               │
│       USB Receiver            │
│             │                 │
│       ┌─────┴─────┐           │
│       ▼           ▼           │
│   Video Decoder  Audio       │
│       │           │           │
│       ▼           ▼           │
│  v4l2loopback   PipeWire     │
│       │           │           │
│       ▼           ▼           │
│   /dev/videoX   Virtual Mic  │
│                               │
│   Discord / OBS / Zoom / etc │
└───────────────────────────────┘
```

**Video pipeline:**

```
Android Camera → YUV → MediaCodec H.264 → USB AOA → Linux FFmpeg decoder → v4l2loopback → /dev/videoX
```

**Audio pipeline:**

```
Android Microphone → AudioRecord PCM → USB AOA → Linux receiver → PipeWire → PhoneBridge Microphone
```

Linux applications (Discord, OBS, Google Meet, Firefox, Chrome, etc.) simply select `PhoneBridge Camera` or `PhoneBridge Microphone` as input devices.

---

## Requirements

### Android

- Android 8.0 (API 26) or newer
- USB cable connected to Linux PC
- USB debugging enabled (for initial install only)

### Linux

- Debian-based distribution (Linux Mint, Ubuntu, etc.)
- amd64 architecture
- USB port

### Linux Runtime Dependencies

| Dependency | Purpose |
|---|---|
| `libusb-1.0` | USB device communication |
| `libavcodec` / `libavformat` / `libswscale` (FFmpeg) | H.264 video decoding |
| `v4l2loopback-dkms` | Virtual webcam device |
| `pipewire` | Virtual microphone source |
| `libgtk-4` | GUI |

---

## Installation

### Linux (.deb)

Download `phonebridge_1.0.0_amd64.deb` and install:

```bash
sudo apt install ./phonebridge_1.0.0_amd64.deb
```

Install system dependencies:

```bash
sudo apt install v4l2loopback-dkms pipewire libavcodec-extra
```

Load the v4l2loopback kernel module:

```bash
sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera" exclusive_caps=1
```

### Android (.apk)

Download `PhoneBridge.apk` and install it on your Android device. Enable "Install from unknown sources" if prompted.

---

## Usage

1. Load the v4l2loopback module on Linux (see above).
2. Launch PhoneBridge on your Linux PC:

   ```bash
   phonebridge
   ```

3. Connect your Android phone via USB cable.
4. Launch PhoneBridge on your Android device.
5. Click **Connect** in the Android app.
6. Grant USB permission when prompted.
7. Click **Connect** in the Linux GUI — it will automatically detect your phone and switch it to USB Accessory (AOA) mode.
8. Click **Start** to begin streaming.

9. In your video application (Discord, OBS, Zoom, etc.), select:

   ```
   PhoneBridge Camera
   ```

10. In your audio application, select:

    ```
    PhoneBridge Microphone
    ```

---

## Configuration

### Camera Settings

| Setting | Options | Default |
|---|---|---|
| Camera | Front / Rear | Rear |
| Resolution | 640x480, 1280x720 | 640x480 |
| FPS | 15, 30 | 15 |
| Video Bitrate | 500 Kbps, 1000 Kbps, 2000 Kbps | 1000 Kbps |

### Audio Settings

| Setting | Options | Default |
|---|---|---|
| Microphone | Enabled / Disabled | Enabled |
| Sample Rate | 16000 Hz, 48000 Hz | 16000 Hz |
| Channels | Mono | Mono |
| Format | 16-bit PCM | 16-bit PCM |

Default settings are chosen for low-end hardware. Start with 640x480 at 15 FPS.

---

## Building from Source

### Prerequisites

**Linux build tools:**

```bash
sudo apt install build-essential cmake pkg-config
sudo apt install libusb-1.0-0-dev libavcodec-dev libavformat-dev libswscale-dev
sudo apt install libgtk-4-dev v4l2loopback-dkms
```

**Android build tools (no Android Studio required):**

```bash
# Install JDK 17
sudo apt install openjdk-17-jdk

# Download Android command-line tools
# Set ANDROID_HOME environment variable
# Accept licenses and install SDK components
```

### Build Linux

```bash
cd phonebridge/linux
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Android

```bash
cd phonebridge/android
chmod +x gradlew
./gradlew assembleDebug
```

The debug APK will be at `android/app/build/outputs/apk/debug/app-debug.apk`.

### Run Tests

```bash
cd phonebridge/linux/build
./phonebridge_tests
```

---

## Troubleshooting

### Virtual camera not appearing

Load the kernel module:

```bash
sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera" exclusive_caps=1
```

Verify it loaded:

```bash
ls /dev/video*
```

### No audio from virtual microphone

Ensure PipeWire is running:

```bash
systemctl --user status pipewire
```

### USB device not detected

Ensure your phone is connected via USB and the PhoneBridge app is running. The Linux app will attempt to switch your phone to USB Accessory (AOA) mode automatically.

### Applications don't see the camera

Some applications require the v4l2loopback module to be loaded before they start. Restart the application after loading the module.

### High CPU usage

Lower the resolution and frame rate. Start with 640x480 at 15 FPS. Ensure hardware H.264 decoding is available on your system.

---

## Project Structure

```
phonebridge/
├── android/
│   ├── app/
│   ├── gradle/
│   ├── build.gradle.kts
│   ├── settings.gradle.kts
│   └── gradlew
│
├── linux/
│   ├── CMakeLists.txt
│   ├── src/
│   ├── include/
│   └── tests/
│
├── protocol/
│   └── protocol.md
│
├── docs/
│   ├── architecture.md
│   ├── development.md
│   ├── protocol.md
│   └── troubleshooting.md
│
├── scripts/
│
├── README.md
└── LICENSE
```

---

## Technology Stack

| Component | Technology |
|---|---|
| Android language | Kotlin |
| Android camera | CameraX |
| Android audio | AudioRecord |
| Android video encoding | MediaCodec (H.264) |
| Android USB | Android Open Accessory (AOA) |
| Linux core | C++ |
| USB (Linux side) | libusb |
| Video decoding | FFmpeg / libavcodec |
| Virtual webcam | v4l2loopback |
| Virtual microphone | PipeWire |
| GUI | GTK4 |
| Linux build | CMake |
| Android build | Gradle (command line) |

---

## Security

- No cloud accounts or remote servers
- No analytics, telemetry, or advertising
- USB-only local communication via AOA
- Packet lengths validated before memory allocation
- No code execution from received data

---

## License

See [LICENSE](LICENSE) for details.


---

## 📚 Full Project Documentation

A complete **5-page engineering deep-dive** for this repo — overview & purpose, technology stack & architecture, feature-by-feature breakdown, code internals & systems design, and development timeline & resume highlights — is available in [`DOCUMENTATION.md`](DOCUMENTATION.md).

*Part of the complete coding-history dossier generated on 2026-10-18.*
