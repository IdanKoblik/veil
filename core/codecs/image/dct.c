#include "dct.h"

#include "../carrier.h"
#include "container.h"
#include "veil/handlers/image.h"
#include "veil/handlers/jpeg.h"
#include "veil/log.h"

#include <sodium.h>
#include <stdlib.h>

struct CoefficientCarrier {
    JCOEF *values;
    size_t count;
    enum CodecType codec;
};

static void dct_replacement(JCOEF *coefficient, unsigned char bit) {
    *coefficient = (JCOEF)((*coefficient & ~1) | bit);
}

static void dct_matching(JCOEF *coefficient, unsigned char bit) {
    if ((*coefficient & 1) == bit)
        return;

    JCOEF down = (JCOEF)(*coefficient - 1);
    JCOEF up = (JCOEF)(*coefficient + 1);

    if (!jpeg_coefficient_usable(down))
        *coefficient = up;
    else if (!jpeg_coefficient_usable(up))
        *coefficient = down;
    else
        *coefficient = randombytes_uniform(2) ? up : down;
}

static unsigned char coefficient_read(const Carrier *carrier, size_t slot) {
    const struct CoefficientCarrier *coefficients = carrier->ctx;

    return (unsigned char)(coefficients->values[slot] & 1);
}

static void coefficient_write(const Carrier *carrier, size_t slot, unsigned char bit) {
    struct CoefficientCarrier *coefficients = carrier->ctx;
    JCOEF *value = &coefficients->values[slot];

    switch (coefficients->codec) {
    case CODEC_LSB_MATCHING: {
        dct_matching(value, bit);
        break;
    }
    default:
        dct_replacement(value, bit);
    }
}

static void coefficient_collect(JCOEF *coefficient, size_t slot, void *ctx) {
    ((JCOEF *)ctx)[slot] = *coefficient;
}

static void coefficient_apply(JCOEF *coefficient, size_t slot, void *ctx) {
    *coefficient = ((const JCOEF *)ctx)[slot];
}

static int coefficients_load(struct JpegImage *image, struct CoefficientCarrier *coefficients) {
    coefficients->count = jpeg_walk_coefficients(image, NULL, NULL);
    if (coefficients->count == 0) {
        ERROR("The image carries no usable DCT coefficients");
        return -1;
    }

    coefficients->values = calloc(coefficients->count, sizeof(*coefficients->values));
    if (!coefficients->values) {
        ERROR("Failed to allocate the coefficient buffer");
        return -1;
    }

    return jpeg_walk_coefficients(image, coefficient_collect, coefficients->values) == 0 ? -1 : 0;
}

static int coefficients_store(struct JpegImage *image, const struct CoefficientCarrier *coefficients) {
    return jpeg_walk_coefficients(image, coefficient_apply, coefficients->values) == 0 ? -1 : 0;
}

static int carrier_open(struct ImageCtx *img, struct JpegImage *image, struct CoefficientCarrier *coefficients, Carrier *carrier) {
    coefficients->values = NULL;
    coefficients->count = 0;
    coefficients->codec = img->codec_type;

    INFO("Loading image: %s", img->source_file);
    if (jpeg_image_open(image, img->source_file) < 0)
        return -1;

    img->width = (int)image->decoder.image_width;
    img->height = (int)image->decoder.image_height;
    img->channels = image->decoder.num_components;

    if (coefficients_load(image, coefficients) < 0)
        return -1;

    carrier->ctx = coefficients;
    carrier->slots = coefficients->count;
    carrier->read = coefficient_read;
    carrier->write = coefficient_write;

    INFO("Image: %dx%d, %d channels", img->width, img->height, img->channels);

    return 0;
}

static void carrier_close(struct JpegImage *image, struct CoefficientCarrier *coefficients) {
    free(coefficients->values);
    coefficients->values = NULL;

    jpeg_image_close(image);
}

static int encode(void *ctx, const unsigned char *data, size_t data_len) {
    struct ImageCtx *img = ctx;

    struct JpegImage image;
    struct CoefficientCarrier coefficients;
    Carrier carrier;

    int status = carrier_open(img, &image, &coefficients, &carrier);

    if (status == 0)
        status = container_embed(&carrier, img->codec_type, img->passphrase, data, data_len);

    if (status == 0)
        status = coefficients_store(&image, &coefficients);

    if (status == 0) {
        DEBUG("Writing output image: %s", img->output_file);
        status = jpeg_image_write(&image, img->output_file);
    }

    carrier_close(&image, &coefficients);
    if (status < 0)
        return -1;

    INFO("Encoded successfully -> %s", img->output_file);

    return 0;
}

static int decode(void *ctx, unsigned char **data, size_t *data_len) {
    struct ImageCtx *img = ctx;

    struct JpegImage image;
    struct CoefficientCarrier coefficients;
    Carrier carrier;

    int status = carrier_open(img, &image, &coefficients, &carrier);

    if (status == 0)
        status = container_extract(&carrier, img->passphrase, &img->codec_type, data, data_len);

    carrier_close(&image, &coefficients);
    if (status < 0)
        return -1;

    INFO("Decoded %zu bytes from %s", *data_len, img->source_file);

    return 0;
}

// GOD I HATE JPEG
const Codec DctCodec = {.encode = encode, .decode = decode};
