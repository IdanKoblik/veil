#pragma once

#include "scatter.h"
#include <stddef.h>
#include <veil/codecs/carrier.h>
#include <veil/crypto/passphrase.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VEIL_MAGIC "VEIL"
#define VEIL_MAGIC_LEN (sizeof(VEIL_MAGIC) - 1)
#define VEIL_CONTAINER_VERSION 3

#define VEIL_FLAG_ENCRYPTED 0x01

#define SALT_LEN crypto_pwhash_SALTBYTES
#define HEADER_NONCE_LEN crypto_secretbox_NONCEBYTES
#define PAYLOAD_NONCE_LEN crypto_secretbox_NONCEBYTES

#define PAYLOAD_LEN_BYTES 8
#define PAYLOAD_CHECKSUM_BYTES 1

#define CONTAINER_PREAMBLE_BYTES (VEIL_MAGIC_LEN + 1 + 1)
#define CONTAINER_KDF_BYTES (SALT_LEN + HEADER_NONCE_LEN)
#define CONTAINER_CLEAR_BYTES (PAYLOAD_LEN_BYTES + PAYLOAD_CHECKSUM_BYTES)
#define CONTAINER_SECRET_BYTES (CONTAINER_CLEAR_BYTES + PAYLOAD_NONCE_LEN)
#define CONTAINER_SEALED_BYTES (CONTAINER_SECRET_BYTES + crypto_secretbox_MACBYTES)
#define CONTAINER_HEADER_BITS_MAX (CONTAINER_SEALED_BYTES * 8)

struct ContainerHeader {
    unsigned char magic[VEIL_MAGIC_LEN];
    unsigned char version;
    unsigned char flags;

    size_t payload_len;
    unsigned char checksum;

    unsigned char salt[SALT_LEN];
    unsigned char header_nonce[HEADER_NONCE_LEN];
    unsigned char payload_nonce[PAYLOAD_NONCE_LEN];
};

struct Container {
    Carrier *carrier;
    struct Scatter scatter;
    struct ContainerHeader header;
    size_t preamble_bits;
    size_t header_bits;
    size_t header_slots[CONTAINER_HEADER_BITS_MAX];
    int header_reserved;
    unsigned char box_key[crypto_secretbox_KEYBYTES];

    const char *target;
    char passphrase[PASSPHRASE_MAX];
};

struct Container *container_init(const char *target, char passphrase[PASSPHRASE_MAX]);

int container_reserve_header(struct Container *container);
int container_write_header(struct Container *container);
int container_read_header(struct Container *container);

int container_encode_chunk(struct Container *container, const unsigned char *buffer, size_t buffer_len);
int container_decode_chunk(struct Container *container, unsigned char **buffer, size_t buffer_len);
void container_free(struct Container *container);

#ifdef __cplusplus
}
#endif
