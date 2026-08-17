package com.phonebridge

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log

class AudioStream {
    companion object {
        private const val TAG = "AudioStream"
        private const val SAMPLE_RATE = 16000
        private const val CHANNELS = AudioFormat.CHANNEL_IN_MONO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
    }

    private var audioRecord: AudioRecord? = null
    private var captureThread: Thread? = null
    @Volatile private var running = false

    var onAudioData: ((ByteArray, Int) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    fun start() {
        if (running) return

        val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNELS, AUDIO_FORMAT)
        if (bufferSize == AudioRecord.ERROR || bufferSize == AudioRecord.ERROR_BAD_VALUE) {
            onError?.invoke("Invalid buffer size")
            return
        }

        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                SAMPLE_RATE, CHANNELS, AUDIO_FORMAT, bufferSize * 2
            )

            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                onError?.invoke("AudioRecord failed to initialize")
                audioRecord?.release()
                audioRecord = null
                return
            }

            running = true
            audioRecord?.startRecording()

            captureThread = Thread({
                val buffer = ByteArray(1024)
                while (running) {
                    val read = audioRecord?.read(buffer, 0, buffer.size) ?: -1
                    if (read > 0) {
                        val data = buffer.copyOf(read)
                        onAudioData?.invoke(data, read)
                    }
                }
            }, "AudioCapture").apply {
                isDaemon = true
                start()
            }

            Log.i(TAG, "Audio capture started: ${SAMPLE_RATE}Hz mono PCM")
        } catch (e: SecurityException) {
            onError?.invoke("Microphone permission denied")
        } catch (e: Exception) {
            onError?.invoke("Audio init failed: ${e.message}")
        }
    }

    fun stop() {
        running = false
        captureThread?.join(1000)
        captureThread = null
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        Log.i(TAG, "Audio capture stopped")
    }

    fun isRunning() = running
    fun getSampleRate() = SAMPLE_RATE
}
