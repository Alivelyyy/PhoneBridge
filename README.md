# PhoneBridge

Turn your Android phone into a USB webcam and microphone for your Linux PC.

PhoneBridge captures camera and microphone data on your Android device, streams it over a physical USB connection, and exposes it to Linux applications as a standard virtual webcam (`/dev/videoX`) and virtual microphone (via PipeWire).

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
│       USB Transport           │
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
Android Camera → YUV → MediaCodec H.264 → USB → Linux FFmpeg decoder → v4l2loopback → /dev/videoX
```

**Audio pipeline:**

```
Android Microphone → AudioRecord PCM → USB → Linux receiver → PipeWire → PhoneBridge Microphone
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
5. Grant USB permission when prompted.
6. Click **Start** in the Linux GUI or use the command line:

   ```bash
   phonebridge --start
   ```

7. In your video application (Discord, OBS, Zoom, etc.), select:

   ```
   PhoneBridge Camera
   ```

8. In your audio application, select:

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
cmake ..
make -j$(nproc)
```

### Build Android

```bash
cd phonebridge/android
chmod +x gradlew
./gradlew assembleDebug
```

The debug APK will be at `android/app/build/outputs/apk/debug/app-debug.apk`.

### Build Debian Package

```bash
cd phonebridge
dpkg-buildpackage -us -uc
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

Check that your phone is connected and USB debugging is enabled:

```bash
lsusb
```

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
│   │   ├── main.cpp
│   │   ├── usb/
│   │   ├── protocol/
│   │   ├── video/
│   │   ├── audio/
│   │   ├── virtual_camera/
│   │   ├── virtual_audio/
│   │   └── gui/
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
├── packaging/
│   └── debian/
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
| Android USB | Android USB APIs |
| Linux core | C++ |
| USB (Linux side) | libusb |
| Video decoding | FFmpeg / libavcodec |
| Virtual webcam | v4l2loopback |
| Virtual microphone | PipeWire |
| GUI | GTK4 |
| Linux build | CMake |
| Android build | Gradle (command line) |
| Debian packaging | debhelper |

---

## Security

- No cloud accounts or remote servers
- No analytics, telemetry, or advertising
- USB-only local communication
- Packet lengths validated before memory allocation
- No code execution from received data

---

## License

See [LICENSE](LICENSE) for details.

Yes. This is a very practical project, and your old PC actually makes the problem interesting: **your phone becomes the camera + microphone, while the PC receives the streams and exposes them to normal Linux applications as a webcam and microphone.**

The important part is choosing the architecture correctly. I would **not** start with Android Studio, Flutter, Electron, or a heavy framework. Your PC is too limited for that workflow.

### The architecture I recommend

```text
                 USB CABLE
┌───────────────────────────────┐
│                               │
│       ANDROID PHONE           │
│                               │
│  Camera ──┐                   │
│           ├─> Capture         │
│  Mic ─────┘                   │
│             │                 │
│             ▼                 │
│       H.264 / Audio           │
│          Encoder              │
│             │                 │
│             ▼                 │
│       USB Transport           │
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
│   Discord / OBS / Meet / etc.│
└───────────────────────────────┘
```

Android officially supports USB communication through its USB APIs, including accessory/host communication. ([Android Developers][1])

For the Linux `.deb`, we can use standard Debian packaging with `debhelper`, which is specifically designed to automate Debian package construction. ([Debian][2])

---

# 1. What we're actually building

Let's give the project a temporary name:

**PhoneBridge**

The software has two components:

### Android application

`PhoneBridge.apk`

Responsibilities:

* Detect USB connection
* Request USB permission
* Capture rear/front camera
* Capture microphone
* Encode video
* Encode audio
* Send video/audio through USB
* Display connection status
* Allow camera selection
* Allow resolution/FPS selection
* Allow microphone enable/disable
* Show bitrate/FPS/status
* Stop streaming safely

### Linux application

`phonebridge`

Responsibilities:

