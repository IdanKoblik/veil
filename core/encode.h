#pragma once

#include "codecs/codec.h"
#include "crypto/passphrase.h"

int encode(const char *target, const char *output, enum CodecType codec_type, const char passphrase[PASSPHRASE_MAX], unsigned char *data, size_t data_len, void *ctx_);
