#include "container.h"
#include <veil/crypto/payload.h>
#include <veil/crypto/prng.h>
#include <veil/log.h>

#include <sodium.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct ContainerHeader {
    unsigned char magic[HUSH_MAGIC_LEN];
    unsigned char version;
    unsigned char flags;
    unsigned char codec;

    uint32_t payload_len;

    unsigned char salt[crypto_pwhash_SALTBYTES];
    unsigned char header_nonce[crypto_secretbox_NONCEBYTES];
    unsigned char payload_nonce[crypto_secretbox_NONCEBYTES];
};

static int bitmap_test(const unsigned char *bitmap, size_t index) {
    return (bitmap[index / 8] >> (index % 8)) & 1;
}

static void bitmap_set(unsigned char *bitmap, size_t index) {
    bitmap[index / 8] |= (unsigned char)(1u << (index % 8));
}

static unsigned char data_bit(const unsigned char *data, size_t index) {
    return (data[index / 8] >> (index % 8)) & 1;
}

/* Expects a zeroed byte, which every extraction path memsets up front. */
static void data_bit_set(unsigned char *data, size_t index, unsigned char bit) {
    data[index / 8] |= (unsigned char)(bit << (index % 8));
}

static void embed_sequential(const Carrier *carrier, const unsigned char *data, size_t data_len, size_t start_slot) {
    size_t data_bits = data_len * 8;

    for (size_t i = 0; i < data_bits; i++)
        carrier->write(carrier, start_slot + i, data_bit(data, i));
}

static void extract_sequential(const Carrier *carrier, unsigned char *out, size_t out_len, size_t start_slot) {
    size_t out_bits = out_len * 8;

    memset(out, 0, out_len);
    for (size_t i = 0; i < out_bits; i++)
        data_bit_set(out, i, carrier->read(carrier, start_slot + i));
}

struct Scatter {
    struct Prng prng;
    unsigned char *taken;
    size_t range;
};

static int scatter_init(struct Scatter *walk, size_t range, const unsigned char *key, const unsigned char *nonce) {
    walk->taken = calloc((range + 7) / 8, 1);
    if (!walk->taken) {
        ERROR("Failed to allocate the slot map");
        return -1;
    }

    walk->range = range;
    prng_init(&walk->prng, key, nonce);

    return 0;
}

static size_t scatter_next(struct Scatter *walk) {
    size_t slot = (size_t)prng_uniform(&walk->prng, walk->range);

    while (bitmap_test(walk->taken, slot))
        slot = (slot + 1) % walk->range;

    bitmap_set(walk->taken, slot);

    return slot;
}

static void scatter_clear(struct Scatter *walk) {
    prng_clear(&walk->prng);
    free(walk->taken);
    walk->taken = NULL;
}

/* Spreads the payload over [start_slot, carrier->slots) in scatter order. */
static int embed_scattered(const Carrier *carrier, const unsigned char *data, size_t data_len, size_t start_slot, const unsigned char *key, const unsigned char *nonce) {
    struct Scatter walk;
    if (scatter_init(&walk, carrier->slots - start_slot, key, nonce) < 0)
        return -1;

    size_t data_bits = data_len * 8;
    for (size_t i = 0; i < data_bits; i++)
        carrier->write(carrier, start_slot + scatter_next(&walk), data_bit(data, i));

    scatter_clear(&walk);

    return 0;
}

static int extract_scattered(const Carrier *carrier, unsigned char *out, size_t out_len, size_t start_slot, const unsigned char *key, const unsigned char *nonce) {
    struct Scatter walk;
    if (scatter_init(&walk, carrier->slots - start_slot, key, nonce) < 0)
        return -1;

    size_t out_bits = out_len * 8;
    memset(out, 0, out_len);
    for (size_t i = 0; i < out_bits; i++)
        data_bit_set(out, i, carrier->read(carrier, start_slot + scatter_next(&walk)));

    scatter_clear(&walk);

    return 0;
}

/*
 * One pwhash call produces both keys: the first half seals the header and the
 * payload, the second half drives the scatter walk.
 */
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

