#include "greatest.h"

#include "helpers.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "veil/analysis/stream.h"
#include "veil/codecs/image/container.h"
#include "veil/fs/file.h"
#include "veil/handlers/image.h"

#define PAYLOAD_LEN 64

static char *temp_path(void) {
    char path[] = "/tmp/test_stream_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;

    close(fd);
    return strdup(path);
}

static int embed(const char *source, const char *output, enum FileType type, enum CodecType codec) {
    unsigned char payload[PAYLOAD_LEN];
    for (size_t i = 0; i < PAYLOAD_LEN; i++)
        payload[i] = (unsigned char)(i * 7 + 3);

    struct ImageCtx ctx = {
        .source_file = source,
        .output_file = output,

        .image_type = type,
        .codec_type = codec,

        .passphrase = NULL,
    };

    return encode_image(&ctx, payload, PAYLOAD_LEN);
}

TEST hex_stream_is_the_file(void) {
    char *path = create_test_png(8, 8, 3);
    ASSERT(path != NULL);

    unsigned char *raw = NULL;
    size_t raw_len = 0;
    ASSERT_EQ(0, read_file_raw_data(path, &raw, &raw_len));

    struct Stream stream;
    ASSERT_EQ(0, stream_load(path, STREAM_HEX, &stream));
    ASSERT_EQ(STREAM_HEX, stream.kind);
    ASSERT_EQ(raw_len, stream.len);
    ASSERT_MEM_EQ(raw, stream.bytes, raw_len);

    stream_free(&stream);
    free(raw);
    unlink(path);
    free(path);
    PASS();
}

/*
 * The helper paints sample i with i % 256, so the low bits alternate 0, 1 and
 * every byte of the stream comes out 0b10101010 -- which only holds if the
 * stream packs its slots least significant bit first, the way the container
 * reads them back.
 */
TEST lsb_stream_packs_the_low_bits(void) {
    const int width = 64;
    const int height = 64;
    const int channels = 3;

    char *path = create_test_png(width, height, channels);
    ASSERT(path != NULL);

    struct Stream stream;
    ASSERT_EQ(0, stream_load(path, STREAM_LSB, &stream));

    const size_t slots = (size_t)width * (size_t)height * (size_t)channels;
    ASSERT_EQ(slots, stream.slots);
    ASSERT_EQ(slots / 8, stream.len);

    for (size_t i = 0; i < stream.len; i++)
        ASSERT_EQ_FMT(0xAAu, (unsigned)stream.bytes[i], "%u");

    stream_free(&stream);
    unlink(path);
    free(path);
    PASS();
}

/* The alpha channel carries nothing, so it is not walked. */
TEST lsb_stream_skips_the_alpha_channel(void) {
    char *path = create_test_png(16, 16, 4);
    ASSERT(path != NULL);

    struct Stream stream;
    ASSERT_EQ(0, stream_load(path, STREAM_LSB, &stream));
    ASSERT_EQ((size_t)16 * 16 * 3, stream.slots);

    stream_free(&stream);
    unlink(path);
    free(path);
    PASS();
}

TEST lsb_stream_opens_a_plain_payload(void) {
    char *source = create_test_png(64, 64, 3);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    ASSERT_EQ(0, embed(source, output, TYPE_PNG_IMAGE, CODEC_LSB_REPLACEMENT));

    struct Stream stream;
    ASSERT_EQ(0, stream_load(output, STREAM_LSB, &stream));
    ASSERT(stream.len >= HUSH_MAGIC_LEN);
    ASSERT_MEM_EQ(HUSH_MAGIC, stream.bytes, HUSH_MAGIC_LEN);

    stream_free(&stream);
    unlink(source);
    unlink(output);
    free(source);
    free(output);
    PASS();
}

