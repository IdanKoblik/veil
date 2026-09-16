#include "container.h"
#include <stdlib.h>
#include <string.h>
#include <veil/log.h>

static int derive_keys(const char *passphrase, const unsigned char *salt, unsigned char *box_key, unsigned char *prng_key) {
    unsigned char material[crypto_secretbox_KEYBYTES + PRNG_KEYBYTES];

    DEBUG("Deriving keys from the passphrase");
    if (crypto_pwhash(material, sizeof(material), passphrase, strlen(passphrase), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT) != 0) {
        ERROR("Key derivation failed, out of memory");
        return -1;
    }

    memcpy(box_key, material, crypto_secretbox_KEYBYTES);
    memcpy(prng_key, material + crypto_secretbox_KEYBYTES, PRNG_KEYBYTES);
    sodium_memzero(material, sizeof(material));

    return 0;
}

struct Container *container_init(const char *target, char passphrase[PASSPHRASE_MAX]) {
    struct Container *container = malloc(sizeof(*container));
    if (!container)
        return NULL;

    if (!target)
        goto fail;

    container->target = target;
    Carrier *carrier = figure_carrier(target);
    if (!carrier)
        goto fail;

    container->carrier = carrier;

    strncpy(container->passphrase, passphrase, PASSPHRASE_MAX - 1);
    container->passphrase[PASSPHRASE_MAX - 1] = '\0';

    struct ContainerHeader header = {
        .version = VEIL_VERSION,
        .payload_len = 0,
        .flags = 0
    };

    memcpy(header.magic, VEIL_MAGIC, VEIL_MAGIC_LEN);
    if (!passphrase || passphrase[0] == '\0') {
        if (scatter_init(container->scatter, carrier->capacity(carrier), 0, NULL, NULL) < 0)
            goto fail;
    } else {
        randombytes_buf(header.salt, sizeof(header.salt));
        randombytes_buf(header.header_nonce, sizeof(header.header_nonce));
        randombytes_buf(header.payload_nonce, sizeof(header.payload_nonce));

        unsigned char box_key[crypto_secretbox_KEYBYTES];
        unsigned char prng_key[PRNG_KEYBYTES];
        if (derive_keys(passphrase, header.salt, box_key, prng_key) < 0) {
            sodium_memzero(box_key, sizeof(box_key));
            sodium_memzero(prng_key, sizeof(prng_key));
            goto fail;
        }

        if (scatter_init(container->scatter, carrier->capacity(carrier), 1, prng_key, header.payload_nonce) < 0) {
            sodium_memzero(box_key, sizeof(box_key));
            sodium_memzero(prng_key, sizeof(prng_key));
            goto fail;
        }

        header.flags = VEIL_FLAG_ENCRYPTED;
    }

    return container;
fail:
    free(container);
    return NULL;
}

int container_encode_chunk(struct Container *container, const unsigned char *buffer, size_t buffer_len) {
    if (!container)
        return -1;

    for (size_t i = 0; i < buffer_len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            unsigned char value = (buffer[i] >> bit) & 1;
            size_t slot = scatter_next(container->scatter);

            if (container->carrier->write(container->carrier, slot, value) < 0)
                return -1;
        }
    }

    return 0;
}

void container_free(struct Container *container) {
    if (!container)
        return;

    if (container->carrier)
        container->carrier->free(container->carrier);

    if (container->scatter)
        scatter_clear(container->scatter);

    free(container);
}
