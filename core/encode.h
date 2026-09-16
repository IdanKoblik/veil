#pragma once

#include "crypto/digest.h"
#include "crypto/passphrase.h"

#ifdef __cplusplus
extern "C" {
#endif

int encode(const char *target, const char *data_file, const char *output, char passphrase[PASSPHRASE_MAX], unsigned char digest[PAYLOAD_DIGEST_BYTES], size_t *payload_len);

#ifdef __cplusplus
}
#endif
