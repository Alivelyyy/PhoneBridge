# PhoneBridge — Project Documentation

> Complete 5-page engineering documentation. Part of the Alivelyyy coding-history dossier (generated 2026-10-18).

## PhoneBridge

> *Turn your Android phone into a wired USB webcam and microphone for Linux. PhoneBridge streams H.264 video and PCM audio from an Android app over a raw USB cable (Accessory mode) into a C++17 Linux daemon that publishes a standard `/dev/videoX` v4l2loopback camera and a PipeWire virtual mic — no Wi-Fi, no cloud, no accounts, and software deliberately engineered to run on decade-old low-end hardware.*

| Fact | Value |
|---|---|
| GitHub URL | https://github.com/Alivelyyy/PhoneBridge |
| Visibility | Public |
| Created / last pushed | 2026-08-17 → 2026-08-17 |
| Primary language | C++ |
| All languages | Kotlin, CMake, C++, Shell |
| Total files | 65 |
| Total commits | 6 |
| Live site | Not deployed (installable desktop/mobile application) |
| Category | Systems programming — cross-platform hardware bridge (Android companion app + Linux daemon) |

### 📄 Page 1 — Overview & Purpose

PhoneBridge solves a problem that a surprising number of Linux users still have: getting a phone's excellent camera and microphone into a desktop application through the most reliable, lowest-latency, most privacy-preserving channel available — a physical USB cable. The README opens with a one-line pitch: **"Turn your Android phone into a USB webcam and microphone for your Linux PC."** It then explains the full mechanism in one sentence: *"PhoneBridge captures camera and microphone data on your Android device, streams it over a physical USB connection (Android Open Accessory mode), and exposes it to Linux applications as a standard virtual webcam (`/dev/videoX`) and virtual microphone (via PipeWire)."*

The point of differentiation is stated bluntly in the README's "Why PhoneBridge" section: *"Most USB webcam apps require Wi-Fi, cloud accounts, or heavy desktop frameworks."* PhoneBridge's counter-position is a four-bullet manifesto:

- **"USB-only — no network, no cloud, no accounts"**
- **"Lightweight — designed for low-end PCs (Intel Pentium, 4 GB RAM)"**
- **"Local — all processing stays on your devices"**
- **"Standard — exposes a regular v4l2 webcam and PipeWire mic source that any Linux app can use"**

The target audience is clear from those lines: owners of modest Debian-based PCs (README explicitly names "Linux Mint, Ubuntu, etc.") who want Discord/OBS/Zoom-quality camera input without buying hardware or trusting a vendor's background service. On the Linux side applications *"simply select `PhoneBridge Camera` or `PhoneBridge Microphone` as input devices"* — the hunt for "just another v4l2 device" is paid off entirely by standard kernel-visible interfaces rather than a proprietary SDK.

Architecturally the project is a deliberate two-platform system. The Android half (Kotlin) is a command-and-capture endpoint: it grabs frames with CameraX, encodes them with the hardware `MediaCodec` H.264 encoder, taps the mic via `AudioRecord`, and pushes bytes down the accessory-mode file descriptor. The Linux half (C++17) is a receiving pipeline: libusb reads bulk transfers, FFmpeg decodes the H.264 NAL data to RGB, v4l2loopback republishes it as a webcam, and PipeWire hosts a virtual microphone that any client can record from. The ASCII diagram in the README compresses the whole story: Android "Camera/Mic → Capture → H.264 / PCM Encoder → USB Accessory (AOA)" feeding the PC's "USB Receiver → Video Decoder / Audio → v4l2loopback / PipeWire → `/dev/videoX` / Virtual Mic → Discord / OBS / Zoom / etc".

What makes the project interesting as a body of work is that nothing is outsourced to an existing camera-bridge stack — there is no scrcpy, no OBS plugin, no uvc gadget trickery. The developer rolled a **custom binary transport protocol** (spec document in `protocol/protocol.md`), a handshake state machine, packet validation with CRC-32 checksums, fragmentation rules for large video frames, and an error-code taxonomy that both ends share. That's systems design from first principles, compressed into 65 files.

