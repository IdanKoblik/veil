#include "lossless.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <veil/log.h>

static size_t color_channels(const size_t channels) {
    // Skip alpha channel
    return (channels == 2 || channels == 4) ? channels - 1 : channels;
}

static size_t slot_to_pixel(const struct LosslessCarrier *carrier, const size_t slot) {
    if (carrier->colors == carrier->channels)
        return slot;

    return (slot / carrier->colors) * carrier->channels + (slot % carrier->colors);
}

static int c_write(Carrier *carrier, const size_t slot, const unsigned char bit) {
    if (!carrier)
        return -1;

    const struct LosslessCarrier *image = (struct LosslessCarrier *)carrier;
    if (slot >= image->slots)
        return -1;

    unsigned char *pixel = &image->pixels[slot_to_pixel(image, slot)];
    lsb_matching(pixel, bit);

    return 0;
}

static unsigned char c_read(Carrier *carrier, size_t slot) {
    if (!carrier)
        return (unsigned char)-1;

    const struct LosslessCarrier *image = (struct LosslessCarrier *)carrier;
    if (slot >= image->slots)
        return (unsigned char)-1;

    const unsigned char pixel = image->pixels[slot_to_pixel(image, slot)];
    return pixel & 1;
}

static size_t c_capacity(Carrier *carrier) {
    if (!carrier)
        return 0;

    const struct LosslessCarrier *image = (struct LosslessCarrier *)carrier;
    return image->slots;
}

static int c_save(Carrier *carrier, const char *output) {
    if (!carrier || !output)
        return -1;

    const struct LosslessCarrier *image = (struct LosslessCarrier *)carrier;

    const int stride = (int)(image->width * image->channels);
    if (!stbi_write_png(output, (int)image->width, (int)image->height, (int)image->channels, image->pixels, stride)) {
        ERROR("Failed to write the image (%s)", output);
        return -1;
    }

    DEBUG("Wrote the LSB carrier into %s", output);
    return 0;
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    struct LosslessCarrier *image = (struct LosslessCarrier *)carrier;
    stbi_image_free(image->pixels);
    free(image);
    return 0;
}

struct LosslessCarrier *lossless_carrier_init(const char *target) {
    if (!target)
        return NULL;

    int width, height, channels;
    unsigned char *raw = stbi_load(target, &width, &height, &channels, 0 /* ANY */);
    if (!raw) {
        ERROR("Failed to load the image (%s): %s", target, stbi_failure_reason());
        return NULL;
    }

    struct LosslessCarrier *carrier = malloc(sizeof(*carrier));
    if (!carrier) {
        ERROR("Failed to allocate the LSB carrier");
        stbi_image_free(raw);
        return NULL;
    }

    carrier->pixels = raw;
    carrier->colors = color_channels(channels);
    carrier->channels = (size_t)channels;
    carrier->height = (size_t)height;
    carrier->width = (size_t)width;
    // Widened before multiplying, int width * height alone overflows on large images.
    carrier->slots = (size_t)width * (size_t)height * carrier->colors;

    DEBUG("LSB carrier: %dx%d, %zu color channels, %zu slots", width, height, carrier->colors, carrier->slots);

    carrier->carrier.write = c_write;
    carrier->carrier.read = c_read;
    carrier->carrier.capacity = c_capacity;
    carrier->carrier.save = c_save;
    carrier->carrier.free = c_free;
    return carrier;
}
