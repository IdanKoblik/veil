#include "h264.h"

#include <stdlib.h>
#include <veil/log.h>

static void h264_release(struct H264Carrier *carrier) {
    // Every libav free takes NULL, so this is safe on a half-built carrier.
    av_packet_free(&carrier->packet);
    av_frame_free(&carrier->frame);
    avcodec_free_context(&carrier->decoder_ctx);
    avformat_close_input(&carrier->format_ctx);
    free(carrier);
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    h264_release((struct H264Carrier *)carrier);
    return 0;
}

static int decoder_open(struct H264Carrier *carrier, const char *target) {
    if (avformat_open_input(&carrier->format_ctx, target, NULL, NULL) < 0) {
        ERROR("Failed to open the video (%s)", target);
        return -1;
    }

    if (avformat_find_stream_info(carrier->format_ctx, NULL) < 0) {
        ERROR("Failed to read the stream info (%s)", target);
        return -1;
    }

    carrier->stream_index = av_find_best_stream(carrier->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (carrier->stream_index < 0) {
        ERROR("The file has no video stream (%s)", target);
        return -1;
    }

    const AVCodecParameters *params = carrier->format_ctx->streams[carrier->stream_index]->codecpar;
    if (params->codec_id != AV_CODEC_ID_H264) {
        ERROR("The video stream is not H264 (%s)", target);
        return -1;
    }

    carrier->decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!carrier->decoder) {
        ERROR("No H264 decoder is available");
        return -1;
    }

    carrier->decoder_ctx = avcodec_alloc_context3(carrier->decoder);
    if (!carrier->decoder_ctx) {
        ERROR("Failed to allocate the H264 decoder context");
        return -1;
    }

    if (avcodec_parameters_to_context(carrier->decoder_ctx, params) < 0) {
        ERROR("Failed to copy the stream parameters into the decoder");
        return -1;
    }

    if (avcodec_open2(carrier->decoder_ctx, carrier->decoder, NULL) < 0) {
        ERROR("Failed to open the H264 decoder");
        return -1;
    }

    return 0;
}

struct H264Carrier *h264_carrier_init(const char *target) {
    if (!target)
        return NULL;

    struct H264Carrier *carrier = calloc(1, sizeof(*carrier));
    if (!carrier) {
        ERROR("Failed to allocate the H264 carrier");
        return NULL;
    }

    if (decoder_open(carrier, target) < 0)
        goto fail;

    carrier->frame = av_frame_alloc();
    carrier->packet = av_packet_alloc();
    if (!carrier->frame || !carrier->packet) {
        ERROR("Failed to allocate the H264 frame buffers");
        goto fail;
    }

    DEBUG("H264 carrier: %dx%d", carrier->decoder_ctx->width, carrier->decoder_ctx->height);

    // TODO rest of the carrier
    carrier->carrier.free = c_free;
    return carrier;
fail:
    h264_release(carrier);
    return NULL;
}
