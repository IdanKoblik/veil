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

static void coefficient_store(JCOEF *coefficient, size_t slot, void *ctx) {
    *coefficient = ((const JCOEF *)ctx)[slot];
}

static int coefficients_load(struct JpegImage *image, struct DctCarrier *carrier) {
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

static int c_save(Carrier *carrier, const char *output) {
    if (!carrier || !output)
        return -1;

    struct DctCarrier *image = (struct DctCarrier *)carrier;

    if (jpeg_walk_coefficients(&image->jpeg_image, coefficient_store, image->values) != image->slots) {
        ERROR("Failed to store the DCT coefficients back into the image");
        return -1;
    }

    if (jpeg_image_write(&image->jpeg_image, output) < 0) {
        ERROR("Failed to write the image (%s)", output);
        return -1;
    }

    DEBUG("Wrote the DCT carrier into %s", output);
    return 0;
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    struct DctCarrier *image = (struct DctCarrier *)carrier;
    jpeg_image_close(&image->jpeg_image);
    free(image->values);
    free(image);
    return 0;
}

struct DctCarrier *dct_carrier_init(const char *target) {
    if (!target)
        return NULL;

    struct DctCarrier *carrier = calloc(1, sizeof(*carrier));
    if (!carrier) {
        ERROR("Failed to allocate the DCT carrier");
        return NULL;
    }

    if (jpeg_image_open(&carrier->jpeg_image, target) < 0) {
        free(carrier);
        return NULL;
    }

    if (coefficients_load(&carrier->jpeg_image, carrier) < 0)
        goto fail;

    carrier->carrier.write = c_write;
    carrier->carrier.read = c_read;
    carrier->carrier.capacity = c_capacity;
    carrier->carrier.save = c_save;
    carrier->carrier.free = c_free;
    return carrier;
fail:
    jpeg_image_close(&carrier->jpeg_image);
    free(carrier->values);
    free(carrier);
    return NULL;
}