static size_t header_pack_preamble(const struct ContainerHeader *header, unsigned char *out) {
    size_t off = 0;

    memcpy(out + off, header->magic, HUSH_MAGIC_LEN);
    off += HUSH_MAGIC_LEN;

    out[off++] = header->version;
    out[off++] = header->flags;

    if (!(header->flags & HUSH_FLAG_ENCRYPTED))
        return off;

    memcpy(out + off, header->salt, sizeof(header->salt));
    off += sizeof(header->salt);

    memcpy(out + off, header->header_nonce, sizeof(header->header_nonce));
    off += sizeof(header->header_nonce);

    return off;
}

static void header_pack_body(const struct ContainerHeader *header, unsigned char *out) {
    memset(out, 0, HUSH_HEADER_BODY_LEN);

    out[HUSH_BODY_CODEC_OFF] = header->codec;

    for (size_t i = 0; i < 4; i++)
        out[HUSH_BODY_LEN_OFF + i] = (unsigned char)((header->payload_len >> (i * 8)) & 0xFF);

    memcpy(out + HUSH_BODY_NONCE_OFF, header->payload_nonce, sizeof(header->payload_nonce));
}

/*
 * Reads back the cleartext part every container starts with. The salt and the
 * header nonce that follow it on an encrypted container are only pulled once the
 * flags say they are there, see header_unpack_keys.
 */
static int header_unpack_preamble(struct ContainerHeader *header, const unsigned char *in) {
    memcpy(header->magic, in, HUSH_MAGIC_LEN);
    header->version = in[HUSH_MAGIC_LEN];
    header->flags = in[HUSH_MAGIC_LEN + 1];

    if (memcmp(header->magic, HUSH_MAGIC, HUSH_MAGIC_LEN) != 0) {
        ERROR("No %s container was found in the image", HUSH_MAGIC);
        return -1;
    }

    if (header->version != HUSH_VERSION) {
        ERROR("Container version %u is not supported, this build speaks version %u", header->version, HUSH_VERSION);
        return -1;
    }

    return 0;
}

static void header_unpack_keys(struct ContainerHeader *header, const unsigned char *in) {
    size_t off = HUSH_PREAMBLE_PLAIN_LEN;

    memcpy(header->salt, in + off, sizeof(header->salt));
    off += sizeof(header->salt);

    memcpy(header->header_nonce, in + off, sizeof(header->header_nonce));
}

static void header_unpack_body(struct ContainerHeader *header, const unsigned char *in) {
    header->codec = in[HUSH_BODY_CODEC_OFF];

    header->payload_len = 0;
    for (size_t i = 0; i < 4; i++)
        header->payload_len |= (uint32_t)in[HUSH_BODY_LEN_OFF + i] << (i * 8);

    memcpy(header->payload_nonce, in + HUSH_BODY_NONCE_OFF, sizeof(header->payload_nonce));
}

static int embed_plain(const Carrier *carrier, enum CodecType codec, const unsigned char *data, size_t data_len) {
    size_t needed = HUSH_PREAMBLE_PLAIN_LEN + data_len + HUSH_END_MARKER_LEN;
    if (carrier->slots / 8 < needed) {
        ERROR("Image size is not big enough to embed the targeted data into it");
        return -1;
    }

    struct ContainerHeader header = {.version = HUSH_VERSION, .flags = 0, .codec = (unsigned char)codec};
    memcpy(header.magic, HUSH_MAGIC, HUSH_MAGIC_LEN);

    unsigned char preamble[HUSH_PREAMBLE_PLAIN_LEN];
    size_t preamble_len = header_pack_preamble(&header, preamble);

    DEBUG("Embedding %zu bytes sequentially, terminated by %s", data_len, HUSH_END_MARKER);
    embed_sequential(carrier, preamble, preamble_len, 0);
    embed_sequential(carrier, data, data_len, preamble_len * 8);
    embed_sequential(carrier, (const unsigned char *)HUSH_END_MARKER, HUSH_END_MARKER_LEN, (preamble_len + data_len) * 8);

    return 0;
}

