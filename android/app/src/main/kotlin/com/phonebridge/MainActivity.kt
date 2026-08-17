package com.phonebridge

import android.Manifest
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "PhoneBridge"
        private const val ACTION_USB_PERMISSION = "com.phonebridge.USB_PERMISSION"
        private const val REQUEST_CAMERA_PERMISSION = 100
        private const val REQUEST_MIC_PERMISSION = 101
    }

    private lateinit var statusText: TextView
    private lateinit var deviceText: TextView
    private lateinit var videoText: TextView
    private lateinit var audioText: TextView
    private lateinit var connectButton: Button
    private lateinit var startButton: Button
    private lateinit var stopButton: Button

    private lateinit var usbManager: UsbManager
    private val usbConnection = UsbConnection()
    private var connectedDevice: UsbDevice? = null
    private var videoStream: VideoStream? = null
    private var audioStream: AudioStream? = null

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                ACTION_USB_PERMISSION -> {
                    synchronized(this) {
                        val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.let { onDevicePermissionGranted(it) }
                        } else {
                            updateStatus("USB permission denied")
                        }
                    }
                }
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                    if (device?.deviceId == connectedDevice?.deviceId) {
                        onDeviceDisconnected()
                    }
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.statusText)
        deviceText = findViewById(R.id.deviceText)
        videoText = findViewById(R.id.videoText)
        audioText = findViewById(R.id.audioText)
        connectButton = findViewById(R.id.connectButton)
        startButton = findViewById(R.id.startButton)
        stopButton = findViewById(R.id.stopButton)

        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager

        connectButton.setOnClickListener { findAndConnectDevice() }
        startButton.setOnClickListener { startStreaming() }
        stopButton.setOnClickListener { stopStreaming() }

        startButton.isEnabled = false
        stopButton.isEnabled = false

        setupUsbCallbacks()

        updateStatus("Disconnected")
        deviceText.text = "None"
        videoText.text = "Stopped"
        audioText.text = "Stopped"

        requestCameraPermission()
        requestMicPermission()
    }

    override fun onResume() {
        super.onResume()
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        registerReceiver(usbReceiver, filter)
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(usbReceiver)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_CAMERA_PERMISSION) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.i(TAG, "Camera permission granted")
            } else {
                Toast.makeText(this, "Camera permission is required", Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun requestCameraPermission() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.CAMERA), REQUEST_CAMERA_PERMISSION)
        }
    }

    private fun requestMicPermission() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_MIC_PERMISSION)
        }
    }

    private fun setupUsbCallbacks() {
        usbConnection.onConnected = {
            Log.i(TAG, "USB connection established")
            runOnUiThread {
                updateStatus("Connected")
                connectButton.isEnabled = false
                startButton.isEnabled = true
                videoText.text = "Ready"
                audioText.text = "Ready"
            }
            val deviceName = connectedDevice?.productName ?: "Android Device"
            usbConnection.sendHello(deviceName, cameraCount = 2, hasMic = true)
            Log.i(TAG, "Sent HELLO")
            usbConnection.startReceiveLoop()
        }

        usbConnection.onDisconnected = {
            runOnUiThread {
                updateStatus("Disconnected")
                deviceText.text = "None"
                videoText.text = "Stopped"
                audioText.text = "Stopped"
                connectButton.isEnabled = true
                startButton.isEnabled = false
                stopButton.isEnabled = false
            }
        }

        usbConnection.onPacketReceived = { header, payload ->
            when (header.type) {
                Protocol.MessageType.HELLO_ACK -> {
                    Log.i(TAG, "Received HELLO_ACK")
                    runOnUiThread {
                        videoText.text = "Streaming Ready"
                        audioText.text = "Streaming Ready"
                    }
                }
                Protocol.MessageType.VIDEO_CONFIG_ACK -> {
                    Log.i(TAG, "Received VIDEO_CONFIG_ACK")
                }
                Protocol.MessageType.AUDIO_CONFIG_ACK -> {
                    Log.i(TAG, "Received AUDIO_CONFIG_ACK")
                }
                Protocol.MessageType.PING -> {
                    Log.i(TAG, "Received PING")
                    usbConnection.sendPong()
                    Log.i(TAG, "Sent PONG")
                }
                Protocol.MessageType.ERROR -> {
                    Log.e(TAG, "Received ERROR from Linux")
                    runOnUiThread {
                        Toast.makeText(this@MainActivity, "Error from Linux device", Toast.LENGTH_SHORT).show()
                    }
                }
                else -> {
                    Log.w(TAG, "Unhandled message type: 0x${String.format("%02X", header.type)}")
                }
            }
        }

        usbConnection.onError = { error ->
            Log.e(TAG, error)
            runOnUiThread {
                updateStatus("Error: $error")
            }
        }
    }

    private fun findAndConnectDevice() {
        val deviceList = usbManager.deviceList
        if (deviceList.isEmpty()) {
            updateStatus("No USB devices found")
            return
        }

        val device = deviceList.values.firstOrNull()
        if (device == null) {
            updateStatus("No USB devices found")
            return
        }

        if (usbManager.hasPermission(device)) {
            onDevicePermissionGranted(device)
        } else {
            val permissionIntent = PendingIntent.getBroadcast(
                this, 0, Intent(ACTION_USB_PERMISSION),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
            )
            usbManager.requestPermission(device, permissionIntent)
            updateStatus("Requesting USB permission...")
        }
    }

    private fun onDevicePermissionGranted(device: UsbDevice) {
        connectedDevice = device
        deviceText.text = device.productName ?: "Android Device"
        Log.i(TAG, "USB permission granted for: ${device.productName}")

        if (!usbConnection.connect(usbManager, device)) {
            updateStatus("Failed to connect")
        }
    }

    private fun onDeviceDisconnected() {
        stopStreaming()
        usbConnection.disconnect()
        connectedDevice = null
    }

    private fun startStreaming() {
        if (!usbConnection.isConnected()) return
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestCameraPermission()
            return
        }

        val stream = VideoStream(this, this)
        stream.configure(640, 480, 15, 1_000_000, false)

        var frameId = 0
        stream.onFrameEncoded = { data, size, isKeyframe ->
            sendVideoFrame(data, size, isKeyframe, frameId++)
        }

        stream.onError = { error ->
            Log.e(TAG, error)
            runOnUiThread {
                videoText.text = "Error"
            }
        }

        videoStream = stream
        stream.start()

        // Send VIDEO_CONFIG
        val configPayload = ByteArray(19)
        java.nio.ByteBuffer.wrap(configPayload).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            putInt(640)       // width
            putInt(480)       // height
            putInt(15)        // fps
            putInt(1_000_000) // bitrate
            put(0)            // codec: H.264
            put(0)            // profile: Baseline
            putShort(30)      // keyframe interval
        }
        usbConnection.sendPacket(Protocol.MessageType.VIDEO_CONFIG, payload = configPayload)

        // Send START_STREAM
        usbConnection.sendPacket(Protocol.MessageType.START_STREAM, payload = byteArrayOf(0x03)) // video + audio

        // Start audio
        val audio = AudioStream()
        var audioFrameId = 0
        audio.onAudioData = { data, size ->
            sendAudioFrame(data, size, audioFrameId++)
        }
        audio.onError = { error -> Log.e(TAG, error) }
        audioStream = audio
        audio.start()

        // Send AUDIO_CONFIG
        val audioConfigPayload = ByteArray(5)
        java.nio.ByteBuffer.wrap(audioConfigPayload).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            put(0)    // sample rate: 0=16000
            put(1)    // channels: mono
            putShort(16) // bit depth
            put(0)    // format: PCM
        }
        usbConnection.sendPacket(Protocol.MessageType.AUDIO_CONFIG, payload = audioConfigPayload)

        runOnUiThread {
            startButton.isEnabled = false
            stopButton.isEnabled = true
            videoText.text = "Streaming 640x480@15fps"
            audioText.text = "Streaming 16kHz"
        }

        Log.i(TAG, "Started video streaming")
    }

    private fun stopStreaming() {
        videoStream?.stop()
        videoStream = null
        audioStream?.stop()
        audioStream = null

        if (usbConnection.isConnected()) {
            usbConnection.sendPacket(Protocol.MessageType.STOP_STREAM)
        }

        runOnUiThread {
            startButton.isEnabled = usbConnection.isConnected()
            stopButton.isEnabled = false
            videoText.text = "Stopped"
            audioText.text = "Stopped"
        }

        Log.i(TAG, "Stopped streaming")
    }

    private fun sendVideoFrame(data: ByteArray, size: Int, isKeyframe: Boolean, frameId: Int) {
        val headerSize = 9
        val payload = ByteArray(headerSize + size)
        java.nio.ByteBuffer.wrap(payload).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            putInt(frameId)
            putInt(size)
            put(if (isKeyframe) 1.toByte() else 0.toByte())
        }
        System.arraycopy(data, 0, payload, headerSize, size)
        val flags: Short = if (isKeyframe) Protocol.FLAG_KEYFRAME else 0
        usbConnection.sendPacket(Protocol.MessageType.VIDEO_FRAME, flags, payload)
    }

    private fun sendAudioFrame(data: ByteArray, size: Int, frameId: Int) {
        val headerSize = 8
        val payload = ByteArray(headerSize + size)
        java.nio.ByteBuffer.wrap(payload).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            putInt(frameId)
            putInt(size)
        }
        System.arraycopy(data, 0, payload, headerSize, size)
        usbConnection.sendPacket(Protocol.MessageType.AUDIO_FRAME, payload = payload)
    }

    private fun updateStatus(status: String) {
        statusText.text = "USB: $status"
    }
}
