# PhoneBridge Architecture

## Overview

PhoneBridge is a two-component system that turns an Android phone into a USB webcam and microphone for a Linux PC. The Android device captures camera and microphone data, encodes it, and streams it over USB. The Linux application receives the data, decodes it, and exposes it to Linux applications as standard virtual video and audio devices.

## System Architecture

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

## USB Transport Layer

### USB Mode Selection

Android supports two USB communication modes:

1. **USB Host Mode** — Android acts as the USB host
2. **USB Accessory Mode** — External device acts as the host, Android is the device

For PhoneBridge, the **Linux PC must be the USB host** and the **Android phone must be the USB device**. This means we use **Android USB Accessory mode** (also called Android Open Accessory / AOA).

In AOA mode:
- The Linux PC enumerates the Android device as a USB accessory
- The Android device presents itself as a USB accessory when no driver claims it
- Bidirectional communication happens over USB bulk endpoints
- The Linux side uses `libusb` to communicate with the device
- The Android side uses the `UsbManager` and `UsbAccessory` APIs

### Why Not USB Serial (CDC ACM)

USB serial (CDC ACM) creates a `/dev/ttyACM*` device on Linux, which is simple but:
- Not all Android devices support CDC ACM natively
- Bandwidth may be limited compared to bulk transfers
- Less control over the protocol

AOA with bulk endpoints gives us:
- Higher throughput
- More control over the protocol
- Better compatibility with Android devices

### USB Communication Flow

```
Android                          Linux
────────                         ─────
Detect USB connection
Request USB permission
Open UsbAccessoryConnection
Create input/output streams
                                  Detect USB accessory
                                  Open with libusb
                                  Claim interface
Send HELLO ──────────────────────> Receive HELLO
                                  Validate protocol
<──────────────────────────────── Send HELLO_ACK
Receive HELLO_ACK
Protocol negotiation complete
                                  ─── Streaming begins ───
Send VIDEO_CONFIG ───────────────> Receive VIDEO_CONFIG
Send VIDEO_FRAME ────────────────> Receive VIDEO_FRAME
Send AUDIO_CONFIG ───────────────> Receive AUDIO_CONFIG
Send AUDIO_FRAME ────────────────> Receive AUDIO_FRAME
Send PING ───────────────────────> Receive PING
<──────────────────────────────── Send PONG
```

## Video Pipeline

### Android Side

```
Camera (CameraX)
    │
    ▼
YUV Frame (NV12/NV21)
    │
    ▼
MediaCodec H.264 Encoder
    │
    ▼
H.264 NAL Units
    │
    ▼
Packetizer (PhoneBridge Protocol)
    │
    ▼
USB Bulk Transfer
```

**Camera Configuration:**
- CameraX for camera lifecycle management
- Resolution: 640x480 initially (1280x720 later)
- Frame rate: 15 FPS initially (30 FPS later)
- Format: NV12 or NV21 (YUV)

**Encoding:**
- Android MediaCodec hardware encoder
- H.264 Baseline profile
- Bitrate: 1 Mbps initially
- Keyframe interval: 1 second
- No B-frames for low latency

### Linux Side

```
USB Bulk Transfer
    │
    ▼
Packet Parser (PhoneBridge Protocol)
    │
    ▼
H.264 NAL Unit Reassembly
    │
    ▼
FFmpeg/libavcodec H.264 Decoder
    │
    ▼
Raw Video Frame (YUV)
    │
    ▼
Color Space Conversion (swscale)
    │
    ▼
Raw Video Frame (RGB/YUYV)
    │
    ▼
v4l2loopback Device Write
    │
    ▼
/dev/videoX
    │
    ▼
Linux Applications
```

**Decoding:**
- FFmpeg libavcodec for H.264 decoding
- Software decoding (target PC has no hardware decoder)
- Output format: YUYV or RGB for v4l2loopback compatibility

## Audio Pipeline

### Android Side

