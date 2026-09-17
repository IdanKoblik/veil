#include "inspect.h"

#include <libswscale/swscale.h>
#include <stdlib.h>
#include <veil/log.h>

static int frame_find(struct H264Reader *reader, AVFrame *frame, const int64_t target) {
    int got = 0;
    while ((got = h264_reader_next(reader, frame, NULL, NULL)) == 1) {
        if (frame->pts >= target)
            return frame->pts == target ? 1 : 0;

        av_frame_unref(frame);
    }

    return got;
}

static int frame_decode(const struct H264Carrier *carrier, size_t index, AVFrame *frame) {
    struct H264Reader reader;
    const int64_t target = carrier->timestamps[index];
    int found = -1;

    if (h264_reader_open(&reader, carrier->source) < 0)
        goto done;

    if (h264_reader_seek(&reader, target) < 0)
        goto done;

    found = frame_find(&reader, frame, target);

    // A backward seek can still land past the frame when timestamps reorder, so fall back to the start.
    if (found == 0) {
        av_frame_unref(frame);
        found = h264_reader_seek(&reader, carrier->timestamps[0]) < 0 ? -1 : frame_find(&reader, frame, target);
    }

    if (found == 0)
        ERROR("Frame %zu is missing from the video (%s)", index, carrier->source);
done:
    h264_reader_close(&reader);
    return found == 1 ? 0 : -1;
}

int video_frame_load(const struct H264Carrier *carrier, size_t index, struct PixelBuffer *out) {
    if (!carrier || !out || index >= carrier->frame_count)
        return -1;

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        ERROR("Failed to allocate a video frame");
        return -1;
    }

    struct SwsContext *scaler = NULL;
    unsigned char *samples = NULL;
    int result = -1;

    if (frame_decode(carrier, index, frame) < 0)
        goto done;

    const int width = frame->width;
    const int height = frame->height;

    scaler = sws_getContext(width, height, frame->format, width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    if (!scaler) {
        ERROR("Failed to set up the RGB conversion for frame %zu", index);
        goto done;
    }

    const size_t len = (size_t)width * (size_t)height * 3;
    samples = malloc(len);
    if (!samples) {
        ERROR("Failed to allocate frame %zu", index);
        goto done;
    }

    uint8_t *planes[4] = {samples, NULL, NULL, NULL};
    int linesizes[4] = {width * 3, 0, 0, 0};
    sws_scale(scaler, (const uint8_t *const *)frame->data, frame->linesize, 0, height, planes, linesizes);

    out->samples = samples;
    out->len = len;
    out->width = width;
    out->height = height;
    out->channels = 3;

    samples = NULL;
    result = 0;
done:
    free(samples);
    sws_freeContext(scaler);
    av_frame_free(&frame);
    return result;
}

// Not pixels_free, that one hands the buffer to stb's allocator.
void video_frame_free(struct PixelBuffer *pixels) {
    if (!pixels)
        return;

    free(pixels->samples);

    pixels->samples = NULL;
    pixels->len = 0;
    pixels->width = 0;
    pixels->height = 0;
    pixels->channels = 0;
}
