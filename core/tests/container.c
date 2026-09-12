#include "greatest.h"

#include <stdlib.h>
#include <string.h>

#include "veil/codecs/carrier.h"
#include "veil/codecs/image/container.h"

#define SLOTS 65536
#define PAYLOAD_LEN 96

struct BitCarrier {
    unsigned char *bits;
};

static unsigned char bit_read(const Carrier *carrier, size_t slot) {
    const struct BitCarrier *bits = carrier->ctx;

    return bits->bits[slot] & 1;
}

static void bit_write(const Carrier *carrier, size_t slot, unsigned char bit) {
    struct BitCarrier *bits = carrier->ctx;

    bits->bits[slot] = bit & 1;
}

static int carrier_open(struct BitCarrier *bits, Carrier *carrier, size_t slots) {
    bits->bits = calloc(slots, 1);
    if (!bits->bits)
        return -1;

    carrier->ctx = bits;
    carrier->slots = slots;
    carrier->read = bit_read;
    carrier->write = bit_write;

    return 0;
}

static void fill_payload(unsigned char *payload) {
    for (size_t i = 0; i < PAYLOAD_LEN; i++)
        payload[i] = (unsigned char)(i * 13 + 7);
}

static enum greatest_test_res round_trip(const char *passphrase) {
    struct BitCarrier bits;
    Carrier carrier;
    ASSERT_EQ(0, carrier_open(&bits, &carrier, SLOTS));

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, container_embed(&carrier, CODEC_LSB_REPLACEMENT, passphrase, payload, PAYLOAD_LEN));

    enum CodecType codec = CODEC_UNKNOWN;
    unsigned char *out = NULL;
    size_t out_len = 0;

    ASSERT_EQ(0, container_extract(&carrier, passphrase, &codec, &out, &out_len));
    ASSERT_EQ((size_t)PAYLOAD_LEN, out_len);
    ASSERT_EQ(0, memcmp(out, payload, PAYLOAD_LEN));
    ASSERT_EQ(passphrase ? CODEC_LSB_REPLACEMENT : CODEC_UNKNOWN, codec);

    free(out);
    free(bits.bits);
    PASS();
}

TEST container_round_trips_in_the_clear(void) {
    CHECK_CALL(round_trip(NULL));
    PASS();
}

TEST container_round_trips_encrypted(void) {
    CHECK_CALL(round_trip("correct horse battery staple"));
    PASS();
}

TEST container_leaves_its_magic_in_the_clear(void) {
    struct BitCarrier bits;
    Carrier carrier;
    ASSERT_EQ(0, carrier_open(&bits, &carrier, SLOTS));

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, container_embed(&carrier, CODEC_DCT, "a passphrase", payload, PAYLOAD_LEN));

    for (size_t i = 0; i < HUSH_MAGIC_LEN * 8; i++) {
        const unsigned char want = (unsigned char)((HUSH_MAGIC[i / 8] >> (i % 8)) & 1);

        ASSERT_EQ(want, carrier.read(&carrier, i));
    }

    free(bits.bits);
    PASS();
}

TEST container_refuses_the_wrong_passphrase(void) {
    struct BitCarrier bits;
    Carrier carrier;
    ASSERT_EQ(0, carrier_open(&bits, &carrier, SLOTS));

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, container_embed(&carrier, CODEC_LSB_MATCHING, "the right one", payload, PAYLOAD_LEN));

    enum CodecType codec = CODEC_UNKNOWN;
    unsigned char *out = NULL;
    size_t out_len = 0;

    ASSERT(container_extract(&carrier, "the wrong one", &codec, &out, &out_len) < 0);
    ASSERT(out == NULL);

    free(bits.bits);
    PASS();
}

TEST container_finds_nothing_in_an_untouched_carrier(void) {
    struct BitCarrier bits;
    Carrier carrier;
    ASSERT_EQ(0, carrier_open(&bits, &carrier, SLOTS));

    enum CodecType codec = CODEC_UNKNOWN;
    unsigned char *out = NULL;
    size_t out_len = 0;

    ASSERT(container_extract(&carrier, NULL, &codec, &out, &out_len) < 0);
    ASSERT(out == NULL);

    free(bits.bits);
    PASS();
}

TEST container_will_not_overfill_a_carrier(void) {
    struct BitCarrier bits;
    Carrier carrier;
    ASSERT_EQ(0, carrier_open(&bits, &carrier, 256));

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT(container_embed(&carrier, CODEC_LSB_REPLACEMENT, NULL, payload, PAYLOAD_LEN) < 0);

    free(bits.bits);
    PASS();
}

TEST container_scatters_only_when_encrypted(void) {
    struct BitCarrier plain;
    struct BitCarrier sealed;
    Carrier plain_carrier;
    Carrier sealed_carrier;

    ASSERT_EQ(0, carrier_open(&plain, &plain_carrier, SLOTS));
    ASSERT_EQ(0, carrier_open(&sealed, &sealed_carrier, SLOTS));

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, container_embed(&plain_carrier, CODEC_LSB_REPLACEMENT, NULL, payload, PAYLOAD_LEN));
    ASSERT_EQ(0, container_embed(&sealed_carrier, CODEC_LSB_REPLACEMENT, "a passphrase", payload, PAYLOAD_LEN));

    size_t plain_used = 0;
    size_t sealed_used = 0;

    for (size_t slot = SLOTS / 2; slot < SLOTS; slot++) {
        plain_used += plain.bits[slot] != 0;
        sealed_used += sealed.bits[slot] != 0;
    }

    ASSERT_EQ((size_t)0, plain_used);
    ASSERT(sealed_used > 0);

    free(plain.bits);
    free(sealed.bits);
    PASS();
}

SUITE(container_suite) {
    RUN_TEST(container_round_trips_in_the_clear);
    RUN_TEST(container_round_trips_encrypted);
    RUN_TEST(container_leaves_its_magic_in_the_clear);
    RUN_TEST(container_refuses_the_wrong_passphrase);
    RUN_TEST(container_finds_nothing_in_an_untouched_carrier);
    RUN_TEST(container_will_not_overfill_a_carrier);
    RUN_TEST(container_scatters_only_when_encrypted);
}
