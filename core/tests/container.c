#include "greatest.h"

#include <stdint.h>

#include "helpers.h"
#include "veil/codecs/container.h"
#include "veil/fs/checksum.h"

#define PAYLOAD_LEN 96

static void fill_payload(unsigned char *payload, size_t len) {
    for (size_t i = 0; i < len; i++)
        payload[i] = (unsigned char)(i * 13 + 7);
}

static struct Container *open_container(const char *target, const char *passphrase) {
    char copy[PASSPHRASE_MAX] = {0};
    if (passphrase)
        strncpy(copy, passphrase, sizeof(copy) - 1);

    return container_init(target, copy);
}

static enum greatest_test_res embed(const char *target, const char *output, const char *passphrase, const unsigned char *payload, size_t len) {
    struct Container *container = open_container(target, passphrase);
    ASSERT(container);

    container->header.payload_len = len;
    container->header.checksum = calculate_checksum(payload, len);
    ASSERT_EQ(0, container_reserve_header(container));
    ASSERT_EQ(0, container_encode_chunk(container, payload, len));
    ASSERT_EQ(0, container_write_header(container));
    ASSERT_EQ(0, container->carrier->save(container->carrier, output));

    container_free(container);
    PASS();
}

static enum greatest_test_res extract(const char *target, const char *passphrase, const unsigned char *want, size_t len) {
    struct Container *container = open_container(target, passphrase);
    ASSERT(container);
    ASSERT_EQ(0, container_read_header(container));
    ASSERT_EQ(len, container->header.payload_len);
    ASSERT_EQ(calculate_checksum(want, len), container->header.checksum);

    unsigned char *got = NULL;
    ASSERT_EQ(0, container_decode_chunk(container, &got, len));
    ASSERT_MEM_EQ(want, got, len);

    free(got);
    container_free(container);
    PASS();
}

static int header_fails_to_read(const char *target, const char *passphrase) {
    struct Container *container = open_container(target, passphrase);
    if (!container)
        return 1;

    const int failed = container_read_header(container) < 0;
    container_free(container);
    return failed;
}

// The same order analysis/stream.c rebuilds bytes in, least significant bit first.
static unsigned char read_byte(Carrier *carrier, size_t index) {
    unsigned char byte = 0;
    for (size_t bit = 0; bit < 8; bit++)
        byte |= (unsigned char)(carrier->read(carrier, index * 8 + bit) << bit);

    return byte;
}

static enum greatest_test_res round_trip(const char *passphrase) {
    char *image = create_test_png(64, 64, 3);
    char *output = create_temp_path();
    ASSERT(image && output);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));

    CHECK_CALL(embed(image, output, passphrase, payload, sizeof(payload)));
    CHECK_CALL(extract(output, passphrase, payload, sizeof(payload)));

    unlink(image);
    unlink(output);
    free(image);
    free(output);
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

TEST container_round_trips_through_a_jpeg(void) {
    char *image = create_test_jpg(96, 96, 90);
    char *output = create_temp_path();
    ASSERT(image && output);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));

    CHECK_CALL(embed(image, output, "a passphrase", payload, sizeof(payload)));
    CHECK_CALL(extract(output, "a passphrase", payload, sizeof(payload)));

    unlink(image);
    unlink(output);
    free(image);
    free(output);
    PASS();
}

TEST container_lays_out_a_clear_header_in_order(void) {
    char *image = create_test_png(32, 32, 3);
    char *output = create_temp_path();
    ASSERT(image && output);

    const unsigned char payload[] = "in the clear";
    CHECK_CALL(embed(image, output, NULL, payload, sizeof(payload)));

    Carrier *carrier = figure_carrier(output);
    ASSERT(carrier);

    for (size_t i = 0; i < VEIL_MAGIC_LEN; i++)
        ASSERT_EQ((unsigned char)VEIL_MAGIC[i], read_byte(carrier, i));

    ASSERT_EQ(VEIL_CONTAINER_VERSION, read_byte(carrier, VEIL_MAGIC_LEN));
    ASSERT_EQ(0, read_byte(carrier, VEIL_MAGIC_LEN + 1));

    uint64_t len = 0;
    for (size_t i = 0; i < PAYLOAD_LEN_BYTES; i++)
        len |= (uint64_t)read_byte(carrier, CONTAINER_PREAMBLE_BYTES + i) << (i * 8);

    ASSERT_EQ((uint64_t)sizeof(payload), len);
    ASSERT_EQ(calculate_checksum(payload, sizeof(payload)), read_byte(carrier, CONTAINER_PREAMBLE_BYTES + PAYLOAD_LEN_BYTES));

    for (size_t i = 0; i < sizeof(payload); i++)
        ASSERT_EQ(payload[i], read_byte(carrier, CONTAINER_PREAMBLE_BYTES + CONTAINER_CLEAR_BYTES + i));

    carrier->free(carrier);
    unlink(image);
    unlink(output);
    free(image);
    free(output);
    PASS();
}

