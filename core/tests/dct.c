#include "greatest.h"
#include "helpers.h"

#include "veil/codecs/codec.h"
#include "veil/fs/file.h"
#include "veil/handlers/image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAYLOAD_LEN 200
#define CARRIER_SIZE 256
#define CARRIER_QUALITY 95

static char *temp_path(void) {
    char path[] = "/tmp/test_dct_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;

    close(fd);
    return strdup(path);
}

static void fill_payload(unsigned char *payload) {
    for (size_t i = 0; i < PAYLOAD_LEN; i++)
        payload[i] = (unsigned char)(i * 7 + 3);
}

static int encode_payload(const char *source, const char *output, const unsigned char *payload, enum CodecType codec, const char *passphrase) {
    struct ImageCtx ctx = {
        .source_file = source,
        .output_file = output,

        .image_type = TYPE_JPEG_IMAGE,
        .codec_type = codec,

        .passphrase = passphrase,
    };

    return encode_image(&ctx, (unsigned char *)payload, PAYLOAD_LEN);
}

static int decode_payload(const char *source, unsigned char **data, size_t *data_len, const char *passphrase) {
    struct ImageCtx ctx = {
        .source_file = source,

        .image_type = TYPE_JPEG_IMAGE,
        .codec_type = CODEC_UNKNOWN,

        .passphrase = passphrase,
    };

    return decode_image(&ctx, data, data_len);
}

/* Rots the entropy coded tail, which leaves the sequential header readable. */
static int corrupt_tail(const char *path) {
    unsigned char *data = NULL;
    size_t data_len = 0;

    if (read_file_raw_data(path, &data, &data_len) < 0)
        return -1;

    for (size_t i = data_len / 2; i + 2 < data_len; i++)
        data[i] ^= 1;

    int status = write_to_file_raw_data(path, data, data_len);
    free(data);

    return status;
}

static enum greatest_test_res round_trip(enum CodecType codec, const char *passphrase) {
    char *source = create_test_jpg(CARRIER_SIZE, CARRIER_SIZE, CARRIER_QUALITY);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, encode_payload(source, output, payload, codec, passphrase));

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    ASSERT_EQ(0, decode_payload(output, &decoded, &decoded_len, passphrase));

    ASSERT_EQ(PAYLOAD_LEN, decoded_len);
    ASSERT_MEM_EQ(payload, decoded, PAYLOAD_LEN);

    free(decoded);
    unlink(source);
    unlink(output);
    free(source);
    free(output);

    PASS();
}

TEST decode_reads_back_plain_replacement(void) {
    CHECK_CALL(round_trip(CODEC_LSB_REPLACEMENT, NULL));
    PASS();
}

TEST decode_reads_back_plain_matching(void) {
    CHECK_CALL(round_trip(CODEC_LSB_MATCHING, NULL));
    PASS();
}

TEST decode_reads_back_encrypted_replacement(void) {
    CHECK_CALL(round_trip(CODEC_LSB_REPLACEMENT, "correct horse battery staple"));
    PASS();
}

TEST decode_reads_back_encrypted_matching(void) {
    CHECK_CALL(round_trip(CODEC_LSB_MATCHING, "correct horse battery staple"));
    PASS();
}

TEST detect_image_finds_jpeg(void) {
    char *source = create_test_jpg(32, 32, CARRIER_QUALITY);
    ASSERT(source != NULL);

    ASSERT_EQ(TYPE_JPEG_IMAGE, detect_image_type(source));

    unlink(source);
    free(source);
    PASS();
}

TEST decode_rejects_a_wrong_passphrase(void) {
    char *source = create_test_jpg(CARRIER_SIZE, CARRIER_SIZE, CARRIER_QUALITY);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, encode_payload(source, output, payload, CODEC_LSB_REPLACEMENT, "right one"));

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    ASSERT_EQ(-1, decode_payload(output, &decoded, &decoded_len, "wrong one"));
    ASSERT(decoded == NULL);

    unlink(source);
    unlink(output);
    free(source);
    free(output);
    PASS();
}

TEST decode_rejects_an_image_without_a_container(void) {
    char *source = create_test_jpg(CARRIER_SIZE, CARRIER_SIZE, CARRIER_QUALITY);
    ASSERT(source != NULL);

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    ASSERT_EQ(-1, decode_payload(source, &decoded, &decoded_len, NULL));

    unlink(source);
    free(source);
    PASS();
}

TEST decode_rejects_a_tampered_payload(void) {
    char *source = create_test_jpg(CARRIER_SIZE, CARRIER_SIZE, CARRIER_QUALITY);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, encode_payload(source, output, payload, CODEC_LSB_REPLACEMENT, "a passphrase"));
    ASSERT_EQ(0, corrupt_tail(output));

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    ASSERT_EQ(-1, decode_payload(output, &decoded, &decoded_len, "a passphrase"));
    ASSERT(decoded == NULL);

    unlink(source);
    unlink(output);
    free(source);
    free(output);
    PASS();
}

SUITE(dct_suite) {
    RUN_TEST(detect_image_finds_jpeg);
    RUN_TEST(decode_reads_back_plain_replacement);
    RUN_TEST(decode_reads_back_plain_matching);
    RUN_TEST(decode_reads_back_encrypted_replacement);
    RUN_TEST(decode_reads_back_encrypted_matching);
    RUN_TEST(decode_rejects_a_wrong_passphrase);
    RUN_TEST(decode_rejects_an_image_without_a_container);
    RUN_TEST(decode_rejects_a_tampered_payload);
}
