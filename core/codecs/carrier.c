#include "codec.h"
#include <string.h>

enum CodecType str_to_codec(const char *str) {
    if (!str)
        return CODEC_UNKNOWN;

    if (strcmp(str, "lsbm") == 0)
        return CODEC_LSB_MATCHING;

    if (strcmp(str, "lsbr") == 0)
        return CODEC_LSB_REPLACEMENT;

    if (strcmp(str, "dct") == 0)
        return CODEC_DCT;

    return CODEC_UNKNOWN;
}
