#include "h264.h"

#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <stdlib.h>
#include <string.h>
#include <veil/log.h>

#define H264_ENCODER "libx264"

struct DecodeState {
    AVFormatContext *format_ctx;
    AVCodecContext *decoder_ctx;
    AVFrame *frame;
    AVPacket *packet;
    int stream_index;
};

struct EncodeState {
    AVFormatContext *input_ctx;
    AVFormatContext *output_ctx;
    AVCodecContext *encoder_ctx;
    AVPacket *packet;
    int *stream_map;
    int video_input;
    int video_output;
};

static void h264_release(struct H264Carrier *carrier) {
    for (size_t i = 0; i < carrier->frame_count; i++)
        av_frame_free(&carrier->frames[i]);

    free(carrier->frames);
    avcodec_parameters_free(&carrier->params);
    free(carrier->source);
    free(carrier);
}

static int pixel_format_supported(int format) {
    // Plane 0 has to be 8-bit luma, one byte per sample, and libx264 has to take it back losslessly.
    switch (format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
    case AV_PIX_FMT_GRAY8:
        return 1;
    default:
        return 0;
    }
}

static int frame_keep(struct H264Carrier *carrier, AVFrame *frame) {
    if (!pixel_format_supported(frame->format)) {
        ERROR("Unsupported pixel format %s", av_get_pix_fmt_name(frame->format));
        return -1;
    }

    if (carrier->frame_count > 0) {
        const AVFrame *first = carrier->frames[0];
        if (frame->width != first->width || frame->height != first->height || frame->format != first->format) {
            ERROR("The video changes resolution or pixel format mid-stream");
            return -1;
        }
    }

    if (av_frame_make_writable(frame) < 0) {
        ERROR("Failed to make a decoded frame writable");
        return -1;
    }

    if (frame->pts == AV_NOPTS_VALUE)
        frame->pts = frame->best_effort_timestamp;

    if (carrier->frame_count % 64 == 0) {
        AVFrame **grown = realloc(carrier->frames, (carrier->frame_count + 64) * sizeof(*grown));
        if (!grown) {
            ERROR("Failed to grow the frame list");
            return -1;
        }

        carrier->frames = grown;
    }

    carrier->frames[carrier->frame_count++] = frame;
    return 0;
}

static int frames_drain(struct H264Carrier *carrier, struct DecodeState *state) {
    for (;;) {
        const int result = avcodec_receive_frame(state->decoder_ctx, state->frame);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return 0;

        if (result < 0) {
            ERROR("Failed to decode a video frame");
            return -1;
        }

        if (frame_keep(carrier, state->frame) < 0)
            return -1;

        state->frame = av_frame_alloc();
        if (!state->frame) {
            ERROR("Failed to allocate a video frame");
            return -1;
        }
    }
}

static int decoder_open(struct DecodeState *state, const char *target) {
    if (avformat_open_input(&state->format_ctx, target, NULL, NULL) < 0) {
        ERROR("Failed to open the video (%s)", target);
        return -1;
    }

    if (avformat_find_stream_info(state->format_ctx, NULL) < 0) {
        ERROR("Failed to read the stream info (%s)", target);
        return -1;
    }

    state->stream_index = av_find_best_stream(state->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (state->stream_index < 0) {
        ERROR("The file has no video stream (%s)", target);
        return -1;
    }

    const AVCodecParameters *params = state->format_ctx->streams[state->stream_index]->codecpar;
    if (params->codec_id != AV_CODEC_ID_H264) {
        ERROR("The video stream is not H264 (%s)", target);
        return -1;
    }

    const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!decoder) {
        ERROR("No H264 decoder is available");
        return -1;
    }

    state->decoder_ctx = avcodec_alloc_context3(decoder);
    if (!state->decoder_ctx) {
        ERROR("Failed to allocate the H264 decoder context");
        return -1;
    }

    if (avcodec_parameters_to_context(state->decoder_ctx, params) < 0) {
        ERROR("Failed to copy the stream parameters into the decoder");
        return -1;
    }

    if (avcodec_open2(state->decoder_ctx, decoder, NULL) < 0) {
        ERROR("Failed to open the H264 decoder");
        return -1;
    }

    state->frame = av_frame_alloc();
    state->packet = av_packet_alloc();
    if (!state->frame || !state->packet) {
        ERROR("Failed to allocate the H264 frame buffers");
        return -1;
    }

    return 0;
}

