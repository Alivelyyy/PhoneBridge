package com.phonebridge

import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicBoolean

class UsbConnection {
    companion object {
        private const val TAG = "UsbConnection"
        private const val USB_TIMEOUT_MS = 5000
        private const val USB_BUFFER_SIZE = 65536
    }

    private var connection: UsbDeviceConnection? = null
    private var usbInterface: UsbInterface? = null
    private var inEndpoint: UsbEndpoint? = null
    private var outEndpoint: UsbEndpoint? = null
    private val connected = AtomicBoolean(false)
    private var receiveThread: Thread? = null
    private val receiving = AtomicBoolean(false)

    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onPacketReceived: ((Protocol.PacketHeader, ByteArray?) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    fun connect(usbManager: UsbManager, device: UsbDevice): Boolean {
        if (connected.get()) return true

        val deviceInterface = findBulkInterface(device) ?: run {
            onError?.invoke("No bulk interface found")
            return false
        }

        val conn = usbManager.openDevice(device) ?: run {
            onError?.invoke("Failed to open USB connection")
            return false
        }

        if (!conn.claimInterface(deviceInterface, true)) {
            onError?.invoke("Failed to claim interface")
            conn.close()
            return false
        }

        val inEp = findEndpoint(deviceInterface, true)
        val outEp = findEndpoint(deviceInterface, false)

        if (inEp == null || outEp == null) {
            onError?.invoke("No bulk endpoints found")
            conn.releaseInterface(deviceInterface)
            conn.close()
            return false
        }

        connection = conn
        usbInterface = deviceInterface
        inEndpoint = inEp
        outEndpoint = outEp
        connected.set(true)

        Log.i(TAG, "USB connected to ${device.productName ?: "device"}")
        onConnected?.invoke()
        return true
    }

    fun disconnect() {
        receiving.set(false)
        receiveThread?.join(1000)
        receiveThread = null

        connection?.let { conn ->
            usbInterface?.let { conn.releaseInterface(it) }
            conn.close()
        }

        connection = null
        usbInterface = null
        inEndpoint = null
        outEndpoint = null
        connected.set(false)

        Log.i(TAG, "USB disconnected")
        onDisconnected?.invoke()
    }

    fun isConnected(): Boolean = connected.get()

    fun sendPacket(type: Byte, flags: Short = 0, payload: ByteArray? = null): Boolean {
        val conn = connection ?: return false
        val outEp = outEndpoint ?: return false
        val seq = Protocol.nextSequence()
        val packet = Protocol.buildPacket(type, flags, seq, payload)

        val transferred = conn.bulkTransfer(outEp, packet, packet.size, USB_TIMEOUT_MS)
        if (transferred < 0) {
            onError?.invoke("USB send failed")
            return false
        }
        return true
    }

    fun sendHello(deviceName: String, cameraCount: Int, hasMic: Boolean): Boolean {
        val payload = ByteBuffer.allocate(38).order(ByteOrder.LITTLE_ENDIAN).apply {
            putInt(1) // protocol version
            val nameBytes = ByteArray(32)
            val nameChars = deviceName.toByteArray(Charsets.UTF_8)
            System.arraycopy(nameChars, 0, nameBytes, 0, minOf(nameChars.size, 32))
            put(nameBytes)
            put(cameraCount.toByte())
            put(if (hasMic) 1.toByte() else 0.toByte())
        }.array()

        return sendPacket(Protocol.MessageType.HELLO, payload = payload)
    }

    fun sendPong(): Boolean {
        return sendPacket(Protocol.MessageType.PONG)
    }

    fun startReceiveLoop() {
        if (receiving.get()) return
        receiving.set(true)

        receiveThread = Thread {
            val buffer = ByteArray(USB_BUFFER_SIZE)
            while (receiving.get()) {
                val conn = connection ?: break
                val inEp = inEndpoint ?: break

                val transferred = conn.bulkTransfer(inEp, buffer, buffer.size, 100)
                if (transferred <= 0) continue

                if (transferred < Protocol.HEADER_SIZE) continue

                val packetData = buffer.copyOfRange(0, transferred)
                if (!Protocol.validatePacket(packetData)) {
                    onError?.invoke("Invalid packet received")
                    continue
                }

                val header = Protocol.parseHeader(packetData) ?: continue
                val payload = header.getPayload(packetData)

                onPacketReceived?.invoke(header, payload)
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

    private fun findBulkInterface(device: UsbDevice): UsbInterface? {
        for (i in 0 until device.interfaceCount) {
            val iface = device.getInterface(i)
            for (j in 0 until iface.endpointCount) {
                val ep = iface.getEndpoint(j)
                if (ep.type == android.hardware.usb.UsbConstants.USB_ENDPOINT_XFER_BULK) {
                    return iface
                }
            }
        }
        return null
    }

    private fun findEndpoint(iface: UsbInterface, incoming: Boolean): UsbEndpoint? {
        for (i in 0 until iface.endpointCount) {
            val ep = iface.getEndpoint(i)
            if (ep.type == android.hardware.usb.UsbConstants.USB_ENDPOINT_XFER_BULK) {
                if (incoming && ep.direction == android.hardware.usb.UsbConstants.USB_DIR_IN) return ep
                if (!incoming && ep.direction == android.hardware.usb.UsbConstants.USB_DIR_OUT) return ep
            }
        }
        return null
    }
}