The git history — all 6 commits dated 2026-08-17, including **"Phase 4-9: USB handshake, video/audio pipeline, GUI"**, **"Phase 10/13/15: Sync tracking, config system, error handling"**, **"Phase 17/22: Debian packaging and documentation"**, and **"Phase 20-21: Unit tests, security hardening, packaging fixes"** — shows the repo was built phase-harness style and shipped as a coherent 1.0.0 release (the version is identical in `linux/CMakeLists.txt` (`project(PhoneBridge VERSION 1.0.0 ...)`), `debian/changelog` (`phonebridge (1.0.0-1) unstable`), and the Android `build.gradle.kts` (`versionName = "1.0.0"`). It is a single-day, phase-tagged system release rather than a long-lived repo — a "built the whole thing, then shipped it" pattern.

### 📄 Page 2 — Technology Stack & Architecture

**Languages & build systems.** The Linux program is modern C++17, built by CMake with a floor of CMake 3.16 and a default of `-Wall -Wextra -Wpedantic` (`linux/CMakeLists.txt`). The Android app is Kotlin on JVM 17 (`kotlinOptions { jvmTarget = "17" }`, `sourceCompatibility = JavaVersion.VERSION_17`), compiled against `compileSdk = 34`, `minSdk = 26`, `targetSdk = 34` in `android/app/build.gradle.kts`. Shell scripts (`scripts/build.sh`, `scripts/test.sh`, `scripts/package.sh`) and Coast-of-debian tooling (debhelper compat 13) round out the builds. The METADATA language makeup — C++ (primary), Kotlin, CMake, Shell — matches the file census: 14 `.cpp`, 9 `.h`, 3 `.kts` (Gradle Kotlin DSL), 5 `.kt`, 3 `.sh`, plus 10 `.xml` (Android layout/resource manifests) and 6 `.md` documents. Everything is CLI-buildable; `docs/development.md` states it explicitly: **"No Android Studio required. All builds are command-line only."**

**Linux dependency matrix** (pulled by `pkg_check_modules` in `linux/CMakeLists.txt` and mirrored in `debian/control`):

| Library | Role in PhoneBridge |
|---|---|
| `libusb-1.0` | USB enumeration, accessory-mode control transfer, bulk transfer read/write |
| `libavcodec` / `libavformat` / `libavutil` / `libswscale` / `libswresample` | FFmpeg H.264 decoding + YUV420P→RGB24 conversion |
| `gtk4` | The status GUI (`gtk_application_window`, CSS provider, grid layouts) |
| `libpipewire-0.3` (+ `spa-*` headers) | Virtual microphone stream (`pw_stream`, SPA audio pods) |
| `v4l2loopback-dkms` | Runtime kernel module that serves the `/dev/videoX` device |

`debian/control` marks the runtime dependency `Depends: ${shlibs:Depends}, ${misc:Depends}, v4l2loopback-dkms | v4l2loopback-utils` and `Recommends: pipewire, pipewire-pulse`.

**Android dependency matrix** (from `android/app/build.gradle.kts`): `androidx.core:core-ktx:1.12.0`, `androidx.appcompat:appcompat:1.6.1`, `com.google.android.material:material:1.11.0`, `androidx.constraintlayout:constraintlayout:2.1.4`, and the CameraX quartet `androidx.camera:camera-core / camera-camera2 / camera-lifecycle / camera-view` all at `1.3.1`. The manifest requires `<uses-feature android:name="android.hardware.usb.accessory" android:required="true" />` plus `CAMERA` and `RECORD_AUDIO` permissions.

**Repository layout** (65 files, all enumerated in the repo):

```
android/            kotlin app: settings/build gradle KTS, app/ source, res/, device_filter.xml
linux/              CMakeLists.txt, src/{main,usb,protocol,video,audio,virtual_camera,virtual_audio,config,gui},
                    include/phonebridge/*.h (9 headers), tests/ (6 files incl. test.h framework)
protocol/           protocol.md — the wire-spec document
docs/               architecture.md, development.md, protocol.md, troubleshooting.md
debian/             control, changelog, rules, install, compat, copyright, phonebridge.desktop, phonebridge.svg
scripts/            build.sh, test.sh, package.sh
README.md, LICENSE
```

**Deployment artifacts.** Two distributables: `phonebridge_1.0.0_amd64.deb` for Linux (`sudo apt install ./phonebridge_1.0.0_amd64.deb`) and `PhoneBridge.apk` for Android. `debian/install` ships `usr/bin/phonebridge`, `usr/share/applications/phonebridge.desktop`, and `usr/share/icons/hicolor/scalable/apps/phonebridge.svg`. `scripts/build.sh` automates both sides in one run: `cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)` then `./gradlew assembleDebug`, printing `"Build complete!"` at the end. There is no CI pipeline or hosted service — deployment means packaging and installing locally, which is the intended mode for this class of tool.

**End-to-end data-flow diagram** (straight from the README, condensed into two pipelines):

```
Video: Android Camera → YUV → MediaCodec H.264 → USB AOA → Linux FFmpeg decoder → v4l2loopback → /dev/videoX
Audio: Android Microphone → AudioRecord PCM → USB AOA → Linux receiver → PipeWire → PhoneBridge Microphone
```

### 📄 Page 3 — Features & Functionality (Feature-by-Feature)

**1. Wireless-free USB transport with automatic AOA mode switching.** When the user clicks Connect in the Linux GUI (`gui.onConnect` in `linux/src/main.cpp`), the daemon first scans for an already-accessory-mode device: `usb.scanForDevice()` matches `desc.idVendor == AOA_VID && isAoaPid(desc.idProduct)`, where `isAoaPid()` accepts all six Android accessory product IDs (`AOA_PID`, `AOA_ACCESSORY_PID`, `AOA_ADB_PID`, `AOA_AUDIO_PID`, `AOA_AUDIO_ADB_PID`, `AOA_AUDIO_ACCESSORY_PID`). If nothing matches, `switchToAoaMode()` opens each Android device and fires the accessory request — `libusb_control_transfer(tempHandle, 0x40, AOA_START_ACCESSORY, 1, 0, nullptr, 0, USB_TIMEOUT_MS)` — then re-enumerates up to **20 attempts × 500 ms** ("Device did not switch to AOA mode after 10 seconds"). This is the exact step the final commit headline refers to: **"Fix USB transport: switch to AOA accessory mode."**

**2. H.264 virtual webcam (`PhoneBridge Camera`).** The Android app binds a CameraX `ImageAnalysis` use case (rear camera by default, `STRATEGY_KEEP_ONLY_LATEST` backpressure), converts planes to NV12 (`imageProxyToNV12` reorders Y/V/U plane data byte-by-byte), and feeds a `MediaCodec` AVC encoder configured with `KEY_BIT_RATE=1_000_000`, `KEY_I_FRAME_INTERVAL=2`, `COLOR_FormatYUV420SemiPlanar`. On Linux, FFmpeg decodes the NAL data (`avcodec_find_decoder(AV_CODEC_ID_H264)`) and `sws_scale` converts YUV420P→RGB24 with `SWS_BILINEAR`, byte-for-byte written into the v4l2loopback device (`open(devicePath, O_WRONLY | O_NONBLOCK)`, `VIDIOC_S_FMT` with `V4L2_PIX_FMT_RGB24`). Apps then select "PhoneBridge Camera" (the card_label assigned via `modprobe v4l2loopback ... card_label="PhoneBridge Camera" exclusive_caps=1` in the README).

**3. PipeWire virtual microphone (`PhoneBridge Microphone`).** `linux/src/virtual_audio/virtual_audio.cpp` builds the complete PipeWire device graph in ~200 lines: a thread loop named `"phonebridge-audio"`, a `pw_context` connected to the core, and a `pw_stream` titled `"PhoneBridge Microphone"` in `PW_DIRECTION_OUTPUT` mode with `SPA_AUDIO_FORMAT_S16_LE` mono audio (`position[0] = SPA_AUDIO_CHANNEL_MONO`). A byte-ring buffer (capacity `sampleRate × channels × 2`) smooths USB arrivals; when the PipeWire process callback drains faster than USB fills it, the tail is zero-filled, and callback state transitions to `PW_STREAM_STATE_STREAMING` are logged.

**4. Custom protocol with handshake, keepalive & fragmentation.** The spec (`protocol/protocol.md`, duplicated in `docs/protocol.md`) defines a fixed **28-byte header**: `MAGIC (0x50424430 "PBD0")`, `VERSION (1)`, `TYPE`, `FLAGS`, `PAYLOAD_LENGTH`, `SEQUENCE`, `TIMESTAMP` (microseconds since epoch), `CHECKSUM (CRC-32 of PAYLOAD only)` — little-endian throughout. Message types span connection management (HELLO 0x01, HELLO_ACK 0x02, DEVICE_INFO 0x03, DEVICE_INFO_ACK 0x04), stream control (VIDEO_CONFIG 0x10 … AUDIO_FRAME 0x15, START_STREAM 0x16, STOP_STREAM 0x17), keepalive (PING 0x20/PONG 0x21 *"sent every 5 seconds if no other traffic"*), and one error code ERROR (0x30) whose payload enumerates 8 codes (VERSION_MISMATCH … RESOURCE_ERROR). Frames larger than one transfer use FRAGMENT/MORE flag rules and identical SEQUENCE on all fragments, with reassembly required on the receiver.

**5. GTK4 status GUI with live telemetry.** `linux/src/gui/gui.cpp` builds a 380×480 window titled "PhoneBridge" with styled `.title`/`.stat-label` CSS, framed sections "Connection" and "Stream", plus three Connect/Start/Stop buttons and live stat rows (FPS, latency, frames, dropped). The controller in `main.cpp` installs a `g_timeout_add(500, ...)` timer that republishes `GuiState` with counters (`g_videoFramesReceived`, `g_audioFramesReceived`, `g_droppedFrames — atomic counters incremented per frame`) and latency computed as `now - header.timestamp` in microseconds, converted to milliseconds. "Phase 10" sync tracking is exactly these atomics: `updateFpsCounter()` runs a 1-second window.

**6. Configurable, low-end-first defaults.** README configuration tables: Camera — Front/Rear (default Rear), Resolution 640×480 / 1280×720 (default 640×480), FPS 15 / 30 (default 15), Video Bitrate 500 Kbps / 1000 Kbps / 2000 Kbps (default 1000 Kbps); Audio — Microphone on/off (default Enabled), Sample Rate 16 kHz / 48 kHz (default 16 kHz), Channels Mono, Format 16-bit PCM, with the note *"Default settings are chosen for low-end hardware. Start with 640x480 at 15 FPS."* The matching CLI surface (`config.cpp parseArgs`) exposes `--width --height --fps --bitrate --sample-rate --channels --device`, a `-h/--help` text dump, and the `--device` override for the virtual camera path (default `/dev/video0`). Both ends acknowledge config over the wire before streaming (`VIDEO_CONFIG_ACK`, `AUDIO_CONFIG_ACK`).

**7. Disconnect resilience and clean teardown.** `usb.onDisconnected` in `main.cpp` stops the stream, clears all counters (`g_videoFramesReceived.store(0)`, etc.), shuts down camera/decoder/PipeWire, updates the GUI to `"Disconnected"` status and prints `"All streams stopped. Waiting for reconnection..."`; USB receives run on their own background thread with a 100 ms polling timeout so unplugging never wedges the UI thread. Per the spec's recovery plan, "Video decoder handles gaps via keyframe resync" and reconnect "triggers full handshake."

**8. Security hardening as a feature.** The README Security section: *"No cloud accounts or remote servers — No analytics, telemetry, or advertising — USB-only local communication via AOA — Packet lengths validated before memory allocation — No code execution from received data."* The code backs it up: `Protocol::validatePacket()` rejects wrong magic/version, oversized payloads (`MAX_PAYLOAD_SIZE` 1 MB, `MAX_FRAME_SIZE` 512 KB caps in `protocol.h`), header/size mismatches, unknown types (`typeVal == 0 || typeVal > 0x30`), and bad CRC-32; `handleVideoFrame` enforces `frameSize > MAX_FRAME_SIZE` plus a cumulative `g_frameBuffer.size() > MAX_FRAME_SIZE` overflow guard. Error replies are structurally validated too (`handleHello` requires `payloadLen >= 38` and sends `INVALID_PACKET` otherwise).

**9. Ten-step documented usage flow.** From README's Usage section (steps 1–10): modprobe the loopback, launch `phonebridge`, connect the phone via USB, launch and grant USB permission in the Android app, click Connect in Linux GUI ("it will automatically detect your phone and switch it to USB Accessory (AOA) mode"), press Start, then select **"PhoneBridge Camera"** in Discord/OBS/Zoom and **"PhoneBridge Microphone"** in any audio app.

### 📄 Page 4 — Code Internals & Systems Design

**Linux control flow (`linux/src/main.cpp`, 544 lines).** A file-scope global registry (`g_usb`, `g_gui`, `g_decoder`, `g_camera`, `g_virtualAudio`, `g_config` + a `std::vector<uint8_t> g_frameBuffer` for reassembly) keeps module wiring trivially explicit. `main()` parses config, initializes GUI and USB, prepares a decoder and the virtual mic, then binds four callbacks: `usb.onConnected` starts the receive thread and waits for HELLO; `usb.onDisconnected` performs the full teardown described above; `gui.onConnect` runs the AOA scan/switch/connect dance; `gui.onStop` sends `STOP_STREAM` and releases the video stack. Packet dispatch is a single `switch (header.type)` in `onPacketReceived` mapping each of the 10 handled types to a handler; HELLO validation checks version equality against `phonebridge::PROTOCOL_VERSION` and responds with `HELLO_ACK` carrying `maxPayload = 65536` and `streamFlags = 0x03` (video+audio).

**Transport layer (`linux/src/usb/usb_device.cpp`).** Pimpl-encapsulated `libusb` state (context, handle, discrete IN/OUT endpoints, sequence counter). Startup walks the active config descriptor hunting a pair of `LIBUSB_TRANSFER_TYPE_BULK` endpoints across `altsetting`s, claims the interface, then spawns `receiveLoop()` on a dedicated `std::thread` — reading raw 100 ms batches into a 64 KB buffer, calling `Protocol::validatePacket`, parsing the header, and dispatching `onPacketReceived`. Writes go through `libusb_bulk_transfer` with `LIBUSB_ERROR_TIMEOUT` tolerated, so a full endpoint queue degrades gracefully rather than erroring the session.

**Frame pipeline nuance worth reading.** `handleVideoFrame` implements reassembly manually: it accumulates `nalData` until `g_frameBuffer.size() >= g_currentFrameSize`, then `decoder->decodeFrame(...)` runs, `getDecodedFrame()` copies the RGB buffer into a stack-sized `uint8_t rgbBuffer[640 * 480 * 3]` and `camera->writeFrame(...)` publishes it. Decoding failures bump `g_droppedFrames` rather than tearing down the stream, and every video frame tracks microsecond latency by comparing `header.timestamp` (sender clock) to `getLocalTimeUs()`.

**Virtual device twin APIs.** Video writes non-blocking («`write(impl_->fd, data, length)`», tolerating `EAGAIN` on a full queue). Audio's pushRing deliberately evicts the oldest byte when full (`readPos` advances under the mutex) — a classic bounded-drop ring, appropriate because perceived audio staleness is worse than silence. Underruns on the PipeWire side zero-fill the requested chunk so the stream never reports gaps to consumers.

**Android internals.** `MainActivity` is the conductor: registers a `BroadcastReceiver` for `ACTION_USB_PERMISSION` / `ACTION_USB_ACCESSORY_ATTACHED|DETACHED`, requests CAMERA and RECORD_AUDIO permissions, and on *Start* assembles the pipeline in code — `stream.configure(640, 480, 15, 1_000_000, false)`, then hand-serialized config packets built with `java.nio.ByteBuffer.wrap(...).order(java.nio.ByteOrder.LITTLE_ENDIAN)` (19-byte `VIDEO_CONFIG` payload: 640/480/15/1_000_000/codec 0/profile 0/keyframe interval 30; 5-byte `AUDIO_CONFIG`: rate code 0=16 kHz, 1 channel, 16-bit, PCM). `UsbConnection` wraps `usbManager.openAccessory()` in `ParcelFileDescriptor` streams, writes packets under a `synchronized(out)` lock and runs a receive thread that loops incrementally filling `Protocol.HEADER_SIZE` and capping `payloadSize & 0x7FFFFFFF` at `Protocol.MAX_PAYLOAD_SIZE` before allocation. `Protocol.kt` mirrors the C++ header struct exactly (magic constant shared by both ends).

**Testing.** No external framework — `linux/tests/test.h` implements a ~60-line `TestRunner` singleton with `TEST_ASSERT`, `TEST_ASSERT_EQ`, `TEST_ASSERT_STREQ` macro families returning `bool` per test. Five suites cover the untouchable-by-hardware modules (`test_main.cpp`, `test_protocol.cpp`, `test_config.cpp`, `test_audio_handler.cpp`, `test_video_decoder.cpp`). The CMake test target links only `protocol.cpp config.cpp audio_handler.cpp video_decoder.cpp` — the headers' comment says why: *"only modules that don't require USB/GUI/hardware"*. Wired into `CTest` via `add_test(NAME phonebridge_tests ...)` and documented as `./phonebridge_tests` in the README. No CI service is configured in the repo; tests run locally via `scripts/test.sh`.

**Error handling style.** Three channels: OS-level (`fprintf(stderr, "[ERROR] ...")` with `strerror(errno)`), callback-level (`usb.onError`, `onError` lambdas on Android feeding GUI/Toast), and protocol-level (`sendError()` building a 6-byte+message ERROR packet with `codeVal` + `relatedSeq = 0`). Production code logs with the `[INFO]/[WARN]/[ERROR]` prefix convention consistently across C++ and Android (`Log.i(TAG, ...)`). Notably, `pipewire` init failure, decoder init failure, and camera reopen failure are treated as *soft* at startup (`"[WARN] ... (will reinit on VIDEO_CONFIG)"`) — the daemon degrades to "wait for correct config" rather than dying.

**Run instructions** (README): build via the cmake/gradle commands or `scripts/build.sh`; on Linux load `sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera" exclusive_caps=1`; run `phonebridge`; the troubleshooting section covers "Virtual camera not appearing" (ls /dev/video*), "No audio from virtual microphone" (`systemctl --user status pipewire`), "USB device not detected", "Applications don't see the camera" (restart app after module load), and "High CPU usage" (drop to 640×480@15, ensure hardware decode).

### 📄 Page 5 — Development Timeline, Skills & Resume Highlights

**Timeline.** Every one of the repo's 6 commits carries the same date — 2026-08-17 — which reads as a compressed, phase-tagged build-out sprint followed immediately by release packaging. In newest-first order, the GITLOG is:

1. `2026-08-17 Fix USB transport: switch to AOA accessory mode` — the newest commit is itself a protocol-layer correction: the transport originally used a different access path, and the fix migrated everything to Android Open Accessory bulk mode (visible in `switchToAoaMode()` being the sole path from "no device" → "connected").
2. `2026-08-17 Phase 20-21: Unit tests, security hardening, packaging fixes` — the test framework + validatePacket/MAX_* caps + debhelper polish.
3. `2026-08-17 Phase 17/22: Debian packaging and documentation` — `debian/*` (8 files) and the 4-document `docs/` folder.
4. `2026-08-17 Phase 10/13/15: Sync tracking, config system, error handling` — the Phase-10 telemetry atomics, `config.cpp` CLI flags, and the ERROR-message plumbing.
5. `2026-08-17 Implement PipeWire virtual mic, fix audio config, improve disconnect handling` — the `virtual_audio.cpp` module plus the disconnect-teardown flow.
6. `2026-08-17 Phase 4-9: USB handshake, video/audio pipeline, GUI` — the founding bulk: USB, decode/encode, v4l2loopback, Gtk builder.

Interesting historical inference: phase numbers start at 4, implying Phases 1-3 (ideation/design or work outside git) predate the initial push — the repo is a publicized snapshot of a development sequence, not its full extent. All work-streams (spec → Kotlin app → C++ pipeline → GUI → tests → packaging → docs) concluded inside the single release day, and the debian changelog entry `phonebridge (1.0.0-1) unstable; urgency=medium` is authored `Mon, 17 Aug 2026 00:00:00 +0000`, matching.

**Skills exercised (all evidenced in-code):**
- Cross-platform systems integration: Linux kernel (v4l2 ioctls, v4l2loopback module parameters) ↔ Android host (CameraX, MediaCodec, AudioRecord, UsbManager).
- Raw USB programming: libusb enumeration, control transfers for the AOA accessory switch (`AOA_START_ACCESSORY`), bulk IN/OUT, endpoint descriptor walking.
- Binary protocol engineering: fixed 28-byte headers, CRC-32 (polynomial `0xEDB88320`) serialized in C++ and Kotlin simultaneously.
- Multimedia: FFmpeg/libavcodec decoding pipeline, SwsScaler color-space conversion, PipeWire `pw_stream` with SPA audio pods and thread-loop safety.
- GUI with GTK4 in C (GObject signal handlers, CSS providers, `g_timeout_add` timers).
- Concurrency: atomic counters, `std::thread` receive loops, mutex-guarded ring buffers, Kotlin daemon threads with hand-rolled lifecycle joins.
- Security: protocol-level input validation, bounded allocations, checksum enforcement.
- Packaging & docs: full Debian packaging set (control/changelog/rules/install/desktop/compat/icon), user-facing README with ASCII architecture diagrams, deduplicated protocol spec in two locations.

**Copy-paste resume bullets:**
- Designed and shipped PhoneBridge, an open-source USB webcam/mic bridge that streams H.264 video and PCM audio from Android to Linux over AOA accessory mode, rendering apps-compatible `PhoneBridge Camera` / `PhoneBridge Microphone` devices with no network dependency.
- Authored a complete binary wire protocol (28-byte header, magic `0x50424430`, CRC-32 payload checksums, fragmented VIDEO_FRAME reassembly, 8-code error taxonomy) implemented in lockstep in C++17 and Kotlin.
- Built the Linux pipeline end-to-end with libusb, FFmpeg (avcodec H.264 decode + sws_scale YUV→RGB), v4l2loopback presentation, and a PipeWire virtual microphone using pw_stream/SPA with a mutex-guarded ring buffer and zero-fill underrun handling.
- Developed the Android companion app in Kotlin using CameraX ImageAnalysis + hardware MediaCodec H.264 encoding (NV12 plane conversion, keyframe flag propagation) and AudioRecord 16 kHz PCM capture over `UsbAccessory` file descriptors.
- Automated accessory-mode discovery: libusb device-list scanning across 6 Android accessory PIDs, `AOA_START_ACCESSORY` control transfer, and a 20 × 500 ms re-enumeration retry window.
- Added a GTK4 status GUI with live FPS/latency/frame-count/dropped-frame telemetry via 500 ms GTK timers and atomic Phase-10 sync counters.
- Hardened the transport: magic/version/type check, 1 MB payload and 512 KB frame ceilings validated before allocation, cumulative buffer-overflow guards, checksum rejection, and structured ERROR replies.
- Delivered 1.0.0 as a Debian package (debhelper 13, install/desktop/icon assets) plus unit-test suites for protocol/config/decoder modules using a hand-rolled C++ test framework and CTest integration.

**Limitations & future work (observable in code):** config is transport-fixed on the Android side (`startStreaming` always sends 640×480@15 with mono 16 kHz, though `VideoStream.configure()` accepts front camera); `DEVICE_INFO`/`DEVICE_INFO_ACK` are defined in the spec and enum but unhandled in `main.cpp`'s dispatch switch; the GUI's Start button currently only logs (`gui.onStart = [&usb]() { printf("[INFO] Start streaming\n"); }`), leaving START_STREAM control to the Android app; no CI; no front/back camera switching UI on Linux. These are natural next-phase items.

**Maturity assessment:** a complete MVP / working release. Version 1.0.0 is consistent across CMake, Debian and Gradle, README + troubleshooting + architecture + development + protocol documentation are finished, unit tests and packaging exist, and the very latest commit is a bugfix on the transport — the classic signature of a project that was finished and then maintained. Best described as: *shipped, scoped, and coherent.*