* Detect the Android device
* Establish USB connection
* Receive video/audio
* Decode video
* Create a Linux virtual webcam
* Create a Linux virtual microphone
* Provide a small GUI
* Show connection status
* Show FPS/bitrate
* Start/stop streaming
* Clean up virtual devices

The final result should be:

```text
PhoneBridge
├── Android
│   └── PhoneBridge.apk
│
└── Linux
    └── phonebridge_1.0.0_amd64.deb
```

---

# 2. VERY IMPORTANT: don't make OpenCode build everything at once

Because you don't know much software development yet, don't give OpenCode one gigantic "make my entire app" request and hope for the best.

We should develop it in **phases**.

```text
PHASE 0   Project architecture
PHASE 1   Linux development environment
PHASE 2   USB communication prototype
PHASE 3   Android camera capture
PHASE 4   Android microphone capture
PHASE 5   Send video/audio
PHASE 6   Linux receive/decode
PHASE 7   Virtual webcam
PHASE 8   Virtual microphone
PHASE 9   GUI
PHASE 10  Performance optimization
PHASE 11  Packaging
PHASE 12  Testing
```

That will dramatically reduce debugging hell.

---

# 3. Recommended technology

For your PC, I'd use:

| Component       | Technology                                                    |
| --------------- | ------------------------------------------------------------- |
| Android         | Kotlin                                                        |
| Android UI      | Jetpack Compose **only if lightweight enough**; otherwise XML |
| Camera          | CameraX                                                       |
| Microphone      | AudioRecord                                                   |
| Video encoding  | Android MediaCodec                                            |
| Audio           | PCM initially, Opus later                                     |
| USB             | Android USB APIs                                              |
| Linux core      | C++                                                           |
| USB Linux side  | libusb                                                        |
| Video decoder   | FFmpeg/libavcodec                                             |
| Virtual webcam  | v4l2loopback                                                  |
| Linux audio     | PipeWire                                                      |
| GUI             | GTK4                                                          |
| Build           | CMake                                                         |
| Linux package   | debhelper                                                     |
| Android build   | Gradle command line                                           |
| Version control | Git                                                           |

**Why C++ on Linux?**

Because the receiver needs to interact with Linux multimedia APIs, USB, FFmpeg, v4l2loopback and PipeWire. C++ gives us a much cleaner path than trying to make Python handle the entire system.

And we don't need Android Studio. Gradle can build the APK from the command line.

---

# 4. One major design decision

I recommend **not** initially trying to make the phone appear directly as a standard USB webcam.

Instead:

```text
Phone
   ↓
PhoneBridge Android
   ↓
USB
   ↓
PhoneBridge Linux
   ↓
v4l2loopback
   ↓
/dev/videoX
```

This means Linux applications simply see:

```text
PhoneBridge Camera
```

as an ordinary webcam.

That's much easier to control.

Similarly:

```text
Phone microphone
       ↓
     USB
       ↓
PhoneBridge Linux
       ↓
    PipeWire
       ↓
PhoneBridge Microphone
```

---

# 5. Start with the USB protocol

This is the foundation.

Don't start with video.

First make:

```text
Android

HELLO
   ↓
Linux

Linux
ACK
   ↓
Android

PING
   ↓
Linux

PONG
```

Once this works reliably, everything else can use the same connection.

The protocol should eventually look something like:

```text
[HEADER]
MAGIC
VERSION
TYPE
FLAGS
PAYLOAD_LENGTH
SEQUENCE
TIMESTAMP
CHECKSUM

[PAYLOAD]
...
```

Message types:

```text
0x01 HELLO
0x02 HELLO_ACK

0x10 VIDEO_CONFIG
0x11 VIDEO_FRAME

0x20 AUDIO_CONFIG
0x21 AUDIO_FRAME

0x30 START_STREAM
0x31 STOP_STREAM

0x40 PING
0x41 PONG

0x50 ERROR
```

OpenCode should create this as a **shared protocol specification**, not duplicate protocol logic randomly in both applications.

---

# 6. Video format

Don't initially send raw camera frames.

That would be wasteful.

Use:

