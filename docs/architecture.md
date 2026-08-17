# PhoneBridge Architecture

## Overview

PhoneBridge turns an Android phone into a USB webcam and microphone for Linux. It streams H.264 video and PCM audio over USB and exposes them as standard Linux devices.

## Components

```
┌─────────────────────────────────────────┐
│              Android App                │
│  ┌──────────┐  ┌──────────┐            │
│  │  Camera   │  │  Mic     │            │
│  │  (CameraX)│  │ (AudioRec)│           │
│  └────┬─────┘  └────┬─────┘            │
│       │              │                  │
│  ┌────▼─────┐  ┌────▼─────┐            │
│  │ H.264    │  │ PCM 16bit│            │
│  │ Encoder  │  │ Raw      │            │
│  │(MediaCodec)│ │          │            │
│  └────┬─────┘  └────┬─────┘            │
│       │              │                  │
│  ┌────▼──────────────▼─────┐           │
│  │    USB Bulk Transfer     │           │
│  │    (Binary Protocol)     │           │
│  └────────────┬────────────┘           │
└───────────────┼─────────────────────────┘
                │ USB Cable
┌───────────────┼─────────────────────────┐
│  ┌────────────▼────────────┐           │
│  │    USB Bulk Transfer     │           │
│  │    (libusb)              │           │
│  └────────────┬────────────┘           │
│       ┌───────┴────────┐               │
│  ┌────▼─────┐  ┌───────▼──────┐       │
│  │ Protocol │  │ Protocol      │       │
│  │ Parser   │  │ Validator     │       │
│  └────┬─────┘  └───────┬──────┘       │
│       │                │               │
│  ┌────▼─────┐  ┌───────▼──────┐       │
│  │ H.264    │  │ PCM Audio     │       │
│  │ Decoder  │  │ Handler       │       │
│  │(FFmpeg)  │  │               │       │
│  └────┬─────┘  └───────┬──────┘       │
│       │                │               │
│  ┌────▼─────┐  ┌───────▼──────┐       │
│  │ v4l2     │  │ PipeWire      │       │
│  │ loopback │  │ Audio Source  │       │
│  └────┬─────┘  └───────┬──────┘       │
│       │                │               │
│  ┌────▼─────┐  ┌───────▼──────┐       │
│  │ /dev/    │  │ Virtual       │       │
│  │ videoX   │  │ Microphone    │       │
│  └──────────┘  └──────────────┘       │
│              Linux App                 │
└─────────────────────────────────────────┘
```

## Data Flow

### Video

1. Android CameraX captures frames from the phone camera
2. MediaCodec encodes frames to H.264 (Baseline profile)
3. H.264 NAL units are packetized with frame metadata (ID, size, type)
4. Packets are sent over USB bulk transfer using the PhoneBridge protocol
5. Linux receives packets, validates checksums, reassembles fragments
6. FFmpeg decodes H.264 to RGB24
7. RGB frames are written to v4l2loopback device (`/dev/videoX`)
8. Standard Linux apps (Zoom, OBS, etc.) read from the virtual camera

### Audio

1. Android AudioRecord captures PCM at 16kHz mono, 16-bit
2. PCM data is packetized with frame metadata
3. Packets are sent over USB bulk transfer
4. Linux receives packets, validates checksums
5. PCM data is pushed into a PipeWire ring buffer
6. PipeWire stream (Audio/Source) feeds the data to applications
7. Standard Linux apps see "PhoneBridge Microphone" as an audio source

### Handshake

```
Android                              Linux
   │                                    │
   │──── HELLO (version, name) ───────>│
   │<─── HELLO_ACK (accepted, caps) ───│
   │                                    │
   │──── VIDEO_CONFIG (res, fps) ─────>│
   │<─── VIDEO_CONFIG_ACK ─────────────│
   │──── AUDIO_CONFIG (rate, ch) ─────>│
   │<─── AUDIO_CONFIG_ACK ─────────────│
   │                                    │
   │──── START_STREAM (flags) ────────>│
   │──── VIDEO_FRAME (repeated) ──────>│
   │──── AUDIO_FRAME (repeated) ──────>│
   │──── PING (if idle) ──────────────>│
   │<─── PONG ─────────────────────────│
   │                                    │
   │──── STOP_STREAM ─────────────────>│
   │                                    │
   │     [USB disconnect/reconnect]     │
```

## Protocol

Binary protocol over USB bulk endpoints. 28-byte header, variable payload.

| Field | Size | Description |
|-------|------|-------------|
| MAGIC | 4 | `0x50424430` ("PBD0") |
| VERSION | 1 | Protocol version (currently 1) |
| TYPE | 1 | Message type |
| FLAGS | 2 | Control flags |
| PAYLOAD_LENGTH | 4 | Payload size in bytes |
| SEQUENCE | 4 | Incrementing packet counter |
| TIMESTAMP | 8 | Microseconds since Unix epoch |
| CHECKSUM | 4 | CRC-32 of payload |

All integers are little-endian. See `protocol/protocol.md` for full specification.

## Build System

### Linux

- C++17, CMake 3.16+
- Dependencies: libusb, FFmpeg (libavcodec/avutil/swscale), GTK4, PipeWire
- Build: `cd linux && mkdir build && cd build && cmake .. && make`

### Android

- Kotlin, Gradle 8.2.2, SDK 34
- Dependencies: CameraX, MediaCodec, USB API
- Build: `cd android && ./gradlew assembleDebug`

### Debian Package

- Build: `dpkg-buildpackage -us -uc`
- Install: `sudo apt install ../phonebridge_1.0.0_amd64.deb`
