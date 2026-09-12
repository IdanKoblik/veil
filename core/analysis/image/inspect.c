#include "inspect.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <veil/log.h>

int pixels_load(const char *target, struct PixelBuffer *out) {
    if (!target || !out)
        return -1;

    int width = 0, height = 0, channels = 0;
    unsigned char *samples = stbi_load(target, &width, &height, &channels, 0 /* ANY */);

    if (!samples) {
        ERROR("Failed to load the image (%s)", target);
        return -1;
    }

    out->samples = samples;
    out->len = (size_t)width * (size_t)height * (size_t)channels;
    out->width = width;
    out->height = height;
    out->channels = channels;

    DEBUG("Inspecting %dx%d, %d channels", width, height, channels);
    return 0;
}

void pixels_free(struct PixelBuffer *pixels) {
    if (!pixels)
        return;

    stbi_image_free(pixels->samples);

    pixels->samples = NULL;
    pixels->len = 0;
    pixels->width = 0;
    pixels->height = 0;
    pixels->channels = 0;
}

// Skip alpha channel
size_t pixel_color_channels(int channels) {
    return (channels == 2 || channels == 4) ? (size_t)channels - 1 : (size_t)channels;
}

size_t pixel_slot_to_sample(size_t slot, size_t colors, size_t channels) {
    if (colors == channels)
        return slot;

    return (slot / colors) * channels + (slot % colors);
}
