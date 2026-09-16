#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <veil/log.h>

#include "stb_image.h"

#define HEADER_SIZE 12

int is_image_file(enum FileType type) {
    return type == TYPE_JPEG_IMAGE || type == TYPE_PNG_IMAGE;
}

const char *file_type_name(enum FileType type) {
    switch (type) {
    case TYPE_PNG_IMAGE:
        return "PNG";
    case TYPE_JPEG_IMAGE:
        return "JPEG";
    case TYPE_NOT_FOUND:
        return "not found";
    default:
        return "unknown";
    }
}

enum FileType detect_image_type(const char *target) {
    int width;
    int height;
    int channels;

    DEBUG("Probing image: %s", target);
    if (!stbi_info(target, &width, &height, &channels))
        return TYPE_UNKNOWN;

    unsigned char header[HEADER_SIZE];
    FILE *file = fopen(target, "rb");
    if (!file)
        return TYPE_NOT_FOUND;

    size_t len = fread(header, 1, sizeof(header), file);
    fclose(file);

    /*
     * PNG header (https://en.wikipedia.org/wiki/PNG#File_format)
     *
     * 89 50 4E 47 0D 0A 1A 0A
     */
    if (len >= 8 && memcmp(header, "\x89PNG\r\n\x1a\n", 8) == 0) {
        DEBUG("Detected PNG image: %dx%d, %d channels", width, height, channels);
        return TYPE_PNG_IMAGE;
    }

    /*
     * JPEG header (https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format)
     *
     * FF D8 FF
     */
    if (len >= 3 && memcmp(header, "\xff\xd8\xff", 3) == 0) {
        DEBUG("Detected JPEG image: %dx%d, %d channels", width, height, channels);
        return TYPE_JPEG_IMAGE;
    }

    return TYPE_UNKNOWN;
}

enum FileType get_file_type(const char *target) {
    enum FileType type;

    type = detect_image_type(target);
    if (type != TYPE_UNKNOWN)
        return type;

    // TODO more file types

    return TYPE_UNKNOWN;
}

int read_file_raw_data(const char *target, unsigned char **data, size_t *data_len) {
    FILE *file = fopen(target, "rb");
    if (!file)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return -1;
    }

    rewind(file);

    *data = malloc(size > 0 ? (size_t)size : 1);
    if (!*data) {
        fclose(file);
        return -1;
    }

    *data_len = (size_t)size;
    if (fread(*data, 1, *data_len, file) != *data_len) {
        free(*data);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

int write_to_file_raw_data(const char *target, const unsigned char *data, size_t data_len) {
    FILE *file = fopen(target, "wb");
    if (!file)
        return -1;

    if (data_len && fwrite(data, 1, data_len, file) != data_len) {
        fclose(file);
        return -1;
    }

    return fclose(file) == 0 ? 0 : -1;
}
