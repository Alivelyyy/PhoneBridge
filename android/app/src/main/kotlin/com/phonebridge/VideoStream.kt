package com.phonebridge

import android.annotation.SuppressLint
import android.content.Context
import android.media.Image
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.util.Size
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import java.nio.ByteBuffer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class VideoStream(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner
) {
    companion object {
        private const val TAG = "VideoStream"
    }

    private var cameraProvider: ProcessCameraProvider? = null
    private var encoder: MediaCodec? = null
    private var analysisExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private var encoderThread: HandlerThread? = null
    private var encoderHandler: Handler? = null

    var onFrameEncoded: ((ByteArray, Int, Boolean) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    private var width = 640
    private var height = 480
    private var fps = 15
    private var bitrate = 1_000_000
    private var useFrontCamera = false
    private var running = false
    private var frameCount = 0

    fun configure(width: Int, height: Int, fps: Int, bitrate: Int, frontCamera: Boolean) {
        this.width = width
        this.height = height
        this.fps = fps
        this.bitrate = bitrate
        this.useFrontCamera = frontCamera
    }

    fun start() {
        if (running) return
        running = true
        frameCount = 0
        startEncoder()
        startCamera()
    }

    fun stop() {
        if (!running) return
        running = false
        stopCamera()
        stopEncoder()
    }

    private fun startEncoder() {
        try {
            val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height).apply {
                setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
                setInteger(MediaFormat.KEY_FRAME_RATE, fps)
                setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2)
                setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar)
            }

            encoderThread = HandlerThread("EncoderThread").also { it.start() }
            encoderHandler = Handler(encoderThread!!.looper)

            encoder = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
            encoder!!.setCallback(object : MediaCodec.Callback() {
                override fun onInputBufferAvailable(codec: MediaCodec, index: Int) {}

                override fun onOutputBufferAvailable(codec: MediaCodec, index: Int, info: MediaCodec.BufferInfo) {
                    if (!running) return
                    try {
                        val buffer = codec.getOutputBuffer(index) ?: return
                        val size = info.size.toInt()
                        if (size <= 0) { codec.releaseOutputBuffer(index, false); return }
                        val data = ByteArray(size)
                        buffer.get(data, 0, size)
                        codec.releaseOutputBuffer(index, false)

                        if (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) return
                        val isKeyframe = info.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME != 0
                        onFrameEncoded?.invoke(data, size, isKeyframe)
                    } catch (e: Exception) {
                        Log.e(TAG, "Error reading output buffer", e)
                    }
                }

                override fun onError(codec: MediaCodec, e: MediaCodec.CodecException) {
                    Log.e(TAG, "Encoder error", e)
                    onError?.invoke("Video encoder error: ${e.message}")
                }

                override fun onOutputFormatChanged(codec: MediaCodec, format: MediaFormat) {
                    Log.i(TAG, "Output format changed: $format")
                }
            }, encoderHandler)

            encoder!!.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            encoder!!.start()
            Log.i(TAG, "H.264 encoder started: ${width}x${height} @ ${fps}fps")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start encoder", e)
            onError?.invoke("Failed to start encoder: ${e.message}")
            running = false
        }
    }

    private fun stopEncoder() {
        try {
            encoder?.stop()
            encoder?.release()
            encoder = null
            encoderThread?.quitSafely()
            encoderThread?.join(1000)
            encoderThread = null
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping encoder", e)
        }
    }

    @SuppressLint("MissingPermission")
    private fun startCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            try {
                cameraProvider = cameraProviderFuture.get()
                bindCameraUseCases()
            } catch (e: Exception) {
                Log.e(TAG, "Failed to get camera provider", e)
                onError?.invoke("Camera provider error: ${e.message}")
            }
        }, ContextCompat.getMainExecutor(context))
    }

    private fun stopCamera() {
        try {
            cameraProvider?.unbindAll()
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping camera", e)
        }
    }

    @SuppressLint("MissingPermission")
    private fun bindCameraUseCases() {
        val provider = cameraProvider ?: return

        val cameraSelector = if (useFrontCamera) {
            CameraSelector.DEFAULT_FRONT_CAMERA
        } else {
            CameraSelector.DEFAULT_BACK_CAMERA
        }

        val imageAnalysis = ImageAnalysis.Builder()
            .setTargetResolution(Size(width, height))
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            .build()

        imageAnalysis.setAnalyzer(analysisExecutor) { imageProxy ->
            processImage(imageProxy)
        }

        try {
            provider.unbindAll()
            provider.bindToLifecycle(lifecycleOwner, cameraSelector, imageAnalysis)
            Log.i(TAG, "Camera bound: ${width}x${height}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to bind camera", e)
            onError?.invoke("Failed to bind camera: ${e.message}")
        }
    }

    private fun processImage(image: ImageProxy) {
        if (!running) {
            image.close()
            return
        }

        try {
            val enc = encoder
            if (enc == null) {
                image.close()
                return
            }

            val yuvImage = imageProxyToNV12(image)
            image.close()

            val inputIndex = enc.dequeueInputBuffer(1000)
            if (inputIndex >= 0) {
                val inputBuffer = enc.getInputBuffer(inputIndex) ?: return
                inputBuffer.clear()
                inputBuffer.put(yuvImage)
                val pts = (frameCount.toLong() * 1000000L / fps)
                frameCount++
                enc.queueInputBuffer(inputIndex, 0, yuvImage.size, pts, 0)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error processing image", e)
            image.close()
        }
    }

    private fun imageProxyToNV12(image: ImageProxy): ByteArray {
        val yBuffer = image.planes[0].buffer
        val uBuffer = image.planes[1].buffer
        val vBuffer = image.planes[2].buffer

        val ySize = yBuffer.remaining()
        val uSize = uBuffer.remaining()
        val vSize = vBuffer.remaining()

        val nv12 = ByteArray(ySize + uSize + vSize)

        yBuffer.get(nv12, 0, ySize)
        vBuffer.get(nv12, ySize, vSize)
        uBuffer.get(nv12, ySize + vSize, uSize)

        return nv12
    }

    fun isRunning() = running
    fun getWidth() = width
    fun getHeight() = height
    fun getFps() = fps
}
