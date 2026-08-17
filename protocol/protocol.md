# PhoneBridge USB Protocol Specification

## Overview

PhoneBridge uses a custom binary protocol over USB to communicate between the Android application and the Linux application. This document defines the protocol format, message types, and behavior.

## Transport

- **Physical:** USB 2.0 (or higher)
- **Mode:** Android USB Accessory (AOA)
- **Endpoints:** USB bulk transfer endpoints
- **Linux side:** libusb
- **Android side:** UsbAccessory APIs

## Byte Order

All multi-byte integers are transmitted in **little-endian** byte order.

## Packet Format

Every packet has a fixed-size header followed by a variable-length payload.

```
Offset  Size    Field
──────  ────    ─────
0       4       MAGIC
4       1       VERSION
5       1       TYPE
6       2       FLAGS
8       4       PAYLOAD_LENGTH
12      4       SEQUENCE
16      8       TIMESTAMP
24      4       CHECKSUM
──────────────────────
28      N       PAYLOAD
```

**Total header size: 28 bytes**

### Field Descriptions

| Field | Size | Description |
|---|---|---|
| MAGIC | 4 bytes | `0x50424430` ("PBD0") — identifies PhoneBridge packets |
| VERSION | 1 byte | Protocol version (currently `1`) |
| TYPE | 1 byte | Message type (see Message Types) |
| FLAGS | 2 bytes | Control flags (bit field) |
| PAYLOAD_LENGTH | 4 bytes | Length of payload in bytes |
| SEQUENCE | 4 bytes | Packet sequence number (incrementing) |
| TIMESTAMP | 8 bytes | Microseconds since Unix epoch |
| CHECKSUM | 4 bytes | CRC-32 of PAYLOAD only (0 if no payload) |

### MAGIC

The magic bytes `0x50424430` correspond to ASCII "PBD0" (PhoneBridge Data, version 0). This allows the receiver to:

- Detect packet boundaries in the USB stream
- Reject garbage data
- Sync to the protocol stream after errors

### VERSION

The version field allows protocol evolution. Current version is `1`.

- Receiver MUST reject packets with unknown versions
- Receiver MUST send ERROR with VERSION_MISMATCH for unsupported versions

### FLAGS

Bit field:

| Bit | Name | Description |
|---|---|---|
| 0 | FRAGMENT | This packet is a fragment of a larger message |
| 1 | MORE | More fragments follow |
| 2 | KEYFRAME | Video keyframe (I-frame) |
| 3-15 | Reserved | Must be 0 |

### CHECKSUM

CRC-32 checksum of the PAYLOAD bytes only. If PAYLOAD_LENGTH is 0, CHECKSUM must be 0.

Receiver MUST verify the checksum. If it fails, the packet is discarded and an ERROR is sent.

## Message Types

### Connection Management

| Type | Name | Direction | Description |
|---|---|---|---|
| 0x01 | HELLO | Android → Linux | Initial connection request |
| 0x02 | HELLO_ACK | Linux → Android | Connection accepted |
| 0x03 | DEVICE_INFO | Android → Linux | Device capabilities |
| 0x04 | DEVICE_INFO_ACK | Linux → Android | Capabilities acknowledged |

### Stream Control

| Type | Name | Direction | Description |
|---|---|---|---|
| 0x10 | VIDEO_CONFIG | Android → Linux | Video stream configuration |
| 0x11 | VIDEO_CONFIG_ACK | Linux → Android | Video config accepted |
| 0x12 | VIDEO_FRAME | Android → Linux | Video frame data |
| 0x13 | AUDIO_CONFIG | Android → Linux | Audio stream configuration |
| 0x14 | AUDIO_CONFIG_ACK | Linux → Android | Audio config accepted |
| 0x15 | AUDIO_FRAME | Android → Linux | Audio frame data |
| 0x16 | START_STREAM | Bidirectional | Begin streaming |
| 0x17 | STOP_STREAM | Bidirectional | Stop streaming |

### Keepalive

| Type | Name | Direction | Description |
|---|---|---|---|
| 0x20 | PING | Bidirectional | Connection keepalive |
| 0x21 | PONG | Bidirectional | Keepalive response |

### Error

| Type | Name | Direction | Description |
|---|---|---|---|
| 0x30 | ERROR | Bidirectional | Error condition |

## Message Definitions

### HELLO (0x01)

Sent by Android after USB connection is established.

**Payload:**
```
Offset  Size    Field
0       4       PROTOCOL_VERSION
4       32      DEVICE_NAME (UTF-8, null-terminated)
36      1       CAMERA_COUNT
37      1       HAS_MICROPHONE
```

**Example:**
```
Protocol version: 1
Device name: "Pixel 6"
Camera count: 2
Has microphone: true
```

### HELLO_ACK (0x02)

Sent by Linux after validating HELLO.

**Payload:**
```
Offset  Size    Field
0       4       ACCEPTED_VERSION
4       4       MAX_PAYLOAD_SIZE
8       4       STREAM_FLAGS
```

**STREAM_FLAGS:**
| Bit | Name | Description |
|---|---|---|
| 0 | SUPPORTS_VIDEO | Linux can receive video |
| 1 | SUPPORTS_AUDIO | Linux can receive audio |

### DEVICE_INFO (0x03)