static int frames_load(struct H264Carrier *carrier, struct DecodeState *state) {
    while (av_read_frame(state->format_ctx, state->packet) >= 0) {
        int result = 0;
        if (state->packet->stream_index == state->stream_index)
            result = avcodec_send_packet(state->decoder_ctx, state->packet);

        av_packet_unref(state->packet);
        if (result < 0) {
            ERROR("Failed to feed a packet to the H264 decoder");
            return -1;
        }

        if (frames_drain(carrier, state) < 0)
            return -1;
    }

    if (avcodec_send_packet(state->decoder_ctx, NULL) < 0 || frames_drain(carrier, state) < 0)
        return -1;

    if (carrier->frame_count == 0) {
        ERROR("The video has no frames");
        return -1;
    }

    return 0;
}

static unsigned char *slot_sample(const struct H264Carrier *carrier, const size_t slot) {
    const size_t pixel = slot % carrier->frame_slots;
    const AVFrame *frame = carrier->frames[slot / carrier->frame_slots];

    const size_t row = pixel / (size_t)frame->width;
    const size_t column = pixel % (size_t)frame->width;

    // linesize can exceed the width because of alignment padding.
    return &frame->data[0][row * (size_t)frame->linesize[0] + column];
}

static int c_write(Carrier *carrier, const size_t slot, const unsigned char bit) {
    if (!carrier)
        return -1;

    const struct H264Carrier *video = (struct H264Carrier *)carrier;
    if (slot >= video->slots)
        return -1;

    lsb_matching(slot_sample(video, slot), bit);
    return 0;
}

static unsigned char c_read(Carrier *carrier, size_t slot) {
    if (!carrier)
        return (unsigned char)-1;

    const struct H264Carrier *video = (struct H264Carrier *)carrier;
    if (slot >= video->slots)
        return (unsigned char)-1;

    return *slot_sample(video, slot) & 1;
}

static size_t c_capacity(Carrier *carrier) {
    if (!carrier)
        return 0;

    const struct H264Carrier *video = (struct H264Carrier *)carrier;
    return video->slots;
}