```text
Camera
  ↓
YUV
  ↓
MediaCodec
  ↓
H.264
  ↓
USB
```

Start with:

```text
640 × 480
15 FPS
H.264
~1 Mbps
```

Then test:

```text
1280 × 720
15 FPS
```

Only after that should we consider 30 FPS.

Your PC is old, so **15 FPS should be our initial target**.

---

# 7. Audio

Start extremely simple.

Android:

```text
Microphone
   ↓
AudioRecord
   ↓
PCM
   ↓
USB
```

Use:

```text
Mono
16-bit
16000 Hz
```

That's only:

```text
16,000 × 2 = 32 KB/s
```

which is tiny compared with video.

Once everything works, we can add Opus compression.

---

# 8. Linux virtual camera

This is where the project becomes really useful.

Linux needs to see:

```text
/dev/videoX
```

We can use `v4l2loopback`.

Then:

```text
PhoneBridge
     ↓
H.264 decoder
     ↓
Raw frames
     ↓
v4l2loopback
     ↓
/dev/videoX
```

Applications can then select:

```text
PhoneBridge Camera
```

instead of your terrible built-in webcam.

---

# 9. Linux virtual microphone

The same concept applies to audio.

The application receives:

```text
PCM
```

and feeds it into a PipeWire virtual source.

Applications then see:

```text
PhoneBridge Microphone
```

So eventually:

```text
Discord
Zoom
Google Meet
OBS
Firefox
Chrome
```

can use the phone's microphone.

---

# 10. The MASTER OpenCode prompt

This is the part I'd actually give to OpenCode.

Don't dump everything into one conversation immediately. Give it the following **master specification first**.

# PhoneBridge — Master OpenCode Development Specification

## Project Overview

Build a lightweight cross-device application called **PhoneBridge**.

PhoneBridge allows an Android smartphone to act as an external webcam and microphone for a Linux PC over a physical USB connection.

The Android application captures camera and microphone data, transfers the data over USB, and the Linux application receives the data and exposes it to Linux applications as:

* A virtual webcam
* A virtual microphone

The target development machine is a very low-end Linux PC with approximately:

* Intel Pentium N3700
* 4 GB RAM
* Intel integrated graphics
* Linux Mint Cinnamon
* Limited storage and CPU resources

The software must therefore prioritize low CPU usage, low memory usage, low latency, and minimal dependencies.

---

# Core Architecture

Use two separate applications:

## Android

Project:

`phonebridge-android`

Output:

`PhoneBridge.apk`

Responsibilities:

* Camera capture
* Microphone capture
* H.264 video encoding
* Audio capture
* USB communication
* Device configuration
* Connection status
* Stream control

## Linux

Project:

`phonebridge-linux`

Output:

`phonebridge_<version>_amd64.deb`

Responsibilities:

* USB communication
* Protocol handling
* H.264 decoding
* Audio reception
* Virtual webcam output
* Virtual microphone output
* Lightweight GUI
* Device management
* Performance statistics

---

# Development Rules

Follow these rules throughout development.

1. Do not rewrite the architecture without explaining why.
2. Do not introduce unnecessary dependencies.
3. Do not use Electron.
4. Do not use Node.js for the core Linux receiver.
5. Do not require Android Studio.
6. Android must be buildable from the command line using Gradle.
7. Linux must be buildable using CMake.
8. Keep CPU and RAM usage as low as possible.
9. Implement one subsystem at a time.
10. Do not implement video before USB communication is proven.
11. Do not implement audio before basic video transport is proven.
12. Every major subsystem must have a test.
13. Never silently change the USB protocol.
14. Keep the protocol documented in `docs/protocol.md`.
15. Never leave placeholder implementations that look finished.
16. If a feature cannot be implemented reliably, explicitly report it.
17. Do not add cloud services.
18. The application must work offline.
19. USB is the primary connection method.
20. Wi-Fi is out of scope for the first release.

---

# Repository Structure

Create this repository structure:

