#include "greatest.h"

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAYLOAD "$$HUSH$$ the payload begins here and runs for a while"

static char *temp_path(const char *stem) {
    char path[64];
    snprintf(path, sizeof(path), "/tmp/test_e2e_%s_XXXXXX", stem);

    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;

    close(fd);
    return strdup(path);
}

static char *payload_file(const char *bytes) {
    char *path = temp_path("data");
    if (!path)
        return NULL;

    FILE *file = fopen(path, "wb");
    if (!file) {
        free(path);
        return NULL;
    }

    fwrite(bytes, 1, strlen(bytes), file);
    fclose(file);

    return path;
}

static int run(const char *args, const char *passphrase) {
    char command[1024];
    snprintf(command, sizeof(command), VEIL_BINARY " %s >/dev/null 2>&1", args);

    FILE *veil = popen(command, "w");
    if (!veil)
        return -1;

    fprintf(veil, "%s\n", passphrase ? passphrase : "");

    const int status = pclose(veil);
    if (status < 0 || !WIFEXITED(status))
        return -1;

    return WEXITSTATUS(status);
}

static enum greatest_test_res same_as_payload(const char *path, const char *want) {
    FILE *file = fopen(path, "rb");
    ASSERT(file != NULL);

    char got[512] = {0};
    const size_t len = fread(got, 1, sizeof(got) - 1, file);
    fclose(file);

    ASSERT_EQ(strlen(want), len);
    ASSERT_STR_EQ(want, got);
    PASS();
}

static enum greatest_test_res round_trip(const char *codec, const char *passphrase) {
    char *carrier = create_test_png(96, 96, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = temp_path("png");
    char *back = temp_path("out");

    ASSERT(carrier && data && stego && back);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s -c %s -verify", carrier, data, stego, codec);
    ASSERT_EQ(0, run(args, passphrase));

    snprintf(args, sizeof(args), "decode %s -o %s", stego, back);
    ASSERT_EQ(0, run(args, passphrase));

    CHECK_CALL(same_as_payload(back, PAYLOAD));

    unlink(carrier);
    unlink(data);
    unlink(stego);
    unlink(back);

    free(carrier);
    free(data);
    free(stego);
    free(back);
    PASS();
}

TEST encode_then_decode_in_the_clear(void) {
    CHECK_CALL(round_trip("lsbr", NULL));
    PASS();
}

TEST encode_then_decode_with_matching(void) {
    CHECK_CALL(round_trip("lsbm", NULL));
    PASS();
}

TEST encode_then_decode_encrypted(void) {
    CHECK_CALL(round_trip("lsbr", "correct horse battery staple"));
    PASS();
}

TEST decode_refuses_the_wrong_passphrase(void) {
    char *carrier = create_test_png(96, 96, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = temp_path("png");
    char *back = temp_path("out");

    ASSERT(carrier && data && stego && back);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s -c lsbr", carrier, data, stego);
    ASSERT_EQ(0, run(args, "the right one"));

    snprintf(args, sizeof(args), "decode %s -o %s", stego, back);
    ASSERT_EQ(1, run(args, "the wrong one"));

    unlink(carrier);
    unlink(data);
    unlink(stego);
    unlink(back);

    free(carrier);
    free(data);
    free(stego);
    free(back);
    PASS();
}

TEST decode_finds_nothing_in_a_plain_carrier(void) {
    char *carrier = create_test_png(64, 64, 3);
    char *back = temp_path("out");

    ASSERT(carrier && back);

    char args[1024];
    snprintf(args, sizeof(args), "decode %s -o %s", carrier, back);
    ASSERT_EQ(1, run(args, NULL));

    unlink(carrier);
    unlink(back);

    free(carrier);
    free(back);
    PASS();
}

TEST encode_will_not_take_a_carrier_it_cannot_read(void) {
    char *data = payload_file(PAYLOAD);
    char *stego = temp_path("png");

    ASSERT(data && stego);

    char args[1024];
    snprintf(args, sizeof(args), "encode /tmp/no_such_carrier_e2e.png %s -o %s -c lsbr", data, stego);
    ASSERT_EQ(1, run(args, NULL));

    snprintf(args, sizeof(args), "encode %s %s -o %s -c lsbr", data, data, stego);
    ASSERT_EQ(1, run(args, NULL));

    unlink(data);
    unlink(stego);

    free(data);
    free(stego);
    PASS();
}

TEST encode_wants_an_output_and_a_known_codec(void) {
    char *carrier = create_test_png(64, 64, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = temp_path("png");

    ASSERT(carrier && data && stego);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -c lsbr", carrier, data);
    ASSERT_EQ(1, run(args, NULL));

    snprintf(args, sizeof(args), "encode %s %s -o %s -c nonsense", carrier, data, stego);
    ASSERT_EQ(1, run(args, NULL));

    unlink(carrier);
    unlink(data);
    unlink(stego);

    free(carrier);
    free(data);
    free(stego);
    PASS();
}

TEST the_binary_knows_its_subcommands(void) {
    ASSERT_EQ(1, run("", NULL));
    ASSERT_EQ(1, run("nonsense", NULL));
    ASSERT_EQ(1, run("encode", NULL));
    PASS();
}

SUITE(e2e_suite) {
    RUN_TEST(encode_then_decode_in_the_clear);
    RUN_TEST(encode_then_decode_with_matching);
    RUN_TEST(encode_then_decode_encrypted);
    RUN_TEST(decode_refuses_the_wrong_passphrase);
    RUN_TEST(decode_finds_nothing_in_a_plain_carrier);
    RUN_TEST(encode_will_not_take_a_carrier_it_cannot_read);
    RUN_TEST(encode_wants_an_output_and_a_known_codec);
    RUN_TEST(the_binary_knows_its_subcommands);
}