static int encoder_open(const struct H264Carrier *carrier, struct EncodeState *state) {
    const AVCodec *encoder = avcodec_find_encoder_by_name(H264_ENCODER);
    if (!encoder) {
        ERROR("FFmpeg was built without %s", H264_ENCODER);
        return -1;
    }

    state->encoder_ctx = avcodec_alloc_context3(encoder);
    if (!state->encoder_ctx) {
        ERROR("Failed to allocate the H264 encoder context");
        return -1;
    }

    const AVFrame *first = carrier->frames[0];
    const AVCodecParameters *params = carrier->params;
    AVCodecContext *ctx = state->encoder_ctx;

    ctx->width = first->width;
    ctx->height = first->height;
    ctx->pix_fmt = first->format;
    ctx->time_base = carrier->time_base;
    ctx->framerate = carrier->frame_rate;
    ctx->sample_aspect_ratio = params->sample_aspect_ratio;
    ctx->color_range = params->color_range;
    ctx->color_primaries = params->color_primaries;
    ctx->color_trc = params->color_trc;
    ctx->colorspace = params->color_space;
    ctx->chroma_sample_location = params->chroma_location;

    if (state->output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Any quantisation would round the LSBs away, qp 0 makes x264 skip it.
    if (av_opt_set(ctx->priv_data, "qp", "0", 0) < 0) {
        ERROR("Failed to put %s into lossless mode", H264_ENCODER);
        return -1;
    }

    if (avcodec_open2(ctx, encoder, NULL) < 0) {
        ERROR("Failed to open %s", H264_ENCODER);
        return -1;
    }

    return 0;
}

static int streams_map(struct EncodeState *state) {
    const AVFormatContext *input = state->input_ctx;

    state->stream_map = malloc(input->nb_streams * sizeof(*state->stream_map));
    if (!state->stream_map) {
        ERROR("Failed to allocate the stream map");
        return -1;
    }

    for (unsigned int i = 0; i < input->nb_streams; i++) {
        const AVStream *in = input->streams[i];
        state->stream_map[i] = -1;

        const int is_video = (int)i == state->video_input;
        const enum AVMediaType type = in->codecpar->codec_type;
        if (!is_video && type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_SUBTITLE)
            continue;

        AVStream *out = avformat_new_stream(state->output_ctx, NULL);
        if (!out) {
            ERROR("Failed to add an output stream");
            return -1;
        }

        const int result = is_video ? avcodec_parameters_from_context(out->codecpar, state->encoder_ctx) : avcodec_parameters_copy(out->codecpar, in->codecpar);
        if (result < 0) {
            ERROR("Failed to set up output stream %u", i);
            return -1;
        }

        out->codecpar->codec_tag = 0;
        out->time_base = is_video ? state->encoder_ctx->time_base : in->time_base;
        state->stream_map[i] = out->index;

        if (is_video)
            state->video_output = out->index;
    }

    return 0;
}

static int packets_flush(struct EncodeState *state) {
    for (;;) {
        const int result = avcodec_receive_packet(state->encoder_ctx, state->packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return 0;

        if (result < 0) {
            ERROR("Failed to encode a video frame");
            return -1;
        }

        const AVStream *out = state->output_ctx->streams[state->video_output];
        if (state->packet->duration <= 0 && state->encoder_ctx->framerate.num > 0)
            state->packet->duration = av_rescale_q(1, av_inv_q(state->encoder_ctx->framerate), state->encoder_ctx->time_base);

        state->packet->stream_index = state->video_output;
        av_packet_rescale_ts(state->packet, state->encoder_ctx->time_base, out->time_base);

        if (av_interleaved_write_frame(state->output_ctx, state->packet) < 0) {
            ERROR("Failed to write a video packet");
            return -1;
        }
    }
}

static int frame_encode(struct EncodeState *state, AVFrame *frame) {
    if (frame)
        frame->pict_type = AV_PICTURE_TYPE_NONE;

    if (avcodec_send_frame(state->encoder_ctx, frame) < 0) {
        ERROR("Failed to feed a frame to %s", H264_ENCODER);
        return -1;
    }

    return packets_flush(state);
}

static int packets_write(const struct H264Carrier *carrier, struct EncodeState *state) {
    size_t next = 0;

    while (av_read_frame(state->input_ctx, state->packet) >= 0) {
        const int in_index = state->packet->stream_index;
        const int out_index = state->stream_map[in_index];

        if (in_index == state->video_input) {
            av_packet_unref(state->packet);
            if (next < carrier->frame_count && frame_encode(state, carrier->frames[next++]) < 0)
                return -1;

            continue;
        }

        if (out_index < 0) {
            av_packet_unref(state->packet);
            continue;
        }

        const AVStream *in = state->input_ctx->streams[in_index];
        const AVStream *out = state->output_ctx->streams[out_index];
        av_packet_rescale_ts(state->packet, in->time_base, out->time_base);
        state->packet->stream_index = out_index;
        state->packet->pos = -1;

        if (av_interleaved_write_frame(state->output_ctx, state->packet) < 0) {
            ERROR("Failed to copy a packet into the output");
            return -1;
        }
    }

    while (next < carrier->frame_count) {
        if (frame_encode(state, carrier->frames[next++]) < 0)
            return -1;
    }

    return frame_encode(state, NULL);
}

static int c_save(Carrier *carrier, const char *output) {
    if (!carrier || !output)
        return -1;

    const struct H264Carrier *video = (struct H264Carrier *)carrier;
    struct EncodeState state = {.video_input = -1, .video_output = -1};
    int result = -1;

    if (avformat_open_input(&state.input_ctx, video->source, NULL, NULL) < 0 || avformat_find_stream_info(state.input_ctx, NULL) < 0) {
        ERROR("Failed to reopen the source video (%s)", video->source);
        goto done;
    }

    state.video_input = av_find_best_stream(state.input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (state.video_input < 0) {
        ERROR("The source video lost its video stream (%s)", video->source);
        goto done;
    }

    if (avformat_alloc_output_context2(&state.output_ctx, NULL, "mp4", output) < 0) {
        ERROR("Failed to set up the MP4 muxer");
        goto done;
    }

    state.packet = av_packet_alloc();
    if (!state.packet) {
        ERROR("Failed to allocate a packet");
        goto done;
    }

    if (encoder_open(video, &state) < 0 || streams_map(&state) < 0)
        goto done;

    if (!(state.output_ctx->oformat->flags & AVFMT_NOFILE) && avio_open(&state.output_ctx->pb, output, AVIO_FLAG_WRITE) < 0) {
        ERROR("Failed to open the output (%s)", output);
        goto done;
    }

    if (avformat_write_header(state.output_ctx, NULL) < 0) {
        ERROR("Failed to write the MP4 header (%s)", output);
        goto done;
    }

    if (packets_write(video, &state) < 0)
        goto done;

    if (av_write_trailer(state.output_ctx) < 0) {
        ERROR("Failed to finish the MP4 (%s)", output);
        goto done;
    }

    DEBUG("Wrote the H264 carrier into %s", output);
    result = 0;
done:
    if (state.output_ctx && !(state.output_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&state.output_ctx->pb);

    avformat_free_context(state.output_ctx);
    avformat_close_input(&state.input_ctx);
    avcodec_free_context(&state.encoder_ctx);
    av_packet_free(&state.packet);
    free(state.stream_map);
    return result;
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    h264_release((struct H264Carrier *)carrier);
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

    struct DecodeState state = {0};
    int loaded = -1;

    // libx264 prints its encoding stats at info level, which floods normal output.
    av_log_set_level(verbose ? AV_LOG_INFO : AV_LOG_ERROR);

    carrier->source = strdup(target);
    carrier->params = avcodec_parameters_alloc();
    if (!carrier->source || !carrier->params) {
        ERROR("Failed to allocate the H264 carrier");
        goto done;
    }

    if (decoder_open(&state, target) < 0 || frames_load(carrier, &state) < 0)
        goto done;

    AVStream *stream = state.format_ctx->streams[state.stream_index];
    if (avcodec_parameters_copy(carrier->params, stream->codecpar) < 0) {
        ERROR("Failed to keep the stream parameters");
        goto done;
    }

    carrier->time_base = stream->time_base;
    carrier->frame_rate = av_guess_frame_rate(state.format_ctx, stream, NULL);

    const AVFrame *first = carrier->frames[0];
    carrier->frame_slots = (size_t)first->width * (size_t)first->height;
    carrier->slots = carrier->frame_slots * carrier->frame_count;

    DEBUG("H264 carrier: %dx%d, %zu frames, %zu slots", first->width, first->height, carrier->frame_count, carrier->slots);

    carrier->carrier.write = c_write;
    carrier->carrier.read = c_read;
    carrier->carrier.capacity = c_capacity;
    carrier->carrier.save = c_save;
    carrier->carrier.free = c_free;
    loaded = 0;
done:
    av_packet_free(&state.packet);
    av_frame_free(&state.frame);
    avcodec_free_context(&state.decoder_ctx);
    avformat_close_input(&state.format_ctx);

    if (loaded < 0) {
        h264_release(carrier);
        return NULL;
    }

    return carrier;
}