```text
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
│   │   ├── main.cpp
│   │   ├── usb/
│   │   ├── protocol/
│   │   ├── video/
│   │   ├── audio/
│   │   ├── virtual_camera/
│   │   ├── virtual_audio/
│   │   └── gui/
│   │
│   ├── include/
│   └── tests/
│
├── protocol/
│   └── protocol.md
│
├── docs/
│   ├── architecture.md
│   ├── development.md
│   ├── troubleshooting.md
│   └── protocol.md
│
├── packaging/
│   └── debian/
│
├── scripts/
│
├── README.md
└── LICENSE
```

---

# Phase 0 — Architecture

Before writing implementation code:

1. Inspect the development environment.
2. Determine available:

   * GCC
   * G++
   * CMake
   * Java
   * Gradle
   * Android SDK
   * FFmpeg
   * libusb
   * PipeWire
   * GTK
   * v4l2loopback
3. Do not install unnecessary packages automatically.
4. Create `docs/architecture.md`.
5. Explain every major component.
6. Explain how Android communicates with Linux.
7. Explain how video travels from camera to `/dev/videoX`.
8. Explain how audio travels from microphone to PipeWire.
9. Explain the build process.

Do not implement streaming yet.

---

# Phase 1 — Android Command-Line Build

Create the Android project.

Requirements:

* Kotlin
* Modern Android Gradle configuration
* Command-line build support
* No Android Studio requirement
* Debug APK must be generated with Gradle

Required command:

```bash
./gradlew assembleDebug
```

The project must produce an APK.

Create a minimal application showing:

```text
PhoneBridge

USB: Disconnected

Waiting for connection...
```

Do not implement camera or microphone yet.

---

# Phase 2 — Linux Skeleton

Create the Linux C++ project.

Use:

* C++
* CMake
* libusb
* GTK only where necessary

The Linux application must start successfully.

GUI:

```text
PhoneBridge

Status:
Disconnected

Device:
None

Video:
Stopped

Audio:
Stopped

[Connect]
```

Do not implement video yet.

---

# Phase 3 — USB Protocol

Design a deterministic binary USB protocol.

Every packet must contain:

```text
MAGIC
VERSION
TYPE
FLAGS
PAYLOAD_LENGTH
SEQUENCE
TIMESTAMP
```

Use explicit integer sizes.

Do not rely on C/C++ struct memory layout.

Serialize and deserialize fields explicitly.

Implement packet validation.

Reject:

* Invalid magic
* Unsupported protocol version
* Invalid packet length
* Invalid message type
* Corrupted packets

---

# Protocol Message Types

Define:

```text
HELLO
HELLO_ACK

DEVICE_INFO
DEVICE_INFO_ACK

VIDEO_CONFIG
VIDEO_CONFIG_ACK

AUDIO_CONFIG
AUDIO_CONFIG_ACK

START_STREAM
STOP_STREAM

VIDEO_FRAME
AUDIO_FRAME

PING
PONG

ERROR
```

Document every message in:

```text
protocol/protocol.md
```

---

# Phase 4 — USB Handshake

Implement the minimum possible USB functionality.

The first successful communication must be:

```text
Android
  |
  | HELLO
  v
Linux
  |
  | HELLO_ACK
  v
Android
```

Then:

```text
Android -> PING
Linux   -> PONG
```

Display:

```text
USB Connected
PhoneBridge Android detected
Protocol: 1
```

Add logging.

Example:

```text
[INFO] USB device detected
[INFO] Opening USB connection
[INFO] USB permission granted
[INFO] Sending HELLO
[INFO] Received HELLO_ACK
[INFO] Protocol version: 1
```

Do not continue until this works reliably.

---

# Phase 5 — Android Camera

Use Android's camera APIs.

Requirements:

* Front camera
* Rear camera
* Camera selection
* Resolution selection
* FPS configuration

Initial configuration:

```text
640x480
15 FPS
H.264
```

Do not initially use 4K, 1080p, 60 FPS, or uncompressed frames.

Use Android hardware video encoding where available.

Use `MediaCodec`.

The pipeline should be:

