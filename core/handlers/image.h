#pragma once

#include <veil/codecs/codec.h>
#include <veil/fs/file.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ImageCtx {
    int height;
    int width;
    int channels;

    const char *source_file;
    const char *output_file;

    enum CodecType codec_type;
    enum FileType image_type;

    const char *passphrase;
};

enum FileType detect_image_type(const char *target);

int encode_image(struct ImageCtx *ctx, unsigned char *data, size_t data_len);
int decode_image(struct ImageCtx *ctx, unsigned char **data, size_t *data_len);

#ifdef __cplusplus
}
#endif
