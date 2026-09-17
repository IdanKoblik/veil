#pragma once

#include "../carrier.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef __cplusplus
extern "C" {
#endif

struct H264Carrier {
    Carrier carrier;

    AVFormatContext *format_ctx;
    int stream_index;

    const AVCodec *decoder;
    AVCodecContext *decoder_ctx;
    AVFrame *frame;
    AVPacket *packet;
};

struct H264Carrier *h264_carrier_init(const char *target);

#ifdef __cplusplus
}
#endif
