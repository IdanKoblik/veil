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
    // Least significant bit first, the order analysis/stream.c rebuilds bytes in.
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            const unsigned char value = (bytes[i] >> bit) & 1;
            const size_t slot = slots ? slots[n] : first_slot + n;
            if (carrier->write(carrier, slot, value) < 0)
                return -1;

            n++;
        }
    }

    return 0;
}

static int read_bytes(Carrier *carrier, unsigned char *bytes, size_t len, const size_t *slots, size_t first_slot) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            const size_t slot = slots ? slots[n] : first_slot + n;
            const unsigned char value = carrier->read(carrier, slot);
            if (value > 1)
                return -1;

            byte |= (unsigned char)(value << bit);
            n++;
        }

        bytes[i] = byte;
    }

    return 0;
}

struct Container *container_init(const char *target, char passphrase[PASSPHRASE_MAX]) {
    struct Container *container = calloc(1, sizeof(*container));
    if (!container) {
        ERROR("Failed to allocate the container");
        return NULL;
    }

    container->target = target ? target : "pipe";
    Carrier *carrier = figure_carrier(target);
    if (!carrier)
        goto fail;

    container->carrier = carrier;

    const int encrypted = passphrase && passphrase[0] != '\0';
    if (encrypted) {
        if (sodium_mlock(container->passphrase, sizeof(container->passphrase)) != 0)
            DEBUG("Could not lock the container passphrase, it may reach swap");

        strncpy(container->passphrase, passphrase, PASSPHRASE_MAX - 1);
        container->passphrase[PASSPHRASE_MAX - 1] = '\0';
    }

    struct ContainerHeader header = {.version = VEIL_CONTAINER_VERSION, .payload_len = 0, .flags = encrypted ? VEIL_FLAG_ENCRYPTED : 0};

    memcpy(header.magic, VEIL_MAGIC, VEIL_MAGIC_LEN);

    container->preamble_bits = CONTAINER_PREAMBLE_BYTES * 8;
    container->header_bits = PAYLOAD_LEN_BYTES * 8;
    if (encrypted) {
        container->preamble_bits += CONTAINER_KDF_BYTES * 8;
        container->header_bits = CONTAINER_SEALED_BYTES * 8;
    }

    const size_t capacity = carrier->capacity(carrier);
    if (capacity < container->preamble_bits) {
        ERROR("The carrier is too small to hold a container (%s)", target);
        goto fail;
    }

    DEBUG("Container over %s: %zu slots, %s", target, capacity, encrypted ? "encrypted" : "in the clear");

    // The preamble sits at fixed slots because the decoder has to read the salt before it holds any key.
    const size_t scatter_capacity = capacity - container->preamble_bits;
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
    passphrase_wipe(&container->passphrase);
    free(container);
    return NULL;
}

