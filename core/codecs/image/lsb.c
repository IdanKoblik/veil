#include "lsb.h"

static size_t color_channels(const size_t channels) {
    // Skip alpha channel
    return (channels == 2 || channels == 4) ? channels - 1 : channels;
}

static size_t slot_to_pixel(const struct LsbCarrier *carrier, const size_t slot) {
    if (carrier->colors == carrier->channels)
        return slot;

    return (slot / carrier->colors) * carrier->channels + (slot % carrier->colors);
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

static int c_write(Carrier *carrier, const size_t slot, const unsigned char bit) {
    if (!carrier)
        return -1;

    const struct LsbCarrier *image = (struct LsbCarrier *)carrier;
    if (slot >= image->slots)
        return -1;

    unsigned char *pixel = &image->pixels[slot_to_pixel(image, slot)];
    lsb_matching(pixel, bit);

    return 0;
}

static unsigned char c_read(Carrier *carrier, size_t slot) {
    if (!carrier)
        return '\0';

    const struct LsbCarrier *image = (struct LsbCarrier *)carrier;
    if (slot >= image->slots)
        return -1;

    const unsigned char pixel = image->pixels[slot_to_pixel(image, slot)];
    return pixel & 1;
}

static int c_capacity(Carrier *carrier) {
    if (!carrier)
        return -1;

    const struct LsbCarrier *image = (struct LsbCarrier *)carrier;
    return (int)image->slots;
}

static int c_free(Carrier *carrier) {
    if (!carrier)
        return -1;

    struct LsbCarrier *image = (struct LsbCarrier *)carrier;
    stbi_image_free(image->pixels);
    free(image);
    return 0;
}

struct LsbCarrier *lsb_carrier_init(const char *target) {
    int width, height, channels;
    unsigned char *raw = stbi_load(target, &width, &height, &channels, 0 /* ANY */);
    if (!raw)
        return NULL;

    struct LsbCarrier *carrier = malloc(sizeof(*carrier));
    if (!carrier) {
        stbi_image_free(raw);
        return NULL;
    }

    carrier->pixels = raw;
    carrier->colors = color_channels(channels);
    carrier->channels = (size_t)channels;
    carrier->height = (size_t)height;
    carrier->width = (size_t)width;
    carrier->slots = width * height * carrier->colors * sizeof(*raw);

    carrier->carrier.write = c_write;
    carrier->carrier.read = c_read;
    carrier->carrier.capacity = c_capacity;
    carrier->carrier.free = c_free;
    return carrier;
}