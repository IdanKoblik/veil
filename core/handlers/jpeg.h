#pragma once

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>

#include <jpeglib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct JpegError {
    struct jpeg_error_mgr mgr;
    jmp_buf escape;
};

struct JpegImage {
    struct jpeg_decompress_struct decoder;
    struct JpegError error;
    jvirt_barray_ptr *arrays;
    FILE *file;
};

typedef void (*CoefficientVisitor)(JCOEF *coefficient, size_t slot, void *ctx);

// A coefficient of 0 or 1 is left untouched, so it carries nothing.
int jpeg_coefficient_usable(JCOEF value);

int jpeg_image_open(struct JpegImage *image, const char *path);
void jpeg_image_close(struct JpegImage *image);
int jpeg_image_write(struct JpegImage *image, const char *path);

size_t jpeg_walk_coefficients(struct JpegImage *image, CoefficientVisitor visit, void *ctx);

#ifdef __cplusplus
}
#endif
