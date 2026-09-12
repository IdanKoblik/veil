#include "encode.h"
#include "codecs/codec.h"
#include "fs/file.h"
#include "handlers/image.h"
#include "log.h"
#include <stdlib.h>

int encode(const char *target, const char *output, enum CodecType codec_type, const char passphrase[PASSPHRASE_MAX], unsigned char *data, size_t data_len, void *ctx_) {
    if (!target) {
        ERROR("Target is absent");
        return -1;
    }

    if(!data) {
        ERROR("Source is absent");
        return -1;
    }

    if (data_len <= 0) {
        ERROR("Cannot encode data, the data is empty");
        return -1;
    }

    enum FileType file_type = get_file_type(target);
    if (file_type == TYPE_NOT_FOUND) {
        ERROR("Target file was not found");
        return -1;
    }

    if (file_type == TYPE_PNG_IMAGE && (codec_type != CODEC_LSB_REPLACEMENT && codec_type != CODEC_LSB_MATCHING)) {
        ERROR("Invalid codec_file_type for a png image");
        return -1;
    } else if (file_type == TYPE_JPEG_IMAGE) {
        codec_type = CODEC_DCT;
    }

    if (is_image_file(file_type)) {
        struct ImageCtx ctx = {
            .source_file = target,
            .output_file = output,

            .image_type = file_type,
            .codec_type = codec_type,

            .passphrase = passphrase[0] ? passphrase : NULL
        };

        if (encode_image(&ctx, data, data_len) < 0) {
            ERROR("Failed to encode data to the targeted file");
            return -1;
        }


        *(struct ImageCtx *)ctx_ = ctx;
    }

    return 0;
}
