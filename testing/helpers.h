#pragma once

#include <stb_image.h>
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline char *create_test_png(int width, int height, int channels) {
    char path[] = "/tmp/test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;
    close(fd);

    unsigned char *pixels = calloc((size_t)(width * height * channels), 1);
    if (!pixels) {
        unlink(path);
        return NULL;
    }

    for (int i = 0; i < width * height * channels; i++)
        pixels[i] = (unsigned char)(i % 256);

    int stride = width * channels;
    if (stbi_write_png(path, width, height, channels, pixels, stride) == 0) {
        free(pixels);
        unlink(path);
        return NULL;
    }

    free(pixels);
    return strdup(path);
}

static inline char *create_test_jpg(int width, int height, int quality) {
    char path[] = "/tmp/test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;
    close(fd);

    unsigned char *pixels = calloc((size_t)(width * height * 3), 1);
    if (!pixels) {
        unlink(path);
        return NULL;
    }

    for (int i = 0; i < width * height * 3; i++)
        pixels[i] = (unsigned char)(i % 256);

    if (stbi_write_jpg(path, width, height, 3, pixels, quality) == 0) {
        free(pixels);
        unlink(path);
        return NULL;
    }

    free(pixels);
    return strdup(path);
}

static inline char *create_test_mp4(int width, int height, int frames) {
    char path[] = "/tmp/test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;
    close(fd);

    char command[512];
    snprintf(command, sizeof(command),
             "ffmpeg -loglevel error -y -f lavfi -i testsrc=size=%dx%d:rate=10 -f lavfi -i sine "
             "-frames:v %d -shortest -c:v libx264 -pix_fmt yuv420p -c:a aac -f mp4 %s",
             width, height, frames, path);

    if (system(command) != 0) {
        unlink(path);
        return NULL;
    }

    return strdup(path);
}

static inline char *create_temp_path(void) {
    char path[] = "/tmp/test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;

    close(fd);
    return strdup(path);
}

static inline char *create_temp_file(const unsigned char *data, size_t len) {
    char *path = create_temp_path();
    if (!path)
        return NULL;

    FILE *file = fopen(path, "wb");
    if (!file || (len && fwrite(data, 1, len, file) != len)) {
        if (file)
            fclose(file);

        unlink(path);
        free(path);
        return NULL;
    }

    fclose(file);
    return path;
}