static int embed_encrypted(const Carrier *carrier, enum CodecType codec, const char *passphrase, const unsigned char *data, size_t data_len) {
    size_t payload_len = data_len + crypto_secretbox_MACBYTES;
    size_t needed = HUSH_PREAMBLE_ENC_LEN + HUSH_HEADER_SEALED_LEN + payload_len;

    if (carrier->slots / 8 < needed) {
        ERROR("Image size is not big enough to embed the encrypted data into it");
        return -1;
    }

    if (payload_len > UINT32_MAX) {
        ERROR("Payload is too large to be described by the header");
        return -1;
    }

    struct ContainerHeader header = {
        .version = HUSH_VERSION,
        .flags = HUSH_FLAG_ENCRYPTED,
        .codec = (unsigned char)codec,
        .payload_len = (uint32_t)payload_len,
    };

    memcpy(header.magic, HUSH_MAGIC, HUSH_MAGIC_LEN);
    randombytes_buf(header.salt, sizeof(header.salt));
    randombytes_buf(header.header_nonce, sizeof(header.header_nonce));
    randombytes_buf(header.payload_nonce, sizeof(header.payload_nonce));

    unsigned char box_key[crypto_secretbox_KEYBYTES];
    unsigned char prng_key[PRNG_KEYBYTES];
    if (derive_keys(passphrase, header.salt, box_key, prng_key) < 0)
        return -1;

    unsigned char body[HUSH_HEADER_BODY_LEN];
    unsigned char sealed_header[HUSH_HEADER_SEALED_LEN];

    header_pack_body(&header, body);
    crypto_secretbox_easy(sealed_header, body, sizeof(body), header.header_nonce, box_key);
    sodium_memzero(body, sizeof(body));

    struct Payload *payload = encrypt_data(data, data_len, header.payload_nonce, box_key);

    if (!payload) {
        ERROR("Failed to encrypt data");
        sodium_memzero(box_key, sizeof(box_key));
        sodium_memzero(prng_key, sizeof(prng_key));
        return -1;
    }
    sodium_memzero(box_key, sizeof(box_key));

    unsigned char preamble[HUSH_PREAMBLE_ENC_LEN];
    size_t preamble_len = header_pack_preamble(&header, preamble);
    size_t payload_start = (preamble_len + HUSH_HEADER_SEALED_LEN) * 8;

    DEBUG("Embedding a %zu byte encrypted header at the start of the image", preamble_len + sizeof(sealed_header));
    embed_sequential(carrier, preamble, preamble_len, 0);
    embed_sequential(carrier, sealed_header, sizeof(sealed_header), preamble_len * 8);

    DEBUG("Scattering %zu encrypted bytes over %zu slots", payload_len, carrier->slots - payload_start);
    int status = embed_scattered(carrier, payload->body, payload->body_len, payload_start, prng_key, header.payload_nonce);

    sodium_memzero(prng_key, sizeof(prng_key));
    payload_free(payload);

    return status;
}

static int extract_plain(const Carrier *carrier, unsigned char **out, size_t *out_len) {
    size_t start_slot = HUSH_PREAMBLE_PLAIN_LEN * 8;
    size_t max_len = carrier->slots / 8 - HUSH_PREAMBLE_PLAIN_LEN;

    if (max_len <= HUSH_END_MARKER_LEN) {
        ERROR("Image is too small to hold a terminated payload");
        return -1;
    }

    unsigned char *data = malloc(max_len);
    if (!data) {
        ERROR("Failed to allocate the payload buffer");
        return -1;
    }

    DEBUG("Scanning up to %zu sequential bytes for %s", max_len, HUSH_END_MARKER);
    for (size_t len = 1; len <= max_len; len++) {
        extract_sequential(carrier, data + len - 1, 1, start_slot + (len - 1) * 8);
        if (len <= HUSH_END_MARKER_LEN)
            continue;

        if (memcmp(data + len - HUSH_END_MARKER_LEN, HUSH_END_MARKER, HUSH_END_MARKER_LEN) != 0)
            continue;

        size_t data_len = len - HUSH_END_MARKER_LEN;
        unsigned char *shrunk = realloc(data, data_len);

        *out = shrunk ? shrunk : data;
        *out_len = data_len;

        return 0;
    }

    ERROR("Reached the end of the image without finding the %s marker", HUSH_END_MARKER);
    free(data);

    return -1;
}