```text
Camera
↓
YUV
↓
MediaCodec
↓
H.264
↓
Packetizer
↓
USB
```

Do not send raw frames.

---

# Phase 6 — Linux Video Receiver

Implement:

```text
USB
↓
Protocol parser
↓
H.264 packet reconstruction
↓
H.264 decoder
↓
Raw video frame
```

Use FFmpeg/libavcodec where appropriate.

The receiver must handle:

* Packet fragmentation
* Packet ordering
* Missing packets
* Stream restart
* Decoder errors
* USB disconnect

Do not crash on malformed input.

---

# Phase 7 — Virtual Webcam

Integrate Linux `v4l2loopback`.

The application should expose:

```text
PhoneBridge Camera
```

as a Linux video device.

Pipeline:

```text
Android Camera
↓
H.264
↓
USB
↓
Linux Receiver
↓
FFmpeg Decoder
↓
Raw Video
↓
v4l2loopback
↓
/dev/videoX
```

Support at minimum:

```text
640x480
15 FPS
```

The virtual camera must be usable by standard Linux applications.

---

# Phase 8 — Android Microphone

Use Android `AudioRecord`.

Initial format:

```text
Mono
16-bit PCM
16000 Hz
```

Pipeline:

```text
Microphone
↓
AudioRecord
↓
PCM
↓
Packetizer
↓
USB
```

Do not implement audio compression until uncompressed audio works.

---

# Phase 9 — Linux Virtual Microphone

Receive PCM audio.

Create a PipeWire-compatible virtual microphone source.

Expose:

```text
PhoneBridge Microphone
```

Pipeline:

```text
Android Microphone
↓
PCM
↓
USB
↓
Linux Receiver
↓
PipeWire
↓
PhoneBridge Microphone
```

The virtual microphone should be selectable by Linux applications.

---

# Phase 10 — Synchronization

Add timestamps.

Every audio and video frame must contain:

```text
sequence
timestamp
```

Use a consistent monotonic clock strategy.

The Linux receiver must maintain:

* Video timestamp
* Audio timestamp
* Frame sequence
* Audio sequence

Avoid unnecessary buffering.

Target latency:

```text
< 200 ms
```

If this cannot be achieved on the target hardware, report the measured latency.

---

# Phase 11 — Disconnect/Reconnect

The application must handle:

```text
USB connected
USB disconnected
USB reconnected
Android application closed
Linux application closed
USB cable removed
USB permission revoked
Decoder failure
```

The Linux application must not crash when the phone disappears.

The GUI should return to:

```text
Status:
Disconnected

Waiting for PhoneBridge...
```

When reconnecting:

```text
Status:
Connected

Camera:
Active

Microphone:
Active
```

---

# Phase 12 — Lightweight GUI

Create a simple GTK interface.

Do not create a heavy graphical interface.

Design:

```text
┌───────────────────────────────┐
│          PhoneBridge          │
├───────────────────────────────┤
│                               │
│ Status      Connected         │
│ Device      Android Phone     │
│                               │
│ Camera      640x480 @ 15 FPS  │
│ Microphone  16 kHz Mono       │
│                               │
│ Video       15 FPS             │
│ Bitrate     1.0 Mbps          │
│ Latency     120 ms             │
│                               │
│ [ Start ]      [ Stop ]       │
│                               │
└───────────────────────────────┘
```

Keep it functional rather than decorative.

---

# Phase 13 — Configuration

Create a configuration system.

Possible settings:

```text
Camera:
- Front
- Rear

Resolution:
- 640x480
- 1280x720

FPS:
- 15
- 30

Video bitrate:
- 500 Kbps
- 1000 Kbps
- 2000 Kbps

Microphone:
- Enabled
- Disabled

Audio sample rate:
- 16000
- 48000
```

Default to low-resource settings.

---

# Phase 14 — Performance Optimization

The target machine is low-end.

Measure:

```text
CPU %
RAM usage
Video FPS
Dropped frames
USB throughput
Decode time
Audio latency
End-to-end latency
```

Add a diagnostic mode.

Example:

