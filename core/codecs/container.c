#include "container.h"
#include <stdint.h>
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

static int write_bytes(Carrier *carrier, const unsigned char *bytes, size_t len, const size_t *slots, size_t first_slot) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            const unsigned char value = (bytes[i] >> bit) & 1;
            const size_t slot = slots ? slots[n] : first_slot + n;
            if (carrier->write(carrier, slot, value) < 0)
                return -1;

            n++;
        }
    }

    return 0;
}

struct Container *container_init(const char *target, char passphrase[PASSPHRASE_MAX]) {
    if (!target)
        return NULL;

    struct Container *container = calloc(1, sizeof(*container));
    if (!container)
        return NULL;

    container->target = target;
    Carrier *carrier = figure_carrier(target);
    if (!carrier)
        goto fail;

    container->carrier = carrier;

    const int encrypted = passphrase && passphrase[0] != '\0';
    if (encrypted) {
        strncpy(container->passphrase, passphrase, PASSPHRASE_MAX - 1);
        container->passphrase[PASSPHRASE_MAX - 1] = '\0';
    }

    struct ContainerHeader header = {
        .version = VEIL_VERSION,
        .payload_len = 0,
        .flags = encrypted ? VEIL_FLAG_ENCRYPTED : 0
    };

    memcpy(header.magic, VEIL_MAGIC, VEIL_MAGIC_LEN);

    container->preamble_bits = CONTAINER_PREAMBLE_BYTES * 8;
    container->header_bits = PAYLOAD_LEN_BYTES * 8;
    if (encrypted) {
        container->preamble_bits += CONTAINER_KDF_BYTES * 8;
        container->header_bits = CONTAINER_SEALED_BYTES * 8;
    }

    const int capacity = carrier->capacity(carrier);
    if (capacity < 0 || (size_t)capacity < container->preamble_bits)
        goto fail;

    // The preamble sits at fixed slots because the decoder has to read the salt before it holds any key.
    const size_t scatter_capacity = (size_t)capacity - container->preamble_bits;

    if (!encrypted) {
        if (scatter_init(&container->scatter, scatter_capacity, 0, NULL, NULL) < 0)
            goto fail;
    } else {
        randombytes_buf(header.salt, sizeof(header.salt));
        randombytes_buf(header.header_nonce, sizeof(header.header_nonce));
        randombytes_buf(header.payload_nonce, sizeof(header.payload_nonce));

        unsigned char prng_key[PRNG_KEYBYTES];
        if (derive_keys(passphrase, header.salt, container->box_key, prng_key) < 0) {
            sodium_memzero(prng_key, sizeof(prng_key));
            goto fail;
        }

        // payload_nonce is sealed, so the scatter is seeded from the nonce the decoder can read in the clear.
        const int rc = scatter_init(&container->scatter, scatter_capacity, 1, prng_key, header.header_nonce);
        sodium_memzero(prng_key, sizeof(prng_key));
        if (rc < 0)
            goto fail;
    }

    container->header = header;
    container->header_reserved = 0;
    return container;
fail:
    sodium_memzero(container->box_key, sizeof(container->box_key));
    free(container);
    return NULL;
}

int container_reserve_header(struct Container *container) {
    if (!container || container->header_reserved)
        return -1;

    for (size_t i = 0; i < container->header_bits; i++) {
        const size_t slot = scatter_next(&container->scatter);
        if (slot == SIZE_MAX)
            return -1;

        container->header_slots[i] = slot + container->preamble_bits;
    }

    container->header_reserved = 1;
    return 0;
}

int container_write_header(struct Container *container) {
    if (!container || !container->header_reserved)
        return -1;

    const struct ContainerHeader *header = &container->header;
    const int encrypted = header->flags & VEIL_FLAG_ENCRYPTED;

    unsigned char preamble[CONTAINER_PREAMBLE_BYTES + CONTAINER_KDF_BYTES];
    size_t off = 0;

    memcpy(preamble + off, header->magic, VEIL_MAGIC_LEN);
    off += VEIL_MAGIC_LEN;
    preamble[off++] = header->version;
    preamble[off++] = header->flags;

    if (encrypted) {
        memcpy(preamble + off, header->salt, SALT_LEN);
        off += SALT_LEN;
        memcpy(preamble + off, header->header_nonce, HEADER_NONCE_LEN);
        off += HEADER_NONCE_LEN;
    }

    if (write_bytes(container->carrier, preamble, off, NULL, 0) < 0)
        return -1;

    unsigned char secret[CONTAINER_SECRET_BYTES];

    // Fixed width little-endian so the layout doesn't depend on the encoder's size_t.
    for (size_t i = 0; i < PAYLOAD_LEN_BYTES; i++)
        secret[i] = (unsigned char)((uint64_t)header->payload_len >> (i * 8));

    memcpy(secret + PAYLOAD_LEN_BYTES, header->payload_nonce, PAYLOAD_NONCE_LEN);

    int rc;
    if (!encrypted) {
        rc = write_bytes(container->carrier, secret, PAYLOAD_LEN_BYTES, container->header_slots, 0);
    } else {
        unsigned char sealed[CONTAINER_SEALED_BYTES];
        crypto_secretbox_easy(sealed, secret, sizeof(secret), header->header_nonce, container->box_key);
        rc = write_bytes(container->carrier, sealed, sizeof(sealed), container->header_slots, 0);
    }

    sodium_memzero(secret, sizeof(secret));
    return rc;
}

int container_encode_chunk(struct Container *container, const unsigned char *buffer, size_t buffer_len) {
    if (!container)
        return -1;

    for (size_t i = 0; i < buffer_len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            const unsigned char value = (buffer[i] >> bit) & 1;
            size_t slot = scatter_next(&container->scatter);
            if (slot == SIZE_MAX)
                return -1;

            if (container->carrier->write(container->carrier, slot + container->preamble_bits, value) < 0)
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

    scatter_clear(&container->scatter);
    sodium_memzero(container->box_key, sizeof(container->box_key));

    free(container);
}
