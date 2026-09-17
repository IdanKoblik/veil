#include "h264.h"

#include <libavutil/cpu.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <stdlib.h>
#include <string.h>
#include <veil/log.h>
#include <veil/progress.h>

#define H264_ENCODER "libx264"
#define H264_ENCODER_THREADS 8
#define H264_ENCODER_PRESET "ultrafast"

struct H264Writer {
    const struct H264Reader *source;

    AVFormatContext *output_ctx;
    AVCodecContext *encoder_ctx;
    AVPacket *packet;

    int *stream_map;
    int video_output;
};

int h264_reader_open(struct H264Reader *reader, const char *path) {
    memset(reader, 0, sizeof(*reader));

    if (avformat_open_input(&reader->format_ctx, path, NULL, NULL) < 0) {
        ERROR("Failed to open the video (%s)", path);
        return -1;
    }

    if (avformat_find_stream_info(reader->format_ctx, NULL) < 0) {
        ERROR("Failed to read the stream info (%s)", path);
        return -1;
    }

    reader->stream_index = av_find_best_stream(reader->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (reader->stream_index < 0) {
        ERROR("The file has no video stream (%s)", path);
        return -1;
    }

    const AVCodecParameters *params = reader->format_ctx->streams[reader->stream_index]->codecpar;
    if (params->codec_id != AV_CODEC_ID_H264) {
        ERROR("The video stream is %s, not H264 (%s)", avcodec_get_name(params->codec_id), path);
        return -1;
    }

    const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!decoder) {
        ERROR("No H264 decoder is available");
        return -1;
    }

    reader->decoder_ctx = avcodec_alloc_context3(decoder);
    if (!reader->decoder_ctx) {
        ERROR("Failed to allocate the H264 decoder context");
        return -1;
    }

    // Carries the SPS/PPS extradata, without it the decoder can't read MP4 packets.
    if (avcodec_parameters_to_context(reader->decoder_ctx, params) < 0) {
        ERROR("Failed to copy the stream parameters into the decoder");
        return -1;
    }

    // libavcodec decodes on one thread unless asked, which makes long videos crawl.
    reader->decoder_ctx->thread_count = 0;
    if (avcodec_open2(reader->decoder_ctx, decoder, NULL) < 0) {
        ERROR("Failed to open the H264 decoder");
        return -1;
    }

    reader->packet = av_packet_alloc();
    if (!reader->packet) {
        ERROR("Failed to allocate a packet");
        return -1;
    }

    return 0;
}

int h264_reader_next(struct H264Reader *reader, AVFrame *frame, H264PacketVisitor visit, void *ctx) {
    for (;;) {
        int result = avcodec_receive_frame(reader->decoder_ctx, frame);
        if (result == 0) {
            if (frame->pts == AV_NOPTS_VALUE)
                frame->pts = frame->best_effort_timestamp;

            return 1;
        }

        if (result == AVERROR_EOF || (result == AVERROR(EAGAIN) && reader->draining))
            return 0;

        if (result != AVERROR(EAGAIN)) {
            ERROR("Failed to decode a video frame");
            return -1;
        }

        if (av_read_frame(reader->format_ctx, reader->packet) < 0) {
            // The decoder holds frames back for reordering until it is flushed.
            reader->draining = 1;
            if (avcodec_send_packet(reader->decoder_ctx, NULL) < 0) {
                ERROR("Failed to flush the H264 decoder");
                return -1;
            }

            continue;
        }

        if (reader->packet->stream_index != reader->stream_index) {
            const int visited = visit ? visit(reader->packet, ctx) : 0;
            av_packet_unref(reader->packet);
            if (visited < 0)
                return -1;

            continue;
        }

        result = avcodec_send_packet(reader->decoder_ctx, reader->packet);
        av_packet_unref(reader->packet);
        if (result < 0) {
            ERROR("Failed to feed a packet to the H264 decoder");
            return -1;
        }
    }
}

