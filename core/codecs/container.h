#pragma once

#include <stddef.h>
#include <veil/codecs/carrier.h>
#include <veil/crypto/passphrase.h>
#include "scatter.h"

#define VEIL_MAGIC "VEIL"
#define VEIL_MAGIC_LEN (sizeof(VEIL_MAGIC) - 1)
#define VEIL_VERSION 2

#define VEIL_FLAG_ENCRYPTED 0x01

struct Container {
    Carrier *carrier;
    struct Scatter *scatter;

    const char *target;
    char passphrase[PASSPHRASE_MAX];
};

#define SALT_LEN crypto_pwhash_SALTBYTES
#define HEADER_NONCE_LEN crypto_secretbox_NONCEBYTES
#define PAYLOAD_NONCE_LEN crypto_secretbox_NONCEBYTES

struct ContainerHeader {
    unsigned char magic[VEIL_MAGIC_LEN];
    unsigned char version;
    unsigned char flags;

    size_t payload_len;

    unsigned char salt[SALT_LEN];
    unsigned char header_nonce[HEADER_NONCE_LEN];
    unsigned char payload_nonce[PAYLOAD_NONCE_LEN];
};

struct Container *container_init(const char *target, char passphrase[PASSPHRASE_MAX]);
int container_encode_chunk(struct Container *container, const unsigned char *buffer, size_t buffer_len);
void container_free(struct Container *container);
