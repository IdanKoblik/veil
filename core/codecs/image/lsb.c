#include "lsb.h"
#include "../carrier.h"
#include "../codec.h"
#include "veil/codecs/image/container.h"
#include "veil/handlers/image.h"
#include "veil/log.h"

#include <sodium/randombytes.h>
#include <stb_image.h>
#include <stb_image_write.h>

struct PixelCarrier {
    unsigned char *pixels;
    size_t colors;
    size_t channels;
    enum CodecType codec;
};

static size_t color_channels(const struct ImageCtx *img) {
    size_t channels = (size_t)img->channels;

    // Skip alpha channel
    return (channels == 2 || channels == 4) ? channels - 1 : channels;
}

static size_t slot_to_pixel(const struct PixelCarrier *carrier, size_t slot) {
    if (carrier->colors == carrier->channels)
        return slot;

    return (slot / carrier->colors) * carrier->channels + (slot % carrier->colors);
}

static void lsb_replacement(unsigned char *pixel, unsigned char bit) {
    *pixel = (*pixel & LSB_FILTER) | bit;
}

static void lsb_matching(unsigned char *pixel, unsigned char bit) {
    unsigned char value = *pixel;
    if ((value & 1) == bit)
        return;

    switch (value) {
    case 0: {
        value = 1;
        break;
    }
    case 255: {
        value = 254;
        break;
    }
    default:
        value += randombytes_uniform(2) ? 1 : -1;
    }

    *pixel = value;
}

static unsigned char pixel_read(const Carrier *carrier, size_t slot) {
    const struct PixelCarrier *pixels = carrier->ctx;

    return pixels->pixels[slot_to_pixel(pixels, slot)] & 1;
}

static void pixel_write(const Carrier *carrier, size_t slot, unsigned char bit) {
    struct PixelCarrier *pixels = carrier->ctx;
    unsigned char *pixel = &pixels->pixels[slot_to_pixel(pixels, slot)];

    switch (pixels->codec) {
    case CODEC_LSB_MATCHING: {
        lsb_matching(pixel, bit);
        break;
    }
    default:
        lsb_replacement(pixel, bit);
    }
}

static unsigned char *load_pixels(struct ImageCtx *img, struct PixelCarrier *pixels, Carrier *carrier) {
    INFO("Loading image: %s", img->source_file);
    unsigned char *raw = stbi_load(img->source_file, &img->width, &img->height, &img->channels, 0 /* ANY */);

    if (!raw) {
        ERROR("Failed to load the image (%s)", img->source_file);
        return NULL;
    }

    pixels->pixels = raw;
    pixels->colors = color_channels(img);
    pixels->channels = (size_t)img->channels;
    pixels->codec = img->codec_type;

    carrier->ctx = pixels;
    carrier->slots = (size_t)img->width * img->height * pixels->colors * sizeof(*raw);
    carrier->read = pixel_read;
    carrier->write = pixel_write;

    INFO("Image: %dx%d, %d channels", img->width, img->height, img->channels);

    return raw;
}

static int encode(void *ctx, const unsigned char *data, size_t data_len) {
    struct ImageCtx *img = ctx;

    struct PixelCarrier pixels;
    Carrier carrier;
    if (!load_pixels(img, &pixels, &carrier))
        return -1;

    int status = container_embed(&carrier, img->codec_type, img->passphrase, data, data_len);
    if (status == 0) {
        DEBUG("Writing output image: %s", img->output_file);
        if (!stbi_write_png(img->output_file, img->width, img->height, img->channels, pixels.pixels, img->width * img->channels)) {
            ERROR("Failed to write into the targeted file");
            status = -1;
        }
    }

    stbi_image_free(pixels.pixels);
    if (status < 0)
        return -1;

    INFO("Encoded successfully -> %s", img->output_file);
    return 0;
}

static int decode(void *ctx, unsigned char **data, size_t *data_len) {
    struct ImageCtx *img = ctx;

    struct PixelCarrier pixels;
    Carrier carrier;
    if (!load_pixels(img, &pixels, &carrier))
        return -1;

    int status = container_extract(&carrier, img->passphrase, &img->codec_type, data, data_len);

    stbi_image_free(pixels.pixels);
    if (status < 0)
        return -1;

    INFO("Decoded %zu bytes from %s", *data_len, img->source_file);
    return 0;
}

const Codec LsbCodec = {.encode = encode, .decode = decode};