```
Microphone
    │
    ▼
AudioRecord
    │
    ▼
PCM Samples (16-bit, mono, 16kHz)
    │
    ▼
Packetizer (PhoneBridge Protocol)
    │
    ▼
USB Bulk Transfer
```

**Audio Configuration:**
- Sample rate: 16000 Hz initially (48000 Hz later)
- Channels: 1 (mono)
- Bit depth: 16-bit signed
- Format: PCM (raw, uncompressed initially)
- Bitrate: 256 Kbps (16000 × 16 × 1)

### Linux Side

```
USB Bulk Transfer
    │
    ▼
Packet Parser (PhoneBridge Protocol)
    │
    ▼
PCM Frame Assembly
    │
    ▼
PipeWire Virtual Source
    │
    ▼
PhoneBridge Microphone
    │
    ▼
Linux Applications
```

**PipeWire Integration:**
- Create a PipeWire node as a virtual audio source
- Expose as "PhoneBridge Microphone"
- Applications can select it as an input device
- Use PipeWire's SPA (Simple Plugin API) for node creation

## Virtual Device Layer

### Virtual Webcam (v4l2loopback)

`v4l2loopback` is a kernel module that creates virtual video devices. It:
- Creates `/dev/videoX` devices
- Acts as a video sink (writers feed frames, readers consume frames)
- Supports `VIDIOC_FMT` negotiation
- Works with any V4L2-compatible application

**Setup:**
```bash
sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera" exclusive_caps=1
```

**Frame Writing:**
- Open `/dev/videoX`
- Set format via `VIDIOC_S_FMT` (YUYV or RGB)
- Write frames with `write()` or `VIDIOC_QBUF`/`VIDIOC_DQBUF`
- Frames appear to readers immediately

### Virtual Microphone (PipeWire)

PipeWire's virtual source creates a software audio device that:
- Appears as a microphone to applications
- Accepts PCM audio data from the PhoneBridge process
- Uses PipeWire's node API for creation

**Setup:**
- PhoneBridge creates a PipeWire node via the SPA API
- Node appears as "PhoneBridge Microphone"
- Applications see it as an available input device

## Protocol Layer

PhoneBridge uses a custom binary protocol over USB. Every message has a fixed header with:

- Magic bytes for packet identification
- Protocol version for compatibility
- Message type for routing
- Flags for control bits
- Payload length for framing
- Sequence number for ordering
- Timestamp for synchronization

The protocol is documented in detail in [protocol.md](../protocol/protocol.md).

## Error Handling

### USB Disconnection

When the phone is disconnected:
1. USB read/write fails
2. Linux application detects the error
3. Streaming stops gracefully
4. Virtual devices remain (applications see frozen last frame or silence)
5. GUI shows "Disconnected" state
6. Application waits for reconnection

### Decoder Failure

If H.264 decoding fails:
1. Log the error with frame details
2. Skip the corrupted frame
3. Continue decoding from next keyframe
4. Report dropped frames in statistics

### PipeWire Failure

If PipeWire is unavailable:
1. Log the error
2. Disable audio streaming
3. Continue video-only mode
4. Report audio status as "Unavailable"

## Performance Considerations

### Target Hardware

- Intel Pentium N3700 (4 cores, 1.6 GHz)
- 4 GB RAM
- No hardware video decode
- Limited USB 3.0 bandwidth

### Optimization Strategy

1. **Use hardware encoding on Android** — MediaCodec uses phone's DSP
2. **Software decoding on Linux** — No hardware decoder available
3. **Low resolution** — 640x480 minimizes decode overhead
4. **Low frame rate** — 15 FPS reduces CPU load
5. **Minimal buffering** — Reduce latency
6. **Efficient memory** — Avoid unnecessary copies

### Expected Resource Usage

| Metric | Target |
|---|---|
| CPU (video decode) | 15-25% |
| CPU (total) | 20-35% |
| RAM | 50-100 MB |
| USB bandwidth | 1-2 Mbps |
| Latency | < 200 ms |
