#pragma once

#include "crypto/digest.h"
#include "crypto/passphrase.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int decode(const char *target, const char *output, char passphrase[PASSPHRASE_MAX], size_t *payload_len);
int decode_digest(const char *target, char passphrase[PASSPHRASE_MAX], unsigned char digest[PAYLOAD_DIGEST_BYTES]);

#ifdef __cplusplus
}
#endif
