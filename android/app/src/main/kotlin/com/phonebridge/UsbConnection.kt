package com.phonebridge

import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean

class UsbConnection {
    companion object {
        private const val TAG = "UsbConnection"
        private const val USB_BUFFER_SIZE = 65536
    }

    private var accessory: UsbAccessory? = null
    private var fileDescriptor: ParcelFileDescriptor? = null
    private var inputStream: FileInputStream? = null
    private var outputStream: FileOutputStream? = null
    private val connected = AtomicBoolean(false)
    private var receiveThread: Thread? = null
    private val receiving = AtomicBoolean(false)

    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onPacketReceived: ((Protocol.PacketHeader, ByteArray?) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    fun connectAccessory(usbManager: UsbManager, acc: UsbAccessory): Boolean {
        if (connected.get()) return true

        val fd = usbManager.openAccessory(acc)
        if (fd == null) {
            onError?.invoke("Failed to open USB accessory")
            return false
        }

        accessory = acc
        fileDescriptor = fd
        inputStream = FileInputStream(fd.fileDescriptor)
        outputStream = FileOutputStream(fd.fileDescriptor)
        connected.set(true)

        Log.i(TAG, "USB accessory connected: ${acc.description}")
        onConnected?.invoke()
        return true
    }

    fun disconnect() {
        receiving.set(false)
        receiveThread?.join(1000)
        receiveThread = null

        try { inputStream?.close() } catch (_: Exception) {}
        try { outputStream?.close() } catch (_: Exception) {}
        try { fileDescriptor?.close() } catch (_: Exception) {}

        inputStream = null
        outputStream = null
        fileDescriptor = null
        accessory = null
        connected.set(false)

        Log.i(TAG, "USB disconnected")
        onDisconnected?.invoke()
    }

    fun isConnected(): Boolean = connected.get()

    fun sendPacket(type: Byte, flags: Short = 0, payload: ByteArray? = null): Boolean {
        val out = outputStream ?: return false
        val seq = Protocol.nextSequence()
        val packet = Protocol.buildPacket(type, flags, seq, payload)

        return try {
            synchronized(out) {
                out.write(packet)
                out.flush()
            }
            true
        } catch (e: Exception) {
            onError?.invoke("USB send failed: ${e.message}")
            false
        }
    }

    fun sendHello(deviceName: String, cameraCount: Int, hasMic: Boolean): Boolean {
        val payload = ByteArray(38)
        java.nio.ByteBuffer.wrap(payload).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            putInt(1)
            val nameBytes = ByteArray(32)
            val nameChars = deviceName.toByteArray(Charsets.UTF_8)
            System.arraycopy(nameChars, 0, nameBytes, 0, minOf(nameChars.size, 32))
            put(nameBytes)
            put(cameraCount.toByte())
            put(if (hasMic) 1.toByte() else 0.toByte())
        }

        return sendPacket(Protocol.MessageType.HELLO, payload = payload)
    }

    fun sendPong(): Boolean {
        return sendPacket(Protocol.MessageType.PONG)
    }

    fun startReceiveLoop() {
        if (receiving.get()) return
        receiving.set(true)

        receiveThread = Thread {
            while (receiving.get()) {
                val inp = inputStream ?: break

                try {
                    var headerBuffer = ByteArray(Protocol.HEADER_SIZE)
                    var totalRead = 0
                    while (totalRead < Protocol.HEADER_SIZE) {
                        val read = inp.read(headerBuffer, totalRead, Protocol.HEADER_SIZE - totalRead)
                        if (read < 0) break
                        totalRead += read
                    }
                    if (totalRead < Protocol.HEADER_SIZE) break

                    val header = Protocol.parseHeader(headerBuffer) ?: continue
                    val payloadSize = header.payloadLength and 0x7FFFFFFF
                    if (payloadSize > Protocol.MAX_PAYLOAD_SIZE) {
                        onError?.invoke("Payload too large: $payloadSize")
                        continue
                    }

                    var payload: ByteArray? = null
                    if (payloadSize > 0) {
                        payload = ByteArray(payloadSize)
                        var payloadRead = 0
                        while (payloadRead < payloadSize) {
                            val read = inp.read(payload, payloadRead, payloadSize - payloadRead)
                            if (read < 0) break
                            payloadRead += read
                        }
                    }

                    val fullPacket = if (payload != null) {
                        headerBuffer + payload
                    } else {
                        headerBuffer
                    }
                    if (!Protocol.validatePacket(fullPacket)) {
                        onError?.invoke("Invalid packet received")
                        continue
                    }

                    onPacketReceived?.invoke(header, payload)
                } catch (e: Exception) {
                    if (receiving.get()) {
                        onError?.invoke("USB receive error: ${e.message}")
                    }
                    break
                }
            }
        }.apply {
            isDaemon = true
            name = "USB-Receive"
            start()
        }
    }

    fun stopReceiveLoop() {
        receiving.set(false)
        receiveThread?.join(1000)
        receiveThread = null
    }
}
