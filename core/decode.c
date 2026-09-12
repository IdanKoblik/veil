#include "decode.h"
#include "codecs/codec.h"
#include "fs/file.h"
#include "handlers/image.h"
#include "log.h"


int decode(const char *target, const char passphrase[PASSPHRASE_MAX], unsigned char **data, size_t *data_len) {
    if (!target) {
        ERROR("Target is absent");
        return -1;
    }

    enum FileType type = get_file_type(target);
    if (type == TYPE_NOT_FOUND) {
        ERROR("Target file was not found");
        return -1;
    }

    int decode_status = -1;
     if (is_image_file(type)) {
        struct ImageCtx ctx = {
            .source_file = target,
            .output_file = NULL,

            .image_type = type,
            .codec_type = CODEC_UNKNOWN,

            .passphrase = passphrase[0] ? passphrase : NULL,
        };

        decode_status = decode_image(&ctx, data, data_len);
    }

    if (decode_status < 0) {
        ERROR("Failed to decode data out of the targeted file");
        return -1;
    }

    return 0;
}
