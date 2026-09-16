#include "dct.h"

#include <stdlib.h>
#include <sodium/randombytes.h>
#include <veil/log.h>

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

static void coefficient_collect(JCOEF *coefficient, size_t slot, void *ctx) {
    ((JCOEF *)ctx)[slot] = *coefficient;
}

static int coefficients_load(struct JpegImage *image, struct DctCarrier *carrier) {
    carrier->jpeg_image = image;
    carrier->slots = jpeg_walk_coefficients(image, NULL, NULL);
    if (carrier->slots == 0) {
        ERROR("The image carries no usable DCT carrier");
        return -1;
    }

    carrier->values = calloc(carrier->slots, sizeof(*carrier->values));
    if (!carrier->values) {
        ERROR("Failed to allocate the coefficient buffer");
        return -1;
    }

    if (jpeg_walk_coefficients(image, coefficient_collect, carrier->values) == 0) {
        ERROR("Failed to collect the DCT coefficients");
        return -1;
    }

    DEBUG("DCT carrier: %zu slots", carrier->slots);
    return 0;
}

static int c_write(Carrier *carrier, const size_t slot, const unsigned char bit) {
    if (!carrier)
        return -1;

    const struct DctCarrier *image = (struct DctCarrier *)carrier;
    if (!image)
        return -1;

    if (slot >= image->slots)
        return -1;

    JCOEF *value = &image->values[slot];
    dct_matching(value, bit);

    return 0;
}

static unsigned char c_read(Carrier *carrier, size_t slot) {
    if (!carrier)
        return '\0';

    const struct DctCarrier *image = (struct DctCarrier *)carrier;
    if (slot >= image->slots)
        return -1;

    return (unsigned char)(image->values[slot] & 1);
}

static int c_capacity(Carrier *carrier) {
    if (!carrier)
        return -1;

    const struct DctCarrier *image = (struct DctCarrier *)carrier;
    return (int)image->slots;
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    const struct DctCarrier *image = (struct DctCarrier *)carrier;
    jpeg_image_close(image->jpeg_image);
    free(carrier);
}

struct DctCarrier *dct_carrier_init(const char *target) {
    if (!target)
        return NULL;

    struct JpegImage image;
    if (jpeg_image_open(&image, target) < 0)
        return NULL;

    struct DctCarrier *carrier = malloc(sizeof(*carrier));
    if (!carrier) {
        ERROR("Failed to allocate the DCT carrier");
        jpeg_image_close(&image);
        return NULL;
    }

    if (coefficients_load(&image, carrier) < 0)
        goto fail;

    carrier->carrier.write = c_write;
    carrier->carrier.read = c_read;
    carrier->carrier.capacity = c_capacity;
    carrier->carrier.capacity = c_capacity;
    return carrier;
fail:
    jpeg_image_close(&image);
    free(carrier);
    return NULL;
}
