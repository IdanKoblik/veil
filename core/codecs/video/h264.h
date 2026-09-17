#pragma once

#include "../carrier.h"

#ifdef __cplusplus
extern "C" {
#endif

// libav's headers carry no C++ guards of their own.
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef int (*H264PacketVisitor)(AVPacket *packet, void *ctx);

struct H264Reader {
    AVFormatContext *format_ctx;
    AVCodecContext *decoder_ctx;
    AVPacket *packet;
    int stream_index;
    int draining;
};

struct H264Carrier {
    Carrier carrier;

    char *source;

    int width;
    int height;
    int pixel_format;
    AVRational frame_rate;

    size_t frame_count;
    size_t frame_capacity;
    size_t frame_slots;
    size_t slots;

    unsigned char *lsbs;
    unsigned char **edits;
    int64_t *timestamps;
    int loaded;
};

int h264_reader_open(struct H264Reader *reader, const char *path);
int h264_reader_next(struct H264Reader *reader, AVFrame *frame, H264PacketVisitor visit, void *ctx);
int h264_reader_seek(struct H264Reader *reader, int64_t timestamp);
AVRational h264_reader_frame_rate(const struct H264Reader *reader);
void h264_reader_close(struct H264Reader *reader);

struct H264Carrier *h264_carrier_open(const char *target);
struct H264Carrier *h264_carrier_init(const char *target);

#ifdef __cplusplus
}
#endif