Sent by Android after HELLO_ACK, providing detailed capabilities.

**Payload:**
```
Offset  Size    Field
0       1       CAMERA_ID
1       4       WIDTH
5       4       HEIGHT
9       4       FPS
13      4       BITRATE
17      1       ENCODING (0=H264, 1=VP8, 2=VP9)
18      1       AUDIO_SAMPLE_RATE
19      1       AUDIO_CHANNELS
20      2       AUDIO_BIT_DEPTH
```

### VIDEO_CONFIG (0x10)

Sent before video streaming begins.

**Payload:**
```
Offset  Size    Field
0       4       WIDTH
4       4       HEIGHT
8       4       FPS
12      4       BITRATE
16      1       CODEC (0=H264)
17      1       PROFILE (0=Baseline)
18      2       KEYFRAME_INTERVAL
```

### VIDEO_FRAME (0x12)

Contains video frame data. May be fragmented.

**Payload:**
```
Offset  Size    Field
0       4       FRAME_ID
4       4       FRAME_SIZE
8       1       FRAME_TYPE (0=P-frame, 1=I-frame)
9       N       NAL_DATA
```

For fragmented frames, the NAL_DATA is split across multiple VIDEO_FRAME packets with the same FRAME_ID. The FRAGMENT and MORE flags indicate fragmentation.

### AUDIO_CONFIG (0x13)

Sent before audio streaming begins.

**Payload:**
```
Offset  Size    Field
0       1       SAMPLE_RATE (0=16000, 1=48000)
1       1       CHANNELS
2       2       BIT_DEPTH
4       1       FORMAT (0=PCM)
```

### AUDIO_FRAME (0x15)

Contains audio frame data.

**Payload:**
```
Offset  Size    Field
0       4       FRAME_ID
4       4       FRAME_SIZE
8       N       PCM_DATA
```

### START_STREAM (0x16)

Signals the start of streaming.

**Payload:**
```
Offset  Size    Field
0       1       STREAM_FLAGS
```

**STREAM_FLAGS:**
| Bit | Name | Description |
|---|---|---|
| 0 | ENABLE_VIDEO | Start video stream |
| 1 | ENABLE_AUDIO | Start audio stream |

### STOP_STREAM (0x17)

Signals the stop of streaming.

**Payload:** None (PAYLOAD_LENGTH = 0)

### PING (0x20) / PONG (0x21)

Keepalive messages. Sent every 5 seconds if no other traffic.

**Payload:** None

### ERROR (0x30)

Reports an error condition.

**Payload:**
```
Offset  Size    Field
0       2       ERROR_CODE
2       4       RELATED_SEQUENCE
6       N       ERROR_MESSAGE (UTF-8, null-terminated)
```

**Error Codes:**
| Code | Name | Description |
|---|---|---|
| 0x01 | VERSION_MISMATCH | Protocol version not supported |
| 0x02 | INVALID_PACKET | Malformed packet received |
| 0x03 | CHECKSUM_ERROR | Payload checksum failed |
| 0x04 | UNSUPPORTED_CODEC | Requested codec not available |
| 0x05 | CAMERA_ERROR | Camera failed to open |
| 0x06 | MICROPHONE_ERROR | Microphone failed to open |
| 0x07 | DECODER_ERROR | Video decoder error |
| 0x08 | RESOURCE_ERROR | System resource unavailable |

## Connection Sequence

```
1. USB Physical Connection
   Android detected as USB accessory by Linux

2. USB Enumeration
   Linux opens device with libusb
   Linux claims interface

3. Protocol Handshake
   Android → HELLO
   Linux → HELLO_ACK
   Android → DEVICE_INFO
   Linux → DEVICE_INFO_ACK

4. Stream Configuration
   Android → VIDEO_CONFIG
   Linux → VIDEO_CONFIG_ACK
   Android → AUDIO_CONFIG
   Linux → AUDIO_CONFIG_ACK

5. Streaming
   Android → START_STREAM
   Android → VIDEO_FRAME (repeated)
   Android → AUDIO_FRAME (repeated)
   Android → PING (if idle)

6. Stop
   Android → STOP_STREAM
   Or USB disconnection detected
```

## Fragmentation

Large packets (typically video frames) may be fragmented across multiple USB transfers.

**Fragmentation rules:**
- Each fragment has the same SEQUENCE number
- FRAGMENT flag is set on all fragments except the first
- MORE flag is set on all fragments except the last
- All fragments of a frame share the same FRAME_ID
- Receiver must reassemble fragments before processing

**Maximum payload size:** Negotiated during HELLO handshake (MAX_PAYLOAD_SIZE).

## Error Recovery

### Packet Loss

- Receiver detects via SEQUENCE gaps
- Missing packets are logged
- Video decoder handles gaps via keyframe resync
- Audio uses PLC (Packet Loss Concealment) or silence

### Checksum Failure

- Packet is discarded
- ERROR message sent with CHECKSUM_ERROR
- Sender may retry the packet

### USB Disconnection

- All streaming stops
- Virtual devices retain last state
- Application returns to disconnected state
- Reconnection triggers full handshake

## Security Considerations

- Protocol is local USB-only, no network exposure
- Packet lengths validated before memory allocation
- Payload lengths from USB are never trusted blindly
- No code execution from received data
- Checksums prevent accidental corruption
