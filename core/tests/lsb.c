#include "greatest.h"

#include "helpers.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "veil/codecs/codec.h"
#include "veil/handlers/image.h"

#define PAYLOAD_LEN 200

static char *temp_path(void) {
    char path[] = "/tmp/test_lsb_XXXXXX";
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

        .image_type = TYPE_PNG_IMAGE,
        .codec_type = codec,

        .passphrase = passphrase,
    };

    return encode_image(&ctx, (unsigned char *)payload, PAYLOAD_LEN);
}

static int decode_payload(const char *source, unsigned char **data, size_t *data_len, const char *passphrase) {
    struct ImageCtx ctx = {
        .source_file = source,

        .image_type = TYPE_PNG_IMAGE,
        .codec_type = CODEC_UNKNOWN,

        .passphrase = passphrase,
    };

    return decode_image(&ctx, data, data_len);
}

/* Flips the least significant bit of every byte past the given offset. */
static int corrupt_pixels(const char *path, size_t from) {
    int width;
    int height;
    int channels;

    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 0);
    if (!pixels)
        return -1;

    size_t len = (size_t)width * height * channels;
    for (size_t i = from; i < len; i++)
        pixels[i] ^= 1;

    int ok = stbi_write_png(path, width, height, channels, pixels, width * channels);
    stbi_image_free(pixels);

    return ok ? 0 : -1;
}

static enum greatest_test_res round_trip(enum CodecType codec, const char *passphrase) {
    char *source = create_test_png(64, 64, 3);
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

TEST decode_rejects_a_wrong_passphrase(void) {
    char *source = create_test_png(64, 64, 3);
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

TEST decode_rejects_a_missing_passphrase(void) {
    char *source = create_test_png(64, 64, 3);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, encode_payload(source, output, payload, CODEC_LSB_REPLACEMENT, "a passphrase"));

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    ASSERT_EQ(-1, decode_payload(output, &decoded, &decoded_len, NULL));

    unlink(source);
    unlink(output);
    free(source);
    free(output);
    PASS();
}

TEST decode_rejects_an_image_without_a_container(void) {
    char *source = create_test_png(64, 64, 3);
    ASSERT(source != NULL);

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    ASSERT_EQ(-1, decode_payload(source, &decoded, &decoded_len, NULL));

    unlink(source);
    free(source);
    PASS();
}

TEST decode_rejects_a_tampered_payload(void) {
    char *source = create_test_png(64, 64, 3);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    unsigned char payload[PAYLOAD_LEN];
    fill_payload(payload);

    ASSERT_EQ(0, encode_payload(source, output, payload, CODEC_LSB_REPLACEMENT, "a passphrase"));

    /* Leaves the preamble and the sealed header alone, only the payload rots. */
    ASSERT_EQ(0, corrupt_pixels(output, 2048));

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

SUITE(lsb_suite) {
    RUN_TEST(decode_reads_back_plain_replacement);
    RUN_TEST(decode_reads_back_plain_matching);
    RUN_TEST(decode_reads_back_encrypted_replacement);
    RUN_TEST(decode_reads_back_encrypted_matching);
    RUN_TEST(decode_rejects_a_wrong_passphrase);
    RUN_TEST(decode_rejects_a_missing_passphrase);
    RUN_TEST(decode_rejects_an_image_without_a_container);
    RUN_TEST(decode_rejects_a_tampered_payload);
}
