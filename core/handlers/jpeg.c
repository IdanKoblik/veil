#include "jpeg.h"
#include "veil/log.h"

int jpeg_coefficient_usable(JCOEF value) {
    return value != 0 && value != 1;
}

static void jpeg_escape(j_common_ptr info) {
    struct JpegError *error = (struct JpegError *)info->err;
    char message[JMSG_LENGTH_MAX];

    (*info->err->format_message)(info, message);
    ERROR("libjpeg: %s", message);

    longjmp(error->escape, 1);
}

int jpeg_image_open(struct JpegImage *image, const char *path) {
    image->arrays = NULL;
    image->file = fopen(path, "rb");

    if (!image->file) {
        ERROR("Failed to load the image (%s)", path);
        return -1;
    }

    image->decoder.err = jpeg_std_error(&image->error.mgr);
    image->error.mgr.error_exit = jpeg_escape;

    if (setjmp(image->error.escape)) {
        jpeg_destroy_decompress(&image->decoder);
        fclose(image->file);
        image->file = NULL;
        return -1;
    }

    jpeg_create_decompress(&image->decoder);
    jpeg_stdio_src(&image->decoder, image->file);
    jpeg_read_header(&image->decoder, TRUE);
    image->arrays = jpeg_read_coefficients(&image->decoder);

    return 0;
}

void jpeg_image_close(struct JpegImage *image) {
    if (!image->file)
        return;

    if (!setjmp(image->error.escape))
        jpeg_finish_decompress(&image->decoder);

    jpeg_destroy_decompress(&image->decoder);
    fclose(image->file);
    image->file = NULL;
}

size_t jpeg_walk_coefficients(struct JpegImage *image, CoefficientVisitor visit, void *ctx) {
    if (setjmp(image->error.escape))
        return 0;

    j_decompress_ptr decoder = &image->decoder;
    size_t slot = 0;

    // O(num of blocks). Because DCTSIZE2 is a compile-time constant.
    for (int component = 0; component < decoder->num_components; component++) {
        jpeg_component_info *info = &decoder->comp_info[component];

        for (JDIMENSION row = 0; row < info->height_in_blocks; row++) {
            JBLOCKARRAY blocks = (*decoder->mem->access_virt_barray)((j_common_ptr)decoder, image->arrays[component], row, 1, TRUE);

            for (JDIMENSION block = 0; block < info->width_in_blocks; block++) {
                JCOEFPTR coefficients = blocks[0][block];

                for (int index = 1; index < DCTSIZE2; index++) {
                    if (!jpeg_coefficient_usable(coefficients[index]))
                        continue;

                    if (visit)
                        visit(&coefficients[index], slot, ctx);

                    slot++;
                }
            }
        }
    }

    return slot;
}

int jpeg_image_write(struct JpegImage *image, const char *path) {
    struct jpeg_compress_struct encoder;
    FILE *const file = fopen(path, "wb");

    if (!file) {
        ERROR("Failed to write into the targeted file");
        return -1;
    }

    encoder.err = &image->error.mgr;

    if (setjmp(image->error.escape)) {
        jpeg_destroy_compress(&encoder);
        fclose(file);
        return -1;
    }

    jpeg_create_compress(&encoder);
    jpeg_stdio_dest(&encoder, file);
    jpeg_copy_critical_parameters(&image->decoder, &encoder);

    if (image->decoder.progressive_mode)
        jpeg_simple_progression(&encoder);

    jpeg_write_coefficients(&encoder, image->arrays);
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);

    return fclose(file) == 0 ? 0 : -1;
}
