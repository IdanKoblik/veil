#include "stream.h"
#include "image/inspect.h"

#include <veil/handlers/jpeg.h>
#include <veil/log.h>

#include <stdlib.h>
#include <string.h>

static void stream_bit_set(unsigned char *bytes, size_t slot, unsigned char bit) {
    bytes[slot / 8] |= (unsigned char)((bit & 1) << (slot % 8));
}

static unsigned char *stream_alloc(size_t slots, size_t *len) {
    *len = slots / 8;
    if (*len == 0)
        return NULL;

    unsigned char *bytes = calloc(*len, 1);
    if (!bytes)
        ERROR("Failed to allocate the stream buffer");

    return bytes;
}

static int hex_load(const char *target, struct Stream *out) {
    unsigned char *bytes = NULL;
    size_t len = 0;

    if (read_file_raw_data(target, &bytes, &len) != 0) {
        ERROR("Failed to read the file (%s)", target);
        return -1;
    }

    out->bytes = bytes;
    out->len = len;
    out->slots = 0;

    return 0;
}

static int lsb_load(const char *target, struct Stream *out) {
    struct PixelBuffer pixels;
    if (pixels_load(target, &pixels) != 0)
        return -1;

    const size_t channels = (size_t)pixels.channels;
    const size_t colors = pixel_color_channels(pixels.channels);
    const size_t slots = (size_t)pixels.width * (size_t)pixels.height * colors;

    size_t len = 0;
    unsigned char *bytes = stream_alloc(slots, &len);
    if (!bytes) {
        if (len == 0)
            ERROR("The image holds fewer than 8 usable samples (%s)", target);

        pixels_free(&pixels);
        return -1;
    }

    for (size_t slot = 0; slot < len * 8; slot++)
        stream_bit_set(bytes, slot, pixels.samples[pixel_slot_to_sample(slot, colors, channels)] & 1);

    pixels_free(&pixels);

    out->bytes = bytes;
    out->len = len;
    out->slots = slots;

    DEBUG("LSB stream: %zu bytes over %zu slots", len, slots);

    return 0;
}

struct CoefficientBits {
    unsigned char *bytes;
    size_t slots;
};

static void coefficient_bit(JCOEF *coefficient, size_t slot, void *ctx) {
    const struct CoefficientBits *bits = ctx;
    if (slot >= bits->slots)
        return;

    stream_bit_set(bits->bytes, slot, (unsigned char)(*coefficient & 1));
}

static int dct_load(const char *target, struct Stream *out) {
    struct JpegImage image;
    if (jpeg_image_open(&image, target) < 0)
        return -1;

    const size_t slots = jpeg_walk_coefficients(&image, NULL, NULL);
    if (slots == 0) {
        ERROR("The image carries no usable DCT coefficients (%s)", target);
        jpeg_image_close(&image);
        return -1;
    }

    size_t len = 0;
    unsigned char *bytes = stream_alloc(slots, &len);
    if (!bytes) {
        if (len == 0)
            ERROR("The image holds fewer than 8 usable coefficients (%s)", target);

        jpeg_image_close(&image);
        return -1;
    }

    struct CoefficientBits bits = {.bytes = bytes, .slots = len * 8};
    const int walked = jpeg_walk_coefficients(&image, coefficient_bit, &bits) != 0;

    jpeg_image_close(&image);

    if (!walked) {
        free(bytes);
        return -1;
    }

    out->bytes = bytes;
    out->len = len;
    out->slots = slots;

    DEBUG("DCT stream: %zu bytes over %zu slots", len, slots);

    return 0;
}

const char *stream_kind_name(enum StreamKind kind) {
    switch (kind) {
    case STREAM_HEX:
        return "hex";
    case STREAM_LSB:
        return "LSB";
    case STREAM_DCT:
        return "DCT";
    default:
        return "unknown";
    }
}

int stream_kind_available(enum StreamKind kind, enum FileType type) {
    switch (kind) {
    case STREAM_HEX:
        return type != TYPE_NOT_FOUND;
    case STREAM_LSB:
        return is_image_file(type);
    case STREAM_DCT:
        return type == TYPE_JPEG_IMAGE;
    default:
        return 0;
    }
}

int stream_load(const char *target, enum StreamKind kind, struct Stream *out) {
    if (!target || !out)
        return -1;

    memset(out, 0, sizeof(*out));
    out->kind = kind;

    switch (kind) {
    case STREAM_HEX:
        return hex_load(target, out);
    case STREAM_LSB:
        return lsb_load(target, out);
    case STREAM_DCT:
        return dct_load(target, out);
    default:
        ERROR("Unsupported stream kind");
        return -1;
    }
}

void stream_free(struct Stream *stream) {
    if (!stream)
        return;

    free(stream->bytes);

    stream->bytes = NULL;
    stream->len = 0;
    stream->slots = 0;
}

int streams_construct(const char *target, struct StreamSet *set) {
    if (!target || !set)
        return -1;

    static const enum StreamKind kinds[STREAM_KIND_COUNT] = {STREAM_HEX, STREAM_LSB, STREAM_DCT};

    memset(set, 0, sizeof(*set));
    set->file_type = get_file_type(target);

    DEBUG("Constructing the streams of %s (%s)", target, file_type_name(set->file_type));

    for (size_t i = 0; i < STREAM_KIND_COUNT; i++) {
        const enum StreamKind kind = kinds[i];
        if (!stream_kind_available(kind, set->file_type))
            continue;

        struct Stream stream;
        if (stream_load(target, kind, &stream) != 0) {
            /* Every file owes a hex stream; without one there is nothing to show. */
            if (kind == STREAM_HEX) {
                streams_free(set);
                return -1;
            }

            WARN("Leaving out the %s stream of %s", stream_kind_name(kind), target);
            continue;
        }

        set->streams[set->count++] = stream;
    }

    return 0;
}

void streams_free(struct StreamSet *set) {
    if (!set)
        return;

    for (size_t i = 0; i < set->count; i++)
        stream_free(&set->streams[i]);

    set->count = 0;
}

const struct Stream *streams_find(const struct StreamSet *set, enum StreamKind kind) {
    if (!set)
        return NULL;

    for (size_t i = 0; i < set->count; i++)
        if (set->streams[i].kind == kind)
            return &set->streams[i];

    return NULL;
}