TEST container_leaves_its_preamble_readable_when_encrypted(void) {
    char *image = create_test_png(64, 64, 3);
    char *output = create_temp_path();
    ASSERT(image && output);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));
    CHECK_CALL(embed(image, output, "a passphrase", payload, sizeof(payload)));

    Carrier *carrier = figure_carrier(output);
    ASSERT(carrier);

    for (size_t i = 0; i < VEIL_MAGIC_LEN; i++)
        ASSERT_EQ((unsigned char)VEIL_MAGIC[i], read_byte(carrier, i));

    ASSERT_EQ(VEIL_CONTAINER_VERSION, read_byte(carrier, VEIL_MAGIC_LEN));
    ASSERT_EQ(VEIL_FLAG_ENCRYPTED, read_byte(carrier, VEIL_MAGIC_LEN + 1));

    carrier->free(carrier);
    unlink(image);
    unlink(output);
    free(image);
    free(output);
    PASS();
}

TEST container_refuses_the_wrong_or_a_missing_passphrase(void) {
    char *image = create_test_png(64, 64, 3);
    char *output = create_temp_path();
    ASSERT(image && output);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));
    CHECK_CALL(embed(image, output, "the right one", payload, sizeof(payload)));

    ASSERT(header_fails_to_read(output, "the wrong one"));
    ASSERT(header_fails_to_read(output, NULL));

    unlink(image);
    unlink(output);
    free(image);
    free(output);
    PASS();
}

TEST container_finds_nothing_in_a_plain_carrier(void) {
    char *image = create_test_png(64, 64, 3);
    ASSERT(image);

    ASSERT(header_fails_to_read(image, NULL));

    unlink(image);
    free(image);
    PASS();
}

TEST container_detects_a_tampered_encrypted_header(void) {
    char *image = create_test_png(64, 64, 3);
    char *output = create_temp_path();
    char *tampered = create_temp_path();
    ASSERT(image && output && tampered);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));
    CHECK_CALL(embed(image, output, "a passphrase", payload, sizeof(payload)));

    struct Container *container = open_container(output, "a passphrase");
    ASSERT(container);
    ASSERT_EQ(0, container_read_header(container));

    Carrier *carrier = container->carrier;
    const size_t slot = container->header_slots[0];
    ASSERT_EQ(0, carrier->write(carrier, slot, (unsigned char)!carrier->read(carrier, slot)));
    ASSERT_EQ(0, carrier->save(carrier, tampered));
    container_free(container);

    ASSERT(header_fails_to_read(tampered, "a passphrase"));

    unlink(image);
    unlink(output);
    unlink(tampered);
    free(image);
    free(output);
    free(tampered);
    PASS();
}

static enum greatest_test_res read_scattered(struct Container *container, unsigned char *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char byte = 0;
        for (size_t bit = 0; bit < 8; bit++) {
            const size_t slot = scatter_next(&container->scatter);
            ASSERT(slot != SIZE_MAX);
            byte |= (unsigned char)(container->carrier->read(container->carrier, slot + container->preamble_bits) << bit);
        }

        bytes[i] = byte;
    }

    PASS();
}

TEST container_does_not_scatter_the_plaintext(void) {
    char *image = create_test_png(64, 64, 3);
    char *output = create_temp_path();
    ASSERT(image && output);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));
    CHECK_CALL(embed(image, output, "a passphrase", payload, sizeof(payload)));

    struct Container *container = open_container(output, "a passphrase");
    ASSERT(container);
    ASSERT_EQ(0, container_read_header(container));

    // The key and scatter are right, so all that stands between these bits and the payload is the encryption.
    unsigned char scattered[PAYLOAD_LEN];
    CHECK_CALL(read_scattered(container, scattered, sizeof(scattered)));
    ASSERT(memcmp(payload, scattered, sizeof(payload)) != 0);

    container_free(container);
    unlink(image);
    unlink(output);
    free(image);
    free(output);
    PASS();
}

TEST container_detects_a_tampered_encrypted_payload(void) {
    char *image = create_test_png(64, 64, 3);
    char *output = create_temp_path();
    char *tampered = create_temp_path();
    ASSERT(image && output && tampered);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));
    CHECK_CALL(embed(image, output, "a passphrase", payload, sizeof(payload)));

    struct Container *container = open_container(output, "a passphrase");
    ASSERT(container);
    ASSERT_EQ(0, container_read_header(container));

    Carrier *carrier = container->carrier;
    const size_t slot = scatter_next(&container->scatter) + container->preamble_bits;
    ASSERT_EQ(0, carrier->write(carrier, slot, (unsigned char)!carrier->read(carrier, slot)));
    ASSERT_EQ(0, carrier->save(carrier, tampered));
    container_free(container);

    container = open_container(tampered, "a passphrase");
    ASSERT(container);
    ASSERT_EQ(0, container_read_header(container));

    unsigned char *got = NULL;
    ASSERT(container_decode_chunk(container, &got, sizeof(payload)) < 0);
    ASSERT_EQ(NULL, got);

    container_free(container);
    unlink(image);
    unlink(output);
    unlink(tampered);
    free(image);
    free(output);
    free(tampered);
    PASS();
}