int container_reserve_header(struct Container *container) {
    if (!container || container->header_reserved)
        return -1;

    for (size_t i = 0; i < container->header_bits; i++) {
        const size_t slot = scatter_next(&container->scatter);
        if (slot == SIZE_MAX) {
            ERROR("The carrier has no room for the container header");
            return -1;
        }

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

    DEBUG("Writing the container header for a %zu byte payload", header->payload_len);
    if (write_bytes(container->carrier, preamble, off, NULL, 0) < 0) {
        ERROR("Failed to write the container preamble");
        return -1;
    }

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
    if (rc < 0)
        ERROR("Failed to write the container header");

    return rc;
}

int container_read_header(struct Container *container) {
    if (!container || container->header_reserved)
        return -1;

    Carrier *carrier = container->carrier;
    const size_t capacity = carrier->capacity(carrier);
    if (capacity < CONTAINER_PREAMBLE_BYTES * 8) {
        ERROR("The carrier is too small to hold a container (%s)", container->target);
        return -1;
    }

    struct ContainerHeader header = {0};
    unsigned char preamble[CONTAINER_PREAMBLE_BYTES + CONTAINER_KDF_BYTES];
    if (read_bytes(carrier, preamble, CONTAINER_PREAMBLE_BYTES, NULL, 0) < 0) {
        ERROR("Failed to read the container preamble (%s)", container->target);
        return -1;
    }

    size_t off = 0;
    memcpy(header.magic, preamble + off, VEIL_MAGIC_LEN);
    off += VEIL_MAGIC_LEN;
    header.version = preamble[off++];
    header.flags = preamble[off++];

    if (memcmp(header.magic, VEIL_MAGIC, VEIL_MAGIC_LEN) != 0) {
        ERROR("No container found in %s", container->target);
        return -1;
    }

    if (header.version != VEIL_CONTAINER_VERSION) {
        ERROR("Unsupported container version %u", header.version);
        return -1;
    }

    if (header.flags & ~VEIL_FLAG_ENCRYPTED) {
        ERROR("Unknown container flags 0x%02x", header.flags);
        return -1;
    }

    const int encrypted = header.flags & VEIL_FLAG_ENCRYPTED;
    if (encrypted && container->passphrase[0] == '\0') {
        ERROR("Container is encrypted but no passphrase was given");
        return -1;
    }

    size_t preamble_bits = CONTAINER_PREAMBLE_BYTES * 8;
    size_t header_bits = PAYLOAD_LEN_BYTES * 8;
    if (encrypted) {
        preamble_bits += CONTAINER_KDF_BYTES * 8;
        header_bits = CONTAINER_SEALED_BYTES * 8;
    }

    if (capacity < preamble_bits) {
        ERROR("The carrier is too small to hold an encrypted container (%s)", container->target);
        return -1;
    }

    unsigned char prng_key[PRNG_KEYBYTES];
    if (encrypted) {
        if (read_bytes(carrier, preamble + off, CONTAINER_KDF_BYTES, NULL, off * 8) < 0) {
            ERROR("Failed to read the container salt and nonce (%s)", container->target);
            return -1;
        }

        memcpy(header.salt, preamble + off, SALT_LEN);
        off += SALT_LEN;
        memcpy(header.header_nonce, preamble + off, HEADER_NONCE_LEN);

        const int derived = derive_keys(container->passphrase, header.salt, container->box_key, prng_key);
        passphrase_wipe(&container->passphrase);
        if (derived < 0) {
            sodium_memzero(prng_key, sizeof(prng_key));
            return -1;
        }
    }

    scatter_clear(&container->scatter);
    const size_t scatter_capacity = capacity - preamble_bits;

    int rc;
    if (encrypted) {
        rc = scatter_init(&container->scatter, scatter_capacity, 1, prng_key, header.header_nonce);
        sodium_memzero(prng_key, sizeof(prng_key));
    } else {
        rc = scatter_init(&container->scatter, scatter_capacity, 0, NULL, NULL);
    }

    if (rc < 0)
        return -1;

    container->preamble_bits = preamble_bits;
    container->header_bits = header_bits;
    if (container_reserve_header(container) < 0)
        return -1;

    unsigned char secret[CONTAINER_SECRET_BYTES] = {0};
    if (!encrypted) {
        rc = read_bytes(carrier, secret, PAYLOAD_LEN_BYTES, container->header_slots, 0);
        if (rc < 0)
            ERROR("Failed to read the container header (%s)", container->target);
    } else {
        unsigned char sealed[CONTAINER_SEALED_BYTES];
        rc = read_bytes(carrier, sealed, sizeof(sealed), container->header_slots, 0);
        if (rc < 0)
            ERROR("Failed to read the container header (%s)", container->target);
        else if (crypto_secretbox_open_easy(secret, sealed, sizeof(sealed), header.header_nonce, container->box_key) != 0) {
            ERROR("Wrong passphrase or corrupted container header");
            rc = -1;
        }
    }

    if (rc < 0) {
        sodium_memzero(secret, sizeof(secret));
        return -1;
    }

    uint64_t payload_len = 0;
    for (size_t i = 0; i < PAYLOAD_LEN_BYTES; i++)
        payload_len |= (uint64_t)secret[i] << (i * 8);

    memcpy(header.payload_nonce, secret + PAYLOAD_LEN_BYTES, PAYLOAD_NONCE_LEN);
    sodium_memzero(secret, sizeof(secret));

    const size_t remaining_bits = scatter_capacity - container->scatter.pos;
    if (payload_len > remaining_bits / 8) {
        ERROR("Container claims a %llu byte payload the carrier can't hold", (unsigned long long)payload_len);
        return -1;
    }

    DEBUG("Found a container version %u, %s, holding %llu bytes", header.version, encrypted ? "encrypted" : "in the clear", (unsigned long long)payload_len);

    header.payload_len = payload_len;
    container->header = header;
    return 0;
}

int container_encode_chunk(struct Container *container, const unsigned char *buffer, size_t buffer_len) {
    if (!container)
        return -1;

    for (size_t i = 0; i < buffer_len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            const unsigned char value = (buffer[i] >> bit) & 1;
            size_t slot = scatter_next(&container->scatter);
            if (slot == SIZE_MAX) {
                ERROR("The payload doesn't fit in the carrier");
                return -1;
            }

            if (container->carrier->write(container->carrier, slot + container->preamble_bits, value) < 0) {
                ERROR("Failed to write carrier slot %zu", slot + container->preamble_bits);
                return -1;
            }
        }
    }

    return 0;
}

int container_decode_chunk(struct Container *container, unsigned char **buffer, size_t buffer_len) {
    if (!container || !buffer || !container->header_reserved)
        return -1;

    unsigned char *out = malloc(buffer_len ? buffer_len : 1);
    if (!out) {
        ERROR("Failed to allocate a %zu byte chunk", buffer_len);
        return -1;
    }

    for (size_t i = 0; i < buffer_len; i++) {
        unsigned char byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            const size_t slot = scatter_next(&container->scatter);
            if (slot == SIZE_MAX) {
                ERROR("Ran past the end of the carrier while decoding");
                goto fail;
            }

            const unsigned char value = container->carrier->read(container->carrier, slot + container->preamble_bits);
            if (value > 1) {
                ERROR("Failed to read carrier slot %zu", slot + container->preamble_bits);
                goto fail;
            }

            byte |= (unsigned char)(value << bit);
        }

        out[i] = byte;
    }

    *buffer = out;
    return 0;
fail:
    free(out);
    return -1;
}

void container_free(struct Container *container) {
    if (!container)
        return;

    if (container->carrier)
        container->carrier->free(container->carrier);

    scatter_clear(&container->scatter);
    sodium_memzero(container->box_key, sizeof(container->box_key));
    passphrase_wipe(&container->passphrase);

    free(container);
}