static int extract_encrypted(const Carrier *carrier, const char *passphrase, struct ContainerHeader *header, unsigned char **out, size_t *out_len) {
    if (!passphrase) {
        ERROR("The container is encrypted, a passphrase is required to open it");
        return -1;
    }

    size_t header_start = HUSH_PREAMBLE_ENC_LEN * 8;
    size_t payload_start = header_start + HUSH_HEADER_SEALED_LEN * 8;

    if (carrier->slots <= payload_start) {
        ERROR("Image is too small to hold the encrypted header");
        return -1;
    }

    unsigned char box_key[crypto_secretbox_KEYBYTES];
    unsigned char prng_key[PRNG_KEYBYTES];
    if (derive_keys(passphrase, header->salt, box_key, prng_key) < 0)
        return -1;

    unsigned char sealed_header[HUSH_HEADER_SEALED_LEN];
    unsigned char body[HUSH_HEADER_BODY_LEN];

    size_t payload_len = 0;
    unsigned char *payload = NULL;
    unsigned char *plain = NULL;
    int status = -1;

    extract_sequential(carrier, sealed_header, sizeof(sealed_header), header_start);
    if (crypto_secretbox_open_easy(body, sealed_header, sizeof(sealed_header), header->header_nonce, box_key) != 0) {
        ERROR("Failed to open the header, the passphrase is wrong or the image was altered");
        goto cleanup;
    }

    header_unpack_body(header, body);
    sodium_memzero(body, sizeof(body));

    payload_len = header->payload_len;
    if (payload_len <= crypto_secretbox_MACBYTES || payload_len * 8 > carrier->slots - payload_start) {
        ERROR("The header describes a %zu byte payload, which does not fit the image", payload_len);
        goto cleanup;
    }

    payload = malloc(payload_len);
    plain = malloc(payload_len - crypto_secretbox_MACBYTES);
    if (!payload || !plain) {
        ERROR("Failed to allocate the payload buffer");
        goto cleanup;
    }

    DEBUG("Gathering %zu encrypted bytes from %zu slots", payload_len, carrier->slots - payload_start);
    if (extract_scattered(carrier, payload, payload_len, payload_start, prng_key, header->payload_nonce) < 0)
        goto cleanup;

    if (crypto_secretbox_open_easy(plain, payload, payload_len, header->payload_nonce, box_key) != 0) {
        ERROR("Failed to open the payload, the image was altered");
        goto cleanup;
    }

    *out = plain;
    *out_len = payload_len - crypto_secretbox_MACBYTES;
    plain = NULL;
    status = 0;

cleanup:
    sodium_memzero(box_key, sizeof(box_key));
    sodium_memzero(prng_key, sizeof(prng_key));

    if (payload) {
        sodium_memzero(payload, payload_len);
        free(payload);
    }

    free(plain);

    return status;
}

int container_embed(const Carrier *carrier, enum CodecType codec, const char *passphrase, const unsigned char *data, size_t data_len) {
    int encrypted = passphrase && passphrase[0] != '\0';

    INFO("Capacity: %zu bytes", carrier->slots / 8);
    INFO("Data to encode: %zu bytes", data_len);
    INFO("Encryption: %s", encrypted ? "on" : "off");

    if (encrypted)
        return embed_encrypted(carrier, codec, passphrase, data, data_len);

    return embed_plain(carrier, codec, data, data_len);
}

int container_extract(const Carrier *carrier, const char *passphrase, enum CodecType *codec, unsigned char **data, size_t *data_len) {
    struct ContainerHeader header = {0};
    unsigned char preamble[HUSH_PREAMBLE_ENC_LEN];

    if (carrier->slots / 8 < HUSH_PREAMBLE_PLAIN_LEN) {
        ERROR("Image is too small to hold a %s container", HUSH_MAGIC);
        return -1;
    }

    extract_sequential(carrier, preamble, HUSH_PREAMBLE_PLAIN_LEN, 0);
    if (header_unpack_preamble(&header, preamble) < 0)
        return -1;

    int encrypted = (header.flags & HUSH_FLAG_ENCRYPTED) != 0;
    INFO("Container: version %u, encryption %s", header.version, encrypted ? "on" : "off");

    if (!encrypted)
        return extract_plain(carrier, data, data_len);

    if (carrier->slots / 8 < HUSH_PREAMBLE_ENC_LEN) {
        ERROR("Image is too small to hold the encrypted preamble");
        return -1;
    }

    extract_sequential(carrier, preamble + HUSH_PREAMBLE_PLAIN_LEN, HUSH_PREAMBLE_ENC_LEN - HUSH_PREAMBLE_PLAIN_LEN, HUSH_PREAMBLE_PLAIN_LEN * 8);
    header_unpack_keys(&header, preamble);

    if (extract_encrypted(carrier, passphrase, &header, data, data_len) < 0)
        return -1;

    *codec = (enum CodecType)header.codec;

    return 0;
}