TEST container_refuses_a_header_length_that_disagrees_with_the_payload(void) {
    char *image = create_test_png(64, 64, 3);
    ASSERT(image);

    struct Container *container = open_container(image, "a passphrase");
    ASSERT(container);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload, sizeof(payload));
    container->header.payload_len = sizeof(payload) + 1;
    ASSERT_EQ(0, container_reserve_header(container));
    ASSERT_EQ(0, container_encode_chunk(container, payload, sizeof(payload)));
    ASSERT(container_write_header(container) < 0);

    container_free(container);
    unlink(image);
    free(image);
    PASS();
}

TEST container_rejects_a_length_the_carrier_cannot_hold(void) {
    char *image = create_test_png(16, 16, 3);
    char *output = create_temp_path();
    ASSERT(image && output);

    struct Container *container = open_container(image, NULL);
    ASSERT(container);

    container->header.payload_len = container->carrier->capacity(container->carrier);
    ASSERT_EQ(0, container_reserve_header(container));
    ASSERT_EQ(0, container_write_header(container));
    ASSERT_EQ(0, container->carrier->save(container->carrier, output));
    container_free(container);

    ASSERT(header_fails_to_read(output, NULL));

    unlink(image);
    unlink(output);
    free(image);
    free(output);
    PASS();
}

TEST container_will_not_overfill_a_carrier(void) {
    char *image = create_test_png(8, 8, 3);
    ASSERT(image);

    struct Container *container = open_container(image, NULL);
    ASSERT(container);
    ASSERT_EQ(0, container_reserve_header(container));

    const size_t fits = (container->carrier->capacity(container->carrier) - container->preamble_bits - container->header_bits) / 8;
    unsigned char payload[64] = {0};
    ASSERT(fits < sizeof(payload));

    ASSERT(container_encode_chunk(container, payload, sizeof(payload)) < 0);

    container_free(container);
    unlink(image);
    free(image);
    PASS();
}

TEST container_calls_come_in_order(void) {
    char *image = create_test_png(16, 16, 3);
    ASSERT(image);

    struct Container *container = open_container(image, NULL);
    ASSERT(container);

    unsigned char *chunk = NULL;
    ASSERT(container_write_header(container) < 0);
    ASSERT(container_decode_chunk(container, &chunk, 1) < 0);

    ASSERT_EQ(0, container_reserve_header(container));
    ASSERT(container_reserve_header(container) < 0);
    ASSERT(container_read_header(container) < 0);

    ASSERT(container_reserve_header(NULL) < 0);
    ASSERT(container_write_header(NULL) < 0);
    ASSERT(container_read_header(NULL) < 0);
    ASSERT(container_encode_chunk(NULL, chunk, 0) < 0);

    container_free(container);
    container_free(NULL);
    unlink(image);
    free(image);
    PASS();
}

TEST container_wants_a_carrier_it_can_read(void) {
    ASSERT_EQ(NULL, open_container("/tmp/no_such_carrier_for_veil_tests.png", NULL));
    ASSERT_EQ(NULL, open_container(NULL, NULL));
    PASS();
}

SUITE(container_suite) {
    RUN_TEST(container_round_trips_in_the_clear);
    RUN_TEST(container_round_trips_encrypted);
    RUN_TEST(container_round_trips_through_a_jpeg);
    RUN_TEST(container_lays_out_a_clear_header_in_order);
    RUN_TEST(container_leaves_its_preamble_readable_when_encrypted);
    RUN_TEST(container_refuses_the_wrong_or_a_missing_passphrase);
    RUN_TEST(container_finds_nothing_in_a_plain_carrier);
    RUN_TEST(container_detects_a_tampered_encrypted_header);
    RUN_TEST(container_does_not_scatter_the_plaintext);
    RUN_TEST(container_detects_a_tampered_encrypted_payload);
    RUN_TEST(container_refuses_a_header_length_that_disagrees_with_the_payload);
    RUN_TEST(container_rejects_a_length_the_carrier_cannot_hold);
    RUN_TEST(container_will_not_overfill_a_carrier);
    RUN_TEST(container_calls_come_in_order);
    RUN_TEST(container_wants_a_carrier_it_can_read);
}