TEST dct_stream_opens_a_plain_payload(void) {
    char *source = create_test_jpg(128, 128, 90);
    ASSERT(source != NULL);

    char *output = temp_path();
    ASSERT(output != NULL);

    ASSERT_EQ(0, embed(source, output, TYPE_JPEG_IMAGE, CODEC_DCT));

    struct Stream stream;
    ASSERT_EQ(0, stream_load(output, STREAM_DCT, &stream));
    ASSERT(stream.slots > 0);
    ASSERT(stream.len >= HUSH_MAGIC_LEN);
    ASSERT_MEM_EQ(HUSH_MAGIC, stream.bytes, HUSH_MAGIC_LEN);

    stream_free(&stream);
    unlink(source);
    unlink(output);
    free(source);
    free(output);
    PASS();
}

TEST dct_stream_refuses_a_png(void) {
    char *path = create_test_png(16, 16, 3);
    ASSERT(path != NULL);

    struct Stream stream;
    ASSERT_EQ(-1, stream_load(path, STREAM_DCT, &stream));

    unlink(path);
    free(path);
    PASS();
}

TEST construct_gives_a_png_hex_and_lsb(void) {
    char *path = create_test_png(16, 16, 3);
    ASSERT(path != NULL);

    struct StreamSet set;
    ASSERT_EQ(0, streams_construct(path, &set));
    ASSERT_EQ(TYPE_PNG_IMAGE, set.file_type);
    ASSERT_EQ((size_t)2, set.count);

    ASSERT(streams_find(&set, STREAM_HEX) != NULL);
    ASSERT(streams_find(&set, STREAM_LSB) != NULL);
    ASSERT(streams_find(&set, STREAM_DCT) == NULL);

    /* The hex stream leads, whatever the file type adds follows. */
    ASSERT_EQ(STREAM_HEX, set.streams[0].kind);

    streams_free(&set);
    unlink(path);
    free(path);
    PASS();
}

TEST construct_gives_a_jpeg_all_three(void) {
    char *path = create_test_jpg(64, 64, 90);
    ASSERT(path != NULL);

    struct StreamSet set;
    ASSERT_EQ(0, streams_construct(path, &set));
    ASSERT_EQ(TYPE_JPEG_IMAGE, set.file_type);
    ASSERT_EQ((size_t)3, set.count);

    const struct Stream *dct = streams_find(&set, STREAM_DCT);
    ASSERT(dct != NULL);
    ASSERT(dct->len > 0);

    streams_free(&set);
    unlink(path);
    free(path);
    PASS();
}

TEST construct_gives_a_plain_file_only_hex(void) {
    char *path = temp_path();
    ASSERT(path != NULL);

    const char *content = "not an image file";
    ASSERT_EQ(0, write_to_file_raw_data(path, (const unsigned char *)content, strlen(content)));

    struct StreamSet set;
    ASSERT_EQ(0, streams_construct(path, &set));
    ASSERT_EQ((size_t)1, set.count);
    ASSERT_EQ(STREAM_HEX, set.streams[0].kind);
    ASSERT_EQ(strlen(content), set.streams[0].len);

    streams_free(&set);
    unlink(path);
    free(path);
    PASS();
}

TEST construct_fails_on_a_missing_file(void) {
    struct StreamSet set;
    ASSERT_EQ(-1, streams_construct("/tmp/nonexistent_file_veil_stream", &set));
    PASS();
}

SUITE(stream_suite) {
    RUN_TEST(hex_stream_is_the_file);
    RUN_TEST(lsb_stream_packs_the_low_bits);
    RUN_TEST(lsb_stream_skips_the_alpha_channel);
    RUN_TEST(lsb_stream_opens_a_plain_payload);
    RUN_TEST(dct_stream_opens_a_plain_payload);
    RUN_TEST(dct_stream_refuses_a_png);
    RUN_TEST(construct_gives_a_png_hex_and_lsb);
    RUN_TEST(construct_gives_a_jpeg_all_three);
    RUN_TEST(construct_gives_a_plain_file_only_hex);
    RUN_TEST(construct_fails_on_a_missing_file);
}