```text
CPU: 42%
RAM: 210 MB
Video: 15 FPS
Dropped: 0
USB: 1.1 Mbps
Latency: 128 ms
```

Do not optimize blindly.

Measure first.

---

# Phase 15 — Error Handling

Never allow errors to silently fail.

Errors should be human-readable.

Examples:

```text
Unable to find Android device.

USB permission was not granted.

Unsupported Android device.

Protocol version mismatch.

Unable to initialize camera.

Unable to initialize microphone.

H.264 decoder initialization failed.

Unable to create virtual camera.

Unable to initialize PipeWire.

USB connection lost.
```

---

# Phase 16 — Linux Dependencies

Identify the exact runtime dependencies.

Potential dependencies include:

* libusb
* FFmpeg libraries
* GTK
* PipeWire
* v4l2loopback

Do not bundle system libraries unnecessarily.

Use the Linux distribution's package manager where appropriate.

Document dependencies in:

```text
docs/development.md
```

---

# Phase 17 — Debian Packaging

Create a proper Debian package.

Package:

```text
phonebridge
```

Architecture:

```text
amd64
```

Create:

```text
debian/
├── control
├── rules
├── changelog
├── copyright
├── compat
└── install
```

Use modern debhelper packaging.

The package should install:

```text
/usr/bin/phonebridge
```

Desktop application:

```text
/usr/share/applications/phonebridge.desktop
```

Application icon:

```text
/usr/share/icons/...
```

Configuration should follow normal Linux filesystem conventions.

Build with:

```bash
dpkg-buildpackage -us -uc
```

Final output should include:

```text
phonebridge_1.0.0_amd64.deb
```

---

# Phase 18 — Installation

The final package must support:

```bash
sudo apt install ./phonebridge_1.0.0_amd64.deb
```

After installation:

```bash
phonebridge
```

should launch the application.

Do not require the source repository after installation.

---

# Phase 19 — Android APK

Final Android build:

```bash
./gradlew assembleRelease
```

Output:

```text
PhoneBridge.apk
```

Do not require Android Studio.

Document how to build it from the command line.

---

# Phase 20 — Testing

Create automated and manual tests.

## USB tests

* Device detection
* Permission
* Handshake
* PING/PONG
* Disconnect
* Reconnect

## Video tests

* Camera opens
* H.264 encoder starts
* Frames transfer
* Frames decode
* Virtual camera works
* 15 FPS achieved
* Dropped frames handled

## Audio tests

* Microphone opens
* PCM transfer works
* PipeWire source appears
* Linux application receives audio

## Failure tests

* Cable removed
* Phone application closed
* Linux application closed
* Invalid packet
* Decoder failure
* USB permission denied

---

# Phase 21 — Security

The application is local and USB-only.

Do not introduce:

* Cloud accounts
* Remote servers
* Analytics
* Advertising
* Telemetry

Validate packet lengths before allocating memory.

Never trust payload lengths received from USB.

Prevent buffer overflows.

Do not execute anything received from the phone.

---

# Phase 22 — Documentation

Create:

```text
README.md
docs/architecture.md
docs/development.md
docs/protocol.md
docs/troubleshooting.md
```

README must explain:

1. What PhoneBridge does
2. Supported platforms
3. Requirements
4. Installation
5. Android installation
6. USB connection
7. Starting the application
8. Selecting the virtual webcam
9. Selecting the virtual microphone
10. Troubleshooting
11. Building from source

---

# Critical Development Workflow

Do NOT implement all phases simultaneously.

After each phase:

1. Build.
2. Run.
3. Test.
4. Inspect logs.
5. Fix errors.
6. Commit the working state.
7. Only then continue.

Use Git commits such as:

```text
phase-01-project-setup
phase-02-linux-skeleton
phase-03-usb-protocol
phase-04-usb-handshake
phase-05-camera-capture
phase-06-video-receiver
phase-07-virtual-camera
phase-08-audio-capture
phase-09-virtual-microphone
phase-10-synchronization
phase-11-gui
phase-12-packaging
```

