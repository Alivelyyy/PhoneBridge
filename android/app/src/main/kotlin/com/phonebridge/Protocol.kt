package com.phonebridge

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicInteger

object Protocol {
    const val MAGIC = 0x50424430u // "PBD0"
    const val VERSION: Byte = 1
    const val HEADER_SIZE = 28

    const val FLAG_FRAGMENT: Short = 0x0001
    const val FLAG_MORE: Short = 0x0002
    const val FLAG_KEYFRAME: Short = 0x0004

    object MessageType {
        const val HELLO: Byte = 0x01
        const val HELLO_ACK: Byte = 0x02
        const val DEVICE_INFO: Byte = 0x03
        const val DEVICE_INFO_ACK: Byte = 0x04
        const val VIDEO_CONFIG: Byte = 0x10
        const val VIDEO_CONFIG_ACK: Byte = 0x11
        const val VIDEO_FRAME: Byte = 0x12
        const val AUDIO_CONFIG: Byte = 0x13
        const val AUDIO_CONFIG_ACK: Byte = 0x14
        const val AUDIO_FRAME: Byte = 0x15
        const val START_STREAM: Byte = 0x16
        const val STOP_STREAM: Byte = 0x17
        const val PING: Byte = 0x20
        const val PONG: Byte = 0x21
        const val ERROR: Byte = 0x30
    }

    private val sequenceCounter = AtomicInteger(0)

    fun nextSequence(): Int = sequenceCounter.getAndIncrement()

    fun calculateChecksum(data: ByteArray, offset: Int = 0, length: Int = data.size): UInt {
        var crc = 0xFFFFFFFFu
        for (i in offset until offset + length) {
            crc = crc xor (data[i].toUInt() and 0xFFu)
            for (j in 0 until 8) {
                crc = (crc shr 1) xor (0xEDB88320u and (0u - (crc and 1u)))
            }
        }
        return crc.inv()
    }

    fun buildPacket(
        type: Byte,
        flags: Short = 0,
        sequence: Int,
        payload: ByteArray? = null
    ): ByteArray {
        val payloadLen = payload?.size ?: 0
        val totalSize = HEADER_SIZE + payloadLen
        val buffer = ByteBuffer.allocate(totalSize).order(ByteOrder.LITTLE_ENDIAN)

        buffer.putInt(MAGIC.toInt())
        buffer.put(VERSION)
        buffer.put(type)
        buffer.putShort(flags)
        buffer.putInt(payloadLen)
        buffer.putInt(sequence)
        buffer.putLong(System.currentTimeMillis() * 1000) // microseconds since epoch
        buffer.putInt(if (payload != null && payloadLen > 0) calculateChecksum(payload).toInt() else 0)

        if (payload != null && payloadLen > 0) {
            buffer.put(payload)
        }

        return buffer.array()
    }

    fun parseHeader(data: ByteArray): PacketHeader? {
        if (data.size < HEADER_SIZE) return null

        val buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        val magic = buffer.int.toUInt()
        if (magic != MAGIC) return null

        val version = buffer.get()
        val type = buffer.get()
        val flags = buffer.short
        val payloadLength = buffer.int
        val sequence = buffer.int
        val timestamp = buffer.long
        val checksum = buffer.int

        return PacketHeader(magic, version, type, flags, payloadLength, sequence, timestamp, checksum)
    }

    fun validatePacket(data: ByteArray): Boolean {
        if (data.size < HEADER_SIZE) return false
        val header = parseHeader(data) ?: return false
        if (data.size != HEADER_SIZE + header.payloadLength) return false

        if (header.payloadLength > 0) {
            val payload = data.copyOfRange(HEADER_SIZE, data.size)
            val expectedChecksum = calculateChecksum(payload).toInt()
            if (header.checksum != expectedChecksum) return false
        }
        return true
    }

    data class PacketHeader(
        val magic: UInt,
        val version: Byte,
        val type: Byte,
        val flags: Short,
        val payloadLength: Int,
        val sequence: Int,
        val timestamp: Long,
        val checksum: Int
    ) {
        fun getPayload(data: ByteArray): ByteArray? {
            if (payloadLength <= 0) return null
            return data.copyOfRange(HEADER_SIZE, HEADER_SIZE + payloadLength)
        }
    }
}
