# Troubleshooting

## Linux

### "Failed to initialize USB (is libusb installed?)"

Install libusb:
```bash
sudo apt install libusb-1.0-0-dev
```

### "No device found"

- Ensure the Android app is running
- Connect the USB cable
- Check `lsusb` for the device
- Some phones require USB OTG support

### "Failed to open /dev/video0"

The v4l2loopback module must be loaded:
```bash
sudo modprobe v4l2loopback devices=1 video_nr=0 card_label="PhoneBridge Camera"
```

To load on boot, add to `/etc/modules`:
```
v4l2loopback
```

### "PipeWire virtual audio init failed"

Install PipeWire:
```bash
sudo apt install pipewire pipewire-pulse
```

Check PipeWire is running:
```bash
pw-cli ls Node
```

### Video is choppy or low FPS

- Reduce resolution: `phonebridge --width 320 --height 240`
- Reduce FPS: `phonebridge --fps 10`
- Check USB bandwidth (avoid USB hubs)
- Ensure the phone is not thermal-throttling

### High latency

- Use USB 3.0 port if available
- Close other USB devices
- Reduce video bitrate: `phonebridge --bitrate 500000`
- Check for WiFi interference (some USB 3.0 hubs)

### Device disconnects randomly

- Try a different USB cable
- Use a shorter cable
- Check USB power: some phones need more current than USB 2.0 provides
- Disable USB autosuspend:
  ```bash
  echo -1 | sudo tee /sys/bus/usb/devices/*/power/autosuspend
  ```

## Android

### "Camera permission is required"

Go to Settings > Apps > PhoneBridge > Permissions > Camera > Allow

### "Microphone permission denied"

Go to Settings > Apps > PhoneBridge > Permissions > Microphone > Allow

### "No USB devices found"

- Enable USB debugging in Developer Options
- Try a different USB port/cable
- Some phones need "File Transfer" USB mode

### "USB permission denied"

When connecting, Android shows a dialog asking to allow USB access. Tap "OK".

### App crashes on start

- Ensure Android 8.0+ (API 26)
- Check logcat: `adb logcat -s PhoneBridge`
- Reinstall the APK

## Virtual Camera

### Apps can't see the virtual camera

Ensure v4l2loopback is loaded and the device exists:
```bash
ls -la /dev/video*
v4l2-ctl --list-devices
```

### Wrong resolution

The virtual camera must be initialized before apps open it. Restart the app after loading v4l2loopback.

### Black screen in Zoom/OBS

Some apps need the first frame before showing video. Wait a few seconds or restart the stream.

## Building

### CMake can't find PipeWire

Install development headers:
```bash
sudo apt install libpipewire-0.3-dev
```

### FFmpeg compile errors

Ensure all FFmpeg dev packages are installed:
```bash
sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

### Android build fails

Ensure SDK and build tools are installed:
```bash
cd android && ./gradlew --version
sdkmanager --list
```
