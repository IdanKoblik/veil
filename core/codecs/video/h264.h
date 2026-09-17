#pragma once

#include "../carrier.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef __cplusplus
extern "C" {
#endif

struct H264Carrier {
    Carrier carrier;

    char *source;

    AVFrame **frames;
    size_t frame_count;
    size_t frame_slots;
    size_t slots;

    AVCodecParameters *params;
    AVRational time_base;
    AVRational frame_rate;
};

struct H264Carrier *h264_carrier_init(const char *target);

#ifdef __cplusplus
}
#endif
