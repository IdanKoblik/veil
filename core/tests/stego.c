#include "greatest.h"

#include <sys/stat.h>

#include "helpers.h"
#include "veil/decode.h"
#include "veil/encode.h"
#include "veil/fs/file.h"

static unsigned char *make_payload(size_t len) {
    unsigned char *payload = malloc(len ? len : 1);
    if (!payload)
        return NULL;

    for (size_t i = 0; i < len; i++)
        payload[i] = (unsigned char)((i * 31) ^ (i >> 8));

    return payload;
}

static enum greatest_test_res file_holds(const char *path, const unsigned char *want, size_t len) {
    unsigned char *got = NULL;
    size_t got_len = 0;

    ASSERT_EQ(0, read_file_raw_data(path, &got, &got_len));
    ASSERT_EQ(len, got_len);
    if (len)
        ASSERT_MEM_EQ(want, got, len);

    free(got);
    PASS();
}

static enum greatest_test_res round_trip(char *image, size_t len, const char *passphrase) {
    unsigned char *payload = make_payload(len);
    char *data = create_temp_file(payload, len);
    char *stego = create_temp_path();
    char *back = create_temp_path();
    ASSERT(image && payload && data && stego && back);

    char secret[PASSPHRASE_MAX] = {0};
    if (passphrase)
        strncpy(secret, passphrase, sizeof(secret) - 1);

    unsigned char encoded[PAYLOAD_DIGEST_BYTES];
    unsigned char decoded[PAYLOAD_DIGEST_BYTES];
    unsigned char want[PAYLOAD_DIGEST_BYTES];
    crypto_generichash(want, sizeof(want), payload, len, NULL, 0);

    size_t encoded_len = 0;
    ASSERT_EQ(0, encode(image, data, stego, secret, encoded, &encoded_len));
    ASSERT_EQ(len, encoded_len);
    ASSERT_MEM_EQ(want, encoded, sizeof(want));

    size_t decoded_len = 0;
    ASSERT_EQ(0, decode(stego, back, secret, &decoded_len));
    ASSERT_EQ(len, decoded_len);
    CHECK_CALL(file_holds(back, payload, len));

    ASSERT_EQ(0, decode_digest(stego, secret, decoded));
    ASSERT_MEM_EQ(want, decoded, sizeof(want));

    unlink(image);
    unlink(data);
    unlink(stego);
    unlink(back);
    free(image);
    free(data);
    free(stego);
    free(back);
    free(payload);
    PASS();
}

TEST stego_round_trips_a_png_in_the_clear(void) {
    CHECK_CALL(round_trip(create_test_png(64, 64, 3), 200, NULL));
    PASS();
}

TEST stego_round_trips_a_png_encrypted(void) {
    CHECK_CALL(round_trip(create_test_png(64, 64, 3), 200, "correct horse battery staple"));
    PASS();
}

TEST stego_round_trips_a_jpeg(void) {
    CHECK_CALL(round_trip(create_test_jpg(128, 128, 90), 64, "a passphrase"));
    PASS();
}

TEST stego_round_trips_across_several_chunks(void) {
    // Past the 64KB chunk size, so decoding has to stitch chunks back together.
    CHECK_CALL(round_trip(create_test_png(512, 512, 3), 90 * 1024, NULL));
    PASS();
}

TEST stego_round_trips_an_empty_payload(void) {
    CHECK_CALL(round_trip(create_test_png(16, 16, 3), 0, NULL));
    PASS();
}

TEST encode_writes_nothing_when_the_payload_does_not_fit(void) {
    char *image = create_test_png(8, 8, 3);
    unsigned char *payload = make_payload(1024);
    char *data = create_temp_file(payload, 1024);
    char *stego = create_temp_path();
    ASSERT(image && payload && data && stego);
    unlink(stego);

    char secret[PASSPHRASE_MAX] = {0};
    ASSERT(encode(image, data, stego, secret, NULL, NULL) < 0);
    ASSERT(access(stego, F_OK) != 0);

    unlink(image);
    unlink(data);
    free(image);
    free(data);
    free(stego);
    free(payload);
    PASS();
}

TEST encode_wants_data_it_can_read(void) {
    char *image = create_test_png(16, 16, 3);
    char *stego = create_temp_path();
    ASSERT(image && stego);

    char secret[PASSPHRASE_MAX] = {0};
    ASSERT(encode(image, "/tmp/no_such_payload_for_veil_tests", stego, secret, NULL, NULL) < 0);
    ASSERT(encode(NULL, image, stego, secret, NULL, NULL) < 0);
    ASSERT(encode(image, NULL, stego, secret, NULL, NULL) < 0);
    ASSERT(encode(image, image, NULL, secret, NULL, NULL) < 0);

    unlink(image);
    unlink(stego);
    free(image);
    free(stego);
    PASS();
}

TEST decode_leaves_the_output_alone_on_a_wrong_passphrase(void) {
    char *image = create_test_png(64, 64, 3);
    const unsigned char payload[] = "hidden";
    const unsigned char existing[] = "already here";
    char *data = create_temp_file(payload, sizeof(payload));
    char *stego = create_temp_path();
    char *back = create_temp_file(existing, sizeof(existing));
    ASSERT(image && data && stego && back);

    char right[PASSPHRASE_MAX] = "the right one";
    char wrong[PASSPHRASE_MAX] = "the wrong one";
    unsigned char digest[PAYLOAD_DIGEST_BYTES];

    ASSERT_EQ(0, encode(image, data, stego, right, NULL, NULL));
    ASSERT(decode(stego, back, wrong, NULL) < 0);
    ASSERT(decode_digest(stego, wrong, digest) < 0);
    CHECK_CALL(file_holds(back, existing, sizeof(existing)));

    unlink(image);
    unlink(data);
    unlink(stego);
    unlink(back);
    free(image);
    free(data);
    free(stego);
    free(back);
    PASS();
}

TEST decode_finds_nothing_in_a_plain_image(void) {
    char *image = create_test_png(32, 32, 3);
    char *back = create_temp_path();
    ASSERT(image && back);

    char secret[PASSPHRASE_MAX] = {0};
    unsigned char digest[PAYLOAD_DIGEST_BYTES];
    ASSERT(decode(image, back, secret, NULL) < 0);
    ASSERT(decode_digest(image, secret, digest) < 0);
    ASSERT(decode(NULL, back, secret, NULL) < 0);
    ASSERT(decode(image, NULL, secret, NULL) < 0);
    ASSERT(decode_digest(image, secret, NULL) < 0);

    unlink(image);
    unlink(back);
    free(image);
    free(back);
    PASS();
}

SUITE(stego_suite) {
    RUN_TEST(stego_round_trips_a_png_in_the_clear);
    RUN_TEST(stego_round_trips_a_png_encrypted);
    RUN_TEST(stego_round_trips_a_jpeg);
    RUN_TEST(stego_round_trips_across_several_chunks);
    RUN_TEST(stego_round_trips_an_empty_payload);
    RUN_TEST(encode_writes_nothing_when_the_payload_does_not_fit);
    RUN_TEST(encode_wants_data_it_can_read);
    RUN_TEST(decode_leaves_the_output_alone_on_a_wrong_passphrase);
    RUN_TEST(decode_finds_nothing_in_a_plain_image);
}
