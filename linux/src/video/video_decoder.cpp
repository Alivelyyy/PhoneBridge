#include "phonebridge/video_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include <cstring>

namespace phonebridge {

struct VideoDecoder::Impl {
    const AVCodec* codec = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbFrame = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* swsCtx = nullptr;
    uint8_t* rgbBuffer = nullptr;
    int width = 0;
    int height = 0;
    bool frameReady = false;
};

VideoDecoder::VideoDecoder() : impl_(new Impl()) {}

VideoDecoder::~VideoDecoder() {
    shutdown();
    delete impl_;
}

bool VideoDecoder::initialize(const VideoConfig& config) {
    impl_->width = config.width;
    impl_->height = config.height;

    impl_->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!impl_->codec) {
        if (onError) onError("H.264 decoder not found");
        return false;
    }

    impl_->codecCtx = avcodec_alloc_context3(impl_->codec);
    if (!impl_->codecCtx) {
        if (onError) onError("Failed to allocate codec context");
        return false;
    }

    impl_->codecCtx->width = config.width;
    impl_->codecCtx->height = config.height;
    impl_->codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(impl_->codecCtx, impl_->codec, nullptr) < 0) {
        if (onError) onError("Failed to open H.264 decoder");
        return false;
    }

    impl_->frame = av_frame_alloc();
    impl_->rgbFrame = av_frame_alloc();
    impl_->packet = av_packet_alloc();

    if (!impl_->frame || !impl_->rgbFrame || !impl_->packet) {
        if (onError) onError("Failed to allocate frames");
        return false;
    }

    int rgbSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, config.width, config.height, 1);
    impl_->rgbBuffer = static_cast<uint8_t*>(av_malloc(rgbSize));
    av_image_fill_arrays(impl_->rgbFrame->data, impl_->rgbFrame->linesize,
                         impl_->rgbBuffer, AV_PIX_FMT_RGB24,
                         config.width, config.height, 1);

    impl_->swsCtx = sws_getContext(
        config.width, config.height, AV_PIX_FMT_YUV420P,
        config.width, config.height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!impl_->swsCtx) {
        if (onError) onError("Failed to create scaler context");
        return false;
    }

    return true;
}

void VideoDecoder::shutdown() {
    if (impl_->swsCtx) {
        sws_freeContext(impl_->swsCtx);
        impl_->swsCtx = nullptr;
    }
    if (impl_->rgbBuffer) {
        av_free(impl_->rgbBuffer);
        impl_->rgbBuffer = nullptr;
    }
    if (impl_->packet) av_packet_free(&impl_->packet);
    if (impl_->frame) av_frame_free(&impl_->frame);
    if (impl_->rgbFrame) av_frame_free(&impl_->rgbFrame);
    if (impl_->codecCtx) avcodec_free_context(&impl_->codecCtx);
}

bool VideoDecoder::decodeFrame(const uint8_t* data, size_t length, bool isKeyframe) {
    if (!impl_->codecCtx) return false;

    impl_->packet->data = const_cast<uint8_t*>(data);
    impl_->packet->size = static_cast<int>(length);

    int ret = avcodec_send_packet(impl_->codecCtx, impl_->packet);
    if (ret < 0) {
        if (onError) onError("Error sending packet to decoder");
        return false;
    }

    ret = avcodec_receive_frame(impl_->codecCtx, impl_->frame);
    if (ret == 0) {
        sws_scale(impl_->swsCtx,
                  impl_->frame->data, impl_->frame->linesize, 0, impl_->height,
                  impl_->rgbFrame->data, impl_->rgbFrame->linesize);
        impl_->frameReady = true;
        return true;
    }

    return false;
}

bool VideoDecoder::getDecodedFrame(uint8_t* output, size_t& outputSize) {
    if (!impl_->frameReady) return false;

    int frameSize = impl_->width * impl_->height * 3;
    std::memcpy(output, impl_->rgbFrame->data[0], frameSize);
    outputSize = frameSize;
    impl_->frameReady = false;
    return true;
}

int VideoDecoder::getWidth() const { return impl_->width; }
int VideoDecoder::getHeight() const { return impl_->height; }

} // namespace phonebridge
