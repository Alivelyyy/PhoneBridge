# Troubleshooting

## Virtual Camera Not Appearing

**Symptom:** `/dev/video*` does not exist or PhoneBridge Camera is not listed.

**Solution:**
```bash
# Load the v4l2loopback module
sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera" exclusive_caps=1

# Verify it loaded
ls /dev/video*
v4l2-ctl --list-devices
```

**Make it persistent across reboots:**
```bash
echo "v4l2loopback" | sudo tee /etc/modules-load.d/v4l2loopback.conf
echo "options v4l2loopback devices=1 video_nr=0 card_label=\"PhoneBridge Camera\" exclusive_caps=1" | sudo tee /etc/modprobe.d/v4l2loopback.conf
```

## No Audio from Virtual Microphone

**Symptom:** PhoneBridge Microphone does not appear in audio applications.

**Solution:**
```bash
# Check PipeWire is running
systemctl --user status pipewire

# Restart PipeWire if needed
systemctl --user restart pipewire

# List audio devices
pw-cli list-objects
```

## USB Device Not Detected

**Symptom:** PhoneBridge does not detect the Android device.

**Solution:**
```bash
# Check USB connection
lsusb

# Check for Android device (usually Vendor ID 18D1 for Google)
lsusb | grep -i android

# Check ADB sees the device
adb devices

# Ensure USB debugging is enabled on the phone
# Settings > Developer Options > USB Debugging
```

## Applications Don't See the Camera

**Symptom:** OBS, Discord, Zoom don't list PhoneBridge Camera.

**Solution:**
1. Ensure v4l2loopback is loaded BEFORE starting the application
2. Restart the application after loading the module
3. Check the application's video device settings

## High CPU Usage

**Symptom:** PhoneBridge uses excessive CPU.

**Solutions:**
- Lower resolution: Use 640x480 instead of 1280x720
- Lower frame rate: Use 15 FPS instead of 30 FPS
- Check for hardware H.264 decode support:
  ```bash
  vainfo  # Check VA-API support
  vdpauinfo  # Check VDPAU support
  ```

## USB Permission Denied

**Symptom:** Cannot open USB device.

**Solution:**
```bash
# Add yourself to the plugdev group
sudo usermod -aG plugdev $USER

# Create udev rule for Android devices
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/51-android.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Protocol Version Mismatch

**Symptom:** ERROR message with VERSION_MISMATCH.

**Solution:**
Ensure both Android and Linux applications are from the same PhoneBridge release. Update both if needed.

## Decoder Failure

**Symptom:** Video appears corrupted or black.

**Solutions:**
- Check if the video frame is a keyframe (I-frame)
- Restart streaming from the Android side
- Verify FFmpeg is installed correctly:
  ```bash
  ffmpeg -version
  pkg-config --modversion libavcodec
  ```
