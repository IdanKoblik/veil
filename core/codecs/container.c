#include "container.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <veil/log.h>

static int derive_keys(const char *passphrase, const unsigned char *salt, unsigned char *box_key, unsigned char *prng_key, unsigned char *payload_key) {
    unsigned char material[crypto_secretbox_KEYBYTES + PRNG_KEYBYTES + PAYLOAD_KEY_LEN];

    DEBUG("Deriving keys from the passphrase");
    if (crypto_pwhash(material, sizeof(material), passphrase, strlen(passphrase), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT) != 0) {
        ERROR("Key derivation failed, out of memory");
        return -1;
    }

    memcpy(box_key, material, crypto_secretbox_KEYBYTES);
    memcpy(prng_key, material + crypto_secretbox_KEYBYTES, PRNG_KEYBYTES);
    memcpy(payload_key, material + crypto_secretbox_KEYBYTES + PRNG_KEYBYTES, PAYLOAD_KEY_LEN);
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

static int payload_write(struct Container *container, const unsigned char *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            const unsigned char value = (bytes[i] >> bit) & 1;
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

static int payload_read(struct Container *container, unsigned char *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            const size_t slot = scatter_next(&container->scatter);
            if (slot == SIZE_MAX) {
                ERROR("Ran past the end of the carrier while decoding");
                return -1;
            }

            const unsigned char value = container->carrier->read(container->carrier, slot + container->preamble_bits);
            if (value > 1) {
                ERROR("Failed to read carrier slot %zu", slot + container->preamble_bits);
                return -1;
            }

            byte |= (unsigned char)(value << bit);
        }

        bytes[i] = byte;
    }

    return 0;
}

static int stream_push(struct Container *container, unsigned char tag) {
    unsigned long long cipher_len = 0;
    crypto_secretstream_xchacha20poly1305_push(&container->stream, container->stream_cipher, &cipher_len, container->stream_plain, container->stream_fill, NULL, 0, tag);

    const int rc = payload_write(container, container->stream_cipher, (size_t)cipher_len);
    sodium_memzero(container->stream_plain, container->stream_fill);
    container->stream_fill = 0;

    return rc;
}

static int stream_pull(struct Container *container) {
    // Chunks are cut at a fixed size and the last one is always tagged final, even when empty, so the plaintext length alone fixes the layout.
    const size_t left = container->header.payload_len - container->stream_total;
    const size_t len = left < CONTAINER_STREAM_CHUNK ? left : CONTAINER_STREAM_CHUNK;
    const unsigned char want = left < CONTAINER_STREAM_CHUNK ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;

    if (payload_read(container, container->stream_cipher, len + CONTAINER_STREAM_ABYTES) < 0)
        return -1;

    unsigned long long plain_len = 0;
    unsigned char tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(&container->stream, container->stream_plain, &plain_len, &tag, container->stream_cipher, len + CONTAINER_STREAM_ABYTES, NULL, 0) != 0 || tag != want) {
        ERROR("The payload is corrupted or was tampered with (%s)", container->target);
        return -1;
    }

    container->stream_fill = (size_t)plain_len;
    container->stream_pos = 0;
    container->stream_total += (size_t)plain_len;
    container->stream_done = tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL;

    return 0;
}