---

# IMPORTANT RULE FOR OPENCODE

When starting a new phase, first inspect the existing repository.

Do not overwrite working code unnecessarily.

Before making architectural changes, explain:

1. What is wrong.
2. Why the current approach is insufficient.
3. What will change.
4. What files will be affected.
5. How the change will be tested.

Then implement the change.

Never pretend a feature works if it has not been tested.

At the end of every phase, provide:

```text
PHASE STATUS

Completed:
- ...

Files changed:
- ...

Build:
PASS / FAIL

Tests:
PASS / FAIL

Known issues:
- ...

Next phase:
...
```

---

# FIRST TASK

Do NOT implement the complete application yet.

Start with Phase 0 only.

Inspect the Linux development environment and repository.

Determine which required tools are already installed.

Do not install anything without first showing the required packages and asking for confirmation.

Then create:

```text
docs/architecture.md
docs/development.md
docs/protocol.md
README.md
```

and provide the proposed architecture.

Stop after Phase 0.

---

# 11. One change I'd make to that plan

Because your PC is **very low-end**, I would make the first MVP even smaller.

### MVP 1

Don't even bother with microphone + webcam simultaneously.

First:

```text
PHONE CAMERA
     ↓
    USB
     ↓
LINUX RECEIVER
     ↓
V4L2
     ↓
LINUX WEBCAM
```

Get that working.

Then add microphone.

Then add GUI.

Then optimize.

That gives us a very clear progression:

```text
MVP 1
USB connection
        ↓
MVP 2
Camera streaming
        ↓
MVP 3
Virtual webcam
        ↓
MVP 4
Microphone
        ↓
MVP 5
Virtual microphone
        ↓
MVP 6
GUI
        ↓
MVP 7
.deb + .apk
        ↓
MVP 8
Performance optimization
```

### And there's one important USB detail

Android has two relevant USB models: **USB Host** and **USB Accessory**. In Host mode, Android acts as the USB host; in Accessory mode, the external device acts as the host and Android is the device. ([Android Developers][1])

For our design, **the Linux PC should be the USB host and the Android phone should be the USB device**, so OpenCode needs to investigate the appropriate Android/Linux USB transport during Phase 0 rather than blindly assuming ordinary USB serial communication will work.

That investigation is important because **"connected by USB" doesn't automatically mean two applications can just open a TCP socket over the cable**.

---

# 12. How you should work with OpenCode

Since you're new to software development, use OpenCode almost like a senior developer working beside you.

Don't say:

> "Make the whole PhoneBridge app."

Instead:

```text
Read the project specification in README.md.

We are currently working on Phase 0 only.

Inspect my system.

Do not modify application code yet.

Tell me:
1. What is already installed?
2. What is missing?
3. What USB architecture do you recommend?
4. Can my Linux machine support the required multimedia stack?
5. What are the biggest technical risks?

Then create the Phase 0 documentation.

Stop.
```

Then, once that's done:

```text
Proceed to Phase 1.

Do not skip any tests.

Build the Android project from the command line.

Do not require Android Studio.

Stop if the APK cannot be built.
```

Then Phase 2, Phase 3, etc.

**This is much safer than letting an AI generate 20,000 lines of code in one shot.**

And importantly, Debian's current packaging documentation recommends the `debhelper`/`dh` workflow for normal Debian packaging, so our final `.deb` should follow that rather than being a hand-built archive. ([Debian][2])

The next thing I'd do is **Phase 0 on your actual Linux machine**: determine exactly what Linux Mint, Java, Gradle, Android SDK, FFmpeg, PipeWire, libusb and v4l2loopback capabilities you already have. That lets us design PhoneBridge around what your old PC can actually handle rather than guessing.

[1]: https://developer.android.com/develop/connectivity/usb/accessory?utm_source=chatgpt.com "USB accessory overview  |  Connectivity  |  Android Developers"
[2]: https://www.debian.org/doc/manuals/debmake-doc/ch06.en.html?utm_source=chatgpt.com "Chapter 6. Basics for packaging"