int h264_reader_seek(struct H264Reader *reader, const int64_t timestamp) {
    if (av_seek_frame(reader->format_ctx, reader->stream_index, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        ERROR("Failed to seek the video");
        return -1;
    }

    avcodec_flush_buffers(reader->decoder_ctx);
    reader->draining = 0;
    return 0;
}

AVRational h264_reader_frame_rate(const struct H264Reader *reader) {
    AVStream *stream = reader->format_ctx->streams[reader->stream_index];
    return av_guess_frame_rate(reader->format_ctx, stream, NULL);
}

void h264_reader_close(struct H264Reader *reader) {
    av_packet_free(&reader->packet);
    avcodec_free_context(&reader->decoder_ctx);
    avformat_close_input(&reader->format_ctx);
}

static int encoder_open(struct H264Writer *writer, int width, int height, int pixel_format) {
    const AVCodec *encoder = avcodec_find_encoder_by_name(H264_ENCODER);
    if (!encoder) {
        ERROR("FFmpeg was built without %s", H264_ENCODER);
        return -1;
    }

    writer->encoder_ctx = avcodec_alloc_context3(encoder);
    if (!writer->encoder_ctx) {
        ERROR("Failed to allocate the H264 encoder context");
        return -1;
    }

    const AVStream *stream = writer->source->format_ctx->streams[writer->source->stream_index];
    const AVCodecParameters *params = stream->codecpar;
    AVCodecContext *ctx = writer->encoder_ctx;

    ctx->width = width;
    ctx->height = height;
    ctx->pix_fmt = pixel_format;
    ctx->time_base = stream->time_base;
    ctx->framerate = h264_reader_frame_rate(writer->source);
    ctx->sample_aspect_ratio = params->sample_aspect_ratio;
    ctx->color_range = params->color_range;
    ctx->color_primaries = params->color_primaries;
    ctx->color_trc = params->color_trc;
    ctx->colorspace = params->color_space;
    ctx->chroma_sample_location = params->chroma_location;

    if (writer->output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // x264 defaults to 1.5 threads per core and each one holds its own frames; past 8 the memory grows much faster than the speed.
    ctx->thread_count = FFMIN(av_cpu_count(), H264_ENCODER_THREADS);

    // Any quantisation would round the LSBs away, qp 0 makes x264 skip it.
    if (av_opt_set(ctx->priv_data, "qp", "0", 0) < 0) {
        ERROR("Failed to put %s into lossless mode", H264_ENCODER);
        return -1;
    }

    // Lossless output is bit-exact on every preset, the slower ones only shave size, at 8x the time on medium.
    if (av_opt_set(ctx->priv_data, "preset", H264_ENCODER_PRESET, 0) < 0) {
        ERROR("Failed to set the %s preset", H264_ENCODER);
        return -1;
    }

    if (avcodec_open2(ctx, encoder, NULL) < 0) {
        ERROR("Failed to open %s", H264_ENCODER);
        return -1;
    }

    return 0;
}

static int streams_map(struct H264Writer *writer) {
    const AVFormatContext *input = writer->source->format_ctx;

    writer->stream_map = malloc(input->nb_streams * sizeof(*writer->stream_map));
    if (!writer->stream_map) {
        ERROR("Failed to allocate the stream map");
        return -1;
    }

    for (unsigned int i = 0; i < input->nb_streams; i++) {
        const AVStream *in = input->streams[i];
        writer->stream_map[i] = -1;

        const int is_video = (int)i == writer->source->stream_index;
        const enum AVMediaType type = in->codecpar->codec_type;
        if (!is_video && type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_SUBTITLE)
            continue;

        AVStream *out = avformat_new_stream(writer->output_ctx, NULL);
        if (!out) {
            ERROR("Failed to add an output stream");
            return -1;
        }

        const int result = is_video ? avcodec_parameters_from_context(out->codecpar, writer->encoder_ctx) : avcodec_parameters_copy(out->codecpar, in->codecpar);
        if (result < 0) {
            ERROR("Failed to set up output stream %u", i);
            return -1;
        }

        out->codecpar->codec_tag = 0;
        out->time_base = is_video ? writer->encoder_ctx->time_base : in->time_base;
        writer->stream_map[i] = out->index;

        if (is_video)
            writer->video_output = out->index;
    }

    return 0;
}

static int h264_writer_open(struct H264Writer *writer, const struct H264Reader *source, const char *path, int width, int height, int pixel_format) {
    memset(writer, 0, sizeof(*writer));
    writer->source = source;
    writer->video_output = -1;

    if (avformat_alloc_output_context2(&writer->output_ctx, NULL, "mp4", path) < 0) {
        ERROR("Failed to set up the MP4 muxer");
        return -1;
    }

    writer->packet = av_packet_alloc();
    if (!writer->packet) {
        ERROR("Failed to allocate a packet");
        return -1;
    }

    if (encoder_open(writer, width, height, pixel_format) < 0 || streams_map(writer) < 0)
        return -1;

    if (!(writer->output_ctx->oformat->flags & AVFMT_NOFILE) && avio_open(&writer->output_ctx->pb, path, AVIO_FLAG_WRITE) < 0) {
        ERROR("Failed to open the output (%s)", path);
        return -1;
    }

    if (avformat_write_header(writer->output_ctx, NULL) < 0) {
        ERROR("Failed to write the MP4 header (%s)", path);
        return -1;
    }

    return 0;
}

static int h264_writer_copy(AVPacket *packet, void *ctx) {
    struct H264Writer *writer = ctx;

    const int out_index = writer->stream_map[packet->stream_index];
    if (out_index < 0)
        return 0;

    const AVStream *in = writer->source->format_ctx->streams[packet->stream_index];
    const AVStream *out = writer->output_ctx->streams[out_index];
    av_packet_rescale_ts(packet, in->time_base, out->time_base);
    packet->stream_index = out_index;
    packet->pos = -1;

    if (av_interleaved_write_frame(writer->output_ctx, packet) < 0) {
        ERROR("Failed to copy a packet into the output");
        return -1;
    }

    return 0;
}

static int packets_flush(struct H264Writer *writer) {
    for (;;) {
        const int result = avcodec_receive_packet(writer->encoder_ctx, writer->packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return 0;

        if (result < 0) {
            ERROR("Failed to encode a video frame");
            return -1;
        }

        if (writer->packet->duration <= 0 && writer->encoder_ctx->framerate.num > 0)
            writer->packet->duration = av_rescale_q(1, av_inv_q(writer->encoder_ctx->framerate), writer->encoder_ctx->time_base);

        const AVStream *out = writer->output_ctx->streams[writer->video_output];
        writer->packet->stream_index = writer->video_output;
        av_packet_rescale_ts(writer->packet, writer->encoder_ctx->time_base, out->time_base);

        if (av_interleaved_write_frame(writer->output_ctx, writer->packet) < 0) {
            ERROR("Failed to write a video packet");
            return -1;
        }
    }
}

static int h264_writer_encode(struct H264Writer *writer, AVFrame *frame) {
    if (frame)
        frame->pict_type = AV_PICTURE_TYPE_NONE;

    if (avcodec_send_frame(writer->encoder_ctx, frame) < 0) {
        ERROR("Failed to feed a frame to %s", H264_ENCODER);
        return -1;
    }

    return packets_flush(writer);
}

static int h264_writer_finish(struct H264Writer *writer) {
    if (h264_writer_encode(writer, NULL) < 0)
        return -1;

    if (av_write_trailer(writer->output_ctx) < 0) {
        ERROR("Failed to finish the MP4");
        return -1;
    }

    return 0;
}

static void h264_writer_close(struct H264Writer *writer) {
    if (writer->output_ctx && !(writer->output_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&writer->output_ctx->pb);

    avformat_free_context(writer->output_ctx);
    writer->output_ctx = NULL;

    avcodec_free_context(&writer->encoder_ctx);
    av_packet_free(&writer->packet);
    free(writer->stream_map);
    writer->stream_map = NULL;
}

static unsigned char bit_get(const unsigned char *bits, const size_t slot) {
    return (unsigned char)((bits[slot / 8] >> (slot % 8)) & 1);
}
static void bit_set(unsigned char *bits, const size_t slot, const unsigned char bit) {
    const unsigned char mask = (unsigned char)(1u << (slot % 8));
    bits[slot / 8] = (unsigned char)(bit ? bits[slot / 8] | mask : bits[slot / 8] & ~mask);
}

static size_t bit_bytes(const size_t bits) {
    return (bits + 7) / 8;
}

static void edits_free(struct H264Carrier *carrier) {
    if (!carrier->edits)
        return;

    for (size_t i = 0; i < carrier->frame_count; i++)
        free(carrier->edits[i]);

    free(carrier->edits);
    carrier->edits = NULL;
}

static void h264_release(struct H264Carrier *carrier) {
    edits_free(carrier);
    free(carrier->lsbs);
    free(carrier->timestamps);
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

static int frame_matches(const struct H264Carrier *carrier, const AVFrame *frame) {
    return frame->width == carrier->width && frame->height == carrier->height && frame->format == carrier->pixel_format;
}

static int frames_grow(struct H264Carrier *carrier, const size_t expected) {
    // The probe already counted the frames, sizing for it up front avoids reserving double while the map grows.
    size_t capacity = carrier->frame_capacity ? carrier->frame_capacity * 2 : 64;
    if (carrier->frame_count < expected && capacity < expected)
        capacity = expected;

    unsigned char *lsbs = realloc(carrier->lsbs, bit_bytes(capacity * carrier->frame_slots));
    if (!lsbs) {
        ERROR("Failed to grow the LSB map");
        return -1;
    }

    carrier->lsbs = lsbs;

    int64_t *timestamps = realloc(carrier->timestamps, capacity * sizeof(*timestamps));
    if (!timestamps) {
        ERROR("Failed to grow the frame timestamps");
        return -1;
    }

    carrier->timestamps = timestamps;
    carrier->frame_capacity = capacity;
    return 0;
}

static unsigned char lsbs_pack(const unsigned char *samples) {
    return (unsigned char)((samples[0] & 1) | (samples[1] & 1) << 1 | (samples[2] & 1) << 2 | (samples[3] & 1) << 3 | (samples[4] & 1) << 4 | (samples[5] & 1) << 5 | (samples[6] & 1) << 6 | (samples[7] & 1) << 7);
}

static int frame_collect(struct H264Carrier *carrier, const AVFrame *frame, const size_t expected) {
    if (carrier->frame_count == 0) {
        if (!pixel_format_supported(frame->format)) {
            ERROR("Unsupported pixel format %s", av_get_pix_fmt_name(frame->format));
            return -1;
        }

        carrier->width = frame->width;
        carrier->height = frame->height;
        carrier->pixel_format = frame->format;
        carrier->frame_slots = (size_t)frame->width * (size_t)frame->height;
    } else if (!frame_matches(carrier, frame)) {
        ERROR("The video changes resolution or pixel format mid-stream");
        return -1;
    }

    if (carrier->frame_count == carrier->frame_capacity && frames_grow(carrier, expected) < 0)
        return -1;

    const size_t width = (size_t)frame->width;
    size_t slot = carrier->frame_count * carrier->frame_slots;

    // Zeroed per frame rather than on growth, so spare capacity is never touched and never resident.
    // The previous frame can end mid-byte, its low bits in that byte have to survive.
    size_t first_byte = slot / 8;
    if (slot % 8 != 0)
        carrier->lsbs[first_byte++] &= (unsigned char)((1u << (slot % 8)) - 1);

    const size_t end_byte = bit_bytes(slot + carrier->frame_slots);
    if (end_byte > first_byte)
        memset(carrier->lsbs + first_byte, 0, end_byte - first_byte);

    for (size_t row = 0; row < (size_t)frame->height; row++) {
        // linesize can exceed the width because of alignment padding.
        const unsigned char *line = frame->data[0] + row * (size_t)frame->linesize[0];

        for (size_t column = 0; column < width;) {
            // A byte-aligned run is packed whole, which is what keeps loading long videos bearable.
            if (slot % 8 == 0 && column + 8 <= width) {
                carrier->lsbs[slot / 8] = lsbs_pack(line + column);
                slot += 8;
                column += 8;
                continue;
            }

            carrier->lsbs[slot / 8] |= (unsigned char)((line[column] & 1) << (slot % 8));
            slot++;
            column++;
        }
    }

    carrier->timestamps[carrier->frame_count++] = frame->pts;
    return 0;
}

static void edits_overlay(struct H264Carrier *carrier) {
    if (!carrier->edits)
        return;

    const size_t bytes = bit_bytes(carrier->frame_slots);
    for (size_t index = 0; index < carrier->frame_count; index++) {
        const unsigned char *edit = carrier->edits[index];
        if (!edit)
            continue;

        for (size_t pixel = 0; pixel < carrier->frame_slots; pixel++) {
            if (bit_get(edit + bytes, pixel))
                bit_set(carrier->lsbs, index * carrier->frame_slots + pixel, bit_get(edit, pixel));
        }
    }
}

// Strict when slots may already have been handed out against the probed layout, which the frames then have to match.
static int carrier_load(struct H264Carrier *carrier, const int strict) {
    struct H264Reader reader = {0};
    AVFrame *frame = NULL;
    int result = -1;

    const size_t probed = carrier->frame_count;
    const int width = carrier->width;
    const int height = carrier->height;
    const int pixel_format = carrier->pixel_format;

    carrier->loaded = 1;
    carrier->frame_count = 0;
    carrier->frame_capacity = 0;

    if (h264_reader_open(&reader, carrier->source) < 0)
        goto done;

    frame = av_frame_alloc();
    if (!frame) {
        ERROR("Failed to allocate a video frame");
        goto done;
    }

    for (;;) {
        const int got = h264_reader_next(&reader, frame, NULL, NULL);
        if (got < 0)
            goto done;

        if (got == 0)
            break;

        const int collected = frame_collect(carrier, frame, probed);
        av_frame_unref(frame);
        if (collected < 0)
            goto done;

        progress_update("Reading frames", carrier->frame_count, probed);
    }

    if (carrier->frame_count == 0) {
        ERROR("The video has no frames");
        goto done;
    }

    if (strict && (carrier->frame_count != probed || carrier->width != width || carrier->height != height || carrier->pixel_format != pixel_format)) {
        ERROR("The decoded frames don't match the video's index (%s)", carrier->source);
        goto done;
    }

    carrier->slots = carrier->frame_slots * carrier->frame_count;

    unsigned char *fitted = realloc(carrier->lsbs, bit_bytes(carrier->slots));
    if (fitted)
        carrier->lsbs = fitted;

    edits_overlay(carrier);

    DEBUG("H264 carrier loaded: %dx%d, %zu frames, %zu slots", carrier->width, carrier->height, carrier->frame_count, carrier->slots);
    result = 0;
done:
    progress_finish();
    av_frame_free(&frame);
    h264_reader_close(&reader);

    if (result < 0) {
        // Reads and writes see a map that was handed off and refuse, rather than trusting half a load.
        free(carrier->lsbs);
        carrier->lsbs = NULL;
        carrier->frame_count = probed;
        carrier->width = width;
        carrier->height = height;
        carrier->pixel_format = pixel_format;
        carrier->frame_slots = (size_t)width * (size_t)height;
    }

    return result;
}

static int c_write(Carrier *carrier, const size_t slot, const unsigned char bit) {
    if (!carrier)
        return -1;

    struct H264Carrier *video = (struct H264Carrier *)carrier;
    if (slot >= video->slots || (video->loaded && !video->lsbs))
        return -1;

    const size_t index = slot / video->frame_slots;
    const size_t pixel = slot % video->frame_slots;
    const size_t bytes = bit_bytes(video->frame_slots);
    unsigned char *edit = video->edits ? video->edits[index] : NULL;

    if (video->lsbs && bit_get(video->lsbs, slot) == bit && !(edit && bit_get(edit + bytes, pixel)))
        return 0;

    if (!video->edits) {
        video->edits = calloc(video->frame_count, sizeof(*video->edits));
        if (!video->edits) {
            ERROR("Failed to allocate the change map");
            return -1;
        }
    }

    // Kept per frame, so saving can skip every frame the payload never reached.
    if (!edit) {
        edit = calloc(2 * bytes, 1);
        if (!edit) {
            ERROR("Failed to allocate the change map");
            return -1;
        }

        video->edits[index] = edit;
    }

    bit_set(edit, pixel, bit);
    bit_set(edit + bytes, pixel, 1);

    if (video->lsbs)
        bit_set(video->lsbs, slot, bit);

    return 0;
}

static unsigned char c_read(Carrier *carrier, size_t slot) {
    if (!carrier)
        return (unsigned char)-1;

    struct H264Carrier *video = (struct H264Carrier *)carrier;
    if (slot >= video->slots)
        return (unsigned char)-1;

    // Encoding never reads, so only decoding pays for a pass over every frame.
    if (!video->loaded && carrier_load(video, 1) < 0)
        return (unsigned char)-1;

    if (!video->lsbs)
        return (unsigned char)-1;

    return bit_get(video->lsbs, slot);
}

static size_t c_capacity(Carrier *carrier) {
    if (!carrier)
        return 0;

    const struct H264Carrier *video = (struct H264Carrier *)carrier;
    return video->slots;
}

static int frame_apply(const struct H264Carrier *carrier, AVFrame *frame, const size_t index) {
    if (index >= carrier->frame_count || !frame_matches(carrier, frame)) {
        ERROR("The source video changed since it was opened (%s)", carrier->source);
        return -1;
    }

    const unsigned char *edit = carrier->edits ? carrier->edits[index] : NULL;
    if (!edit)
        return 0;

    if (av_frame_make_writable(frame) < 0) {
        ERROR("Failed to make a decoded frame writable");
        return -1;
    }

    const size_t width = (size_t)carrier->width;
    const size_t bytes = bit_bytes(carrier->frame_slots);
    const unsigned char *mask = edit + bytes;

    for (size_t byte = 0; byte < bytes; byte++) {
        if (mask[byte] == 0)
            continue;

        for (size_t bit = 0; bit < 8; bit++) {
            if (!((mask[byte] >> bit) & 1))
                continue;

            const size_t pixel = byte * 8 + bit;
            unsigned char *sample = &frame->data[0][(pixel / width) * (size_t)frame->linesize[0] + pixel % width];
            lsb_matching(sample, (unsigned char)((edit[byte] >> bit) & 1));
        }
    }

    return 0;
}

static int c_save(Carrier *carrier, const char *output) {
    if (!carrier || !output)
        return -1;

    const struct H264Carrier *video = (struct H264Carrier *)carrier;
    struct H264Reader reader = {0};
    struct H264Writer writer = {0};
    AVFrame *frame = NULL;
    int result = -1;

    if (h264_reader_open(&reader, video->source) < 0)
        goto done;

    if (h264_writer_open(&writer, &reader, output, video->width, video->height, video->pixel_format) < 0)
        goto done;

    frame = av_frame_alloc();
    if (!frame) {
        ERROR("Failed to allocate a video frame");
        goto done;
    }

    size_t index = 0;
    for (;;) {
        // Audio and subtitle packets are copied as the reader passes them, which keeps the output interleaved.
        const int got = h264_reader_next(&reader, frame, h264_writer_copy, &writer);
        if (got < 0)
            goto done;

        if (got == 0)
            break;

        const int encoded = frame_apply(video, frame, index++) == 0 && h264_writer_encode(&writer, frame) == 0;
        av_frame_unref(frame);
        if (!encoded)
            goto done;

        progress_update("Encoding frames", index, video->frame_count);
    }

    if (index != video->frame_count) {
        ERROR("The source video changed since it was opened (%s)", video->source);
        goto done;
    }

    if (h264_writer_finish(&writer) < 0)
        goto done;

    DEBUG("Wrote the H264 carrier into %s", output);
    result = 0;
done:
    progress_finish();
    av_frame_free(&frame);
    h264_writer_close(&writer);
    h264_reader_close(&reader);
    return result;
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    h264_release((struct H264Carrier *)carrier);
    return 0;
}

static size_t frames_count(struct H264Reader *reader) {
    // Counting packets reads the index and skips decoding, a fraction of a second even on long videos.
    // Discarded packets are the ones an edit list cuts, the decoder never outputs a frame for them.
    AVPacket *packet = reader->packet;
    size_t frames = 0;

    while (av_read_frame(reader->format_ctx, packet) >= 0) {
        frames += packet->stream_index == reader->stream_index && !(packet->flags & AV_PKT_FLAG_DISCARD);
        av_packet_unref(packet);
    }

    return frames;
}

struct H264Carrier *h264_carrier_open(const char *target) {
    if (!target)
        return NULL;

    // libx264 prints its encoding stats at info level, which floods normal output.
    av_log_set_level(verbose ? AV_LOG_INFO : AV_LOG_ERROR);

    struct H264Carrier *carrier = calloc(1, sizeof(*carrier));
    if (!carrier) {
        ERROR("Failed to allocate the H264 carrier");
        return NULL;
    }

    struct H264Reader reader = {0};
    int opened = -1;

    carrier->source = strdup(target);
    if (!carrier->source) {
        ERROR("Failed to allocate the H264 carrier");
        goto done;
    }

    if (h264_reader_open(&reader, target) < 0)
        goto done;

    const AVCodecParameters *params = reader.format_ctx->streams[reader.stream_index]->codecpar;
    if (!pixel_format_supported(params->format)) {
        ERROR("Unsupported pixel format %s", av_get_pix_fmt_name(params->format));
        goto done;
    }

    carrier->width = params->width;
    carrier->height = params->height;
    carrier->pixel_format = params->format;
    carrier->frame_slots = (size_t)params->width * (size_t)params->height;
    carrier->frame_rate = h264_reader_frame_rate(&reader);

    carrier->frame_count = frames_count(&reader);
    if (carrier->frame_count == 0 || carrier->frame_slots == 0) {
        ERROR("The video has no frames");
        goto done;
    }

    carrier->slots = carrier->frame_slots * carrier->frame_count;

    DEBUG("H264 carrier: %dx%d, %zu frames, %zu slots", carrier->width, carrier->height, carrier->frame_count, carrier->slots);

    carrier->carrier.write = c_write;
    carrier->carrier.read = c_read;
    carrier->carrier.capacity = c_capacity;
    carrier->carrier.save = c_save;
    carrier->carrier.free = c_free;
    opened = 0;
done:
    h264_reader_close(&reader);

    if (opened < 0) {
        h264_release(carrier);
        return NULL;
    }

    return carrier;
}

struct H264Carrier *h264_carrier_init(const char *target) {
    struct H264Carrier *carrier = h264_carrier_open(target);
    if (!carrier)
        return NULL;

    if (carrier_load(carrier, 0) < 0) {
        h264_release(carrier);
        return NULL;
    }

    return carrier;
}