static void stream_wipe(struct Container *container) {
    sodium_memzero(&container->stream, sizeof(container->stream));
    sodium_memzero(container->stream_plain, sizeof(container->stream_plain));
    sodium_memzero(container->stream_cipher, sizeof(container->stream_cipher));
    container->stream_fill = 0;
    container->stream_pos = 0;
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
    container->header_bits = CONTAINER_CLEAR_BYTES * 8;
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

        unsigned char prng_key[PRNG_KEYBYTES];
        unsigned char payload_key[PAYLOAD_KEY_LEN];
        if (derive_keys(passphrase, header.salt, container->box_key, prng_key, payload_key) < 0) {
            sodium_memzero(prng_key, sizeof(prng_key));
            sodium_memzero(payload_key, sizeof(payload_key));
            goto fail;
        }

        crypto_secretstream_xchacha20poly1305_init_push(&container->stream, header.payload_stream, payload_key);
        sodium_memzero(payload_key, sizeof(payload_key));

        // payload_stream is sealed, so the scatter is seeded from the nonce the decoder can read in the clear.
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
    stream_wipe(container);
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

    if (encrypted) {
        if (header->payload_len != container->stream_total) {
            ERROR("The header claims %zu payload bytes but %zu were encoded", header->payload_len, container->stream_total);
            return -1;
        }

        // The final chunk has to land in the carrier before the header, which is written last and closes the payload.
        if (!container->stream_done) {
            if (stream_push(container, crypto_secretstream_xchacha20poly1305_TAG_FINAL) < 0)
                return -1;

            container->stream_done = 1;
        }
    }

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

    secret[PAYLOAD_LEN_BYTES] = header->checksum;
    memcpy(secret + CONTAINER_CLEAR_BYTES, header->payload_stream, PAYLOAD_STREAM_LEN);

    int rc;
    if (!encrypted) {
        rc = write_bytes(container->carrier, secret, CONTAINER_CLEAR_BYTES, container->header_slots, 0);
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
    size_t header_bits = CONTAINER_CLEAR_BYTES * 8;
    if (encrypted) {
        preamble_bits += CONTAINER_KDF_BYTES * 8;
        header_bits = CONTAINER_SEALED_BYTES * 8;
    }

    if (capacity < preamble_bits) {
        ERROR("The carrier is too small to hold an encrypted container (%s)", container->target);
        return -1;
    }

    unsigned char prng_key[PRNG_KEYBYTES];
    unsigned char payload_key[PAYLOAD_KEY_LEN];
    if (encrypted) {
        if (read_bytes(carrier, preamble + off, CONTAINER_KDF_BYTES, NULL, off * 8) < 0) {
            ERROR("Failed to read the container salt and nonce (%s)", container->target);
            return -1;
        }

        memcpy(header.salt, preamble + off, SALT_LEN);
        off += SALT_LEN;
        memcpy(header.header_nonce, preamble + off, HEADER_NONCE_LEN);

        const int derived = derive_keys(container->passphrase, header.salt, container->box_key, prng_key, payload_key);
        passphrase_wipe(&container->passphrase);
        if (derived < 0) {
            sodium_memzero(prng_key, sizeof(prng_key));
            sodium_memzero(payload_key, sizeof(payload_key));
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

    if (rc < 0) {
        sodium_memzero(payload_key, sizeof(payload_key));
        return -1;
    }

    container->preamble_bits = preamble_bits;
    container->header_bits = header_bits;
    if (container_reserve_header(container) < 0) {
        sodium_memzero(payload_key, sizeof(payload_key));
        return -1;
    }

    unsigned char secret[CONTAINER_SECRET_BYTES] = {0};
    if (!encrypted) {
        rc = read_bytes(carrier, secret, CONTAINER_CLEAR_BYTES, container->header_slots, 0);
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
        sodium_memzero(payload_key, sizeof(payload_key));
        return -1;
    }

    uint64_t payload_len = 0;
    for (size_t i = 0; i < PAYLOAD_LEN_BYTES; i++)
        payload_len |= (uint64_t)secret[i] << (i * 8);

    header.checksum = secret[PAYLOAD_LEN_BYTES];
    memcpy(header.payload_stream, secret + CONTAINER_CLEAR_BYTES, PAYLOAD_STREAM_LEN);
    sodium_memzero(secret, sizeof(secret));

    const size_t remaining_bytes = (scatter_capacity - container->scatter.pos) / 8;
    int fits = payload_len <= remaining_bytes;
    if (fits && encrypted) {
        const uint64_t tags = (payload_len / CONTAINER_STREAM_CHUNK + 1) * CONTAINER_STREAM_ABYTES;
        fits = tags <= remaining_bytes - payload_len;
    }

    if (!fits) {
        ERROR("Container claims a %llu byte payload the carrier can't hold", (unsigned long long)payload_len);
        sodium_memzero(payload_key, sizeof(payload_key));
        return -1;
    }

    if (encrypted) {
        rc = crypto_secretstream_xchacha20poly1305_init_pull(&container->stream, header.payload_stream, payload_key);
        sodium_memzero(payload_key, sizeof(payload_key));
        if (rc != 0) {
            ERROR("Wrong passphrase or corrupted container header");
            return -1;
        }
    }

    DEBUG("Found a container version %u, %s, holding %llu bytes", header.version, encrypted ? "encrypted" : "in the clear", (unsigned long long)payload_len);

    header.payload_len = payload_len;
    container->header = header;
    return 0;
}

int container_encode_chunk(struct Container *container, const unsigned char *buffer, size_t buffer_len) {
    if (!container)
        return -1;

    if (!(container->header.flags & VEIL_FLAG_ENCRYPTED))
        return payload_write(container, buffer, buffer_len);

    if (container->stream_done)
        return -1;

    while (buffer_len > 0) {
        const size_t room = CONTAINER_STREAM_CHUNK - container->stream_fill;
        const size_t take = buffer_len < room ? buffer_len : room;

        memcpy(container->stream_plain + container->stream_fill, buffer, take);
        container->stream_fill += take;
        container->stream_total += take;
        buffer += take;
        buffer_len -= take;

        if (container->stream_fill == CONTAINER_STREAM_CHUNK && stream_push(container, crypto_secretstream_xchacha20poly1305_TAG_MESSAGE) < 0)
            return -1;
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

    if (!(container->header.flags & VEIL_FLAG_ENCRYPTED)) {
        if (payload_read(container, out, buffer_len) < 0)
            goto fail;

        *buffer = out;
        return 0;
    }

    size_t n = 0;
    while (n < buffer_len) {
        if (container->stream_pos == container->stream_fill) {
            if (container->stream_done) {
                ERROR("Asked for more than the %zu byte payload", container->header.payload_len);
                goto fail;
            }

            if (stream_pull(container) < 0)
                goto fail;

            continue;
        }

        const size_t ready = container->stream_fill - container->stream_pos;
        const size_t take = buffer_len - n < ready ? buffer_len - n : ready;
        memcpy(out + n, container->stream_plain + container->stream_pos, take);
        container->stream_pos += take;
        n += take;
    }

    // A payload that ends on a chunk boundary still owes an empty final chunk, pull it so its tag is checked.
    if (container->stream_total == container->header.payload_len && container->stream_pos == container->stream_fill && !container->stream_done && stream_pull(container) < 0)
        goto fail;

    *buffer = out;
    return 0;
fail:
    sodium_memzero(out, buffer_len);
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
    stream_wipe(container);
    passphrase_wipe(&container->passphrase);

    free(container);
}
