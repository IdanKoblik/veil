#include "greatest.h"

#include "helpers.h"
#include <veil/fs/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAYLOAD "$$VEIL$$ the payload begins here and runs for a while"

static char *payload_file(const char *bytes) {
    return create_temp_file((const unsigned char *)bytes, strlen(bytes));
}

static int run_into(const char *args, const char *passphrase, const char *stdout_path) {
    char command[2048];
    const int len = snprintf(command, sizeof(command), VEIL_BINARY " %s >%s 2>/dev/null", args, stdout_path ? stdout_path : "/dev/null");
    if (len < 0 || (size_t)len >= sizeof(command))
        return -1;

    FILE *veil = popen(command, "w");
    if (!veil)
        return -1;

    fprintf(veil, "%s\n", passphrase ? passphrase : "");

    const int status = pclose(veil);
    if (status < 0 || !WIFEXITED(status))
        return -1;

    return WEXITSTATUS(status);
}

static int run(const char *args, const char *passphrase) {
    return run_into(args, passphrase, NULL);
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

static enum greatest_test_res round_trip(char *carrier, const char *passphrase) {
    char *data = payload_file(PAYLOAD);
    char *stego = create_temp_path();
    char *back = create_temp_path();

    ASSERT(carrier && data && stego && back);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s -verify", carrier, data, stego);
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
    CHECK_CALL(round_trip(create_test_png(96, 96, 3), NULL));
    PASS();
}

TEST encode_then_decode_encrypted(void) {
    CHECK_CALL(round_trip(create_test_png(96, 96, 3), "correct horse battery staple"));
    PASS();
}

TEST encode_then_decode_a_jpeg(void) {
    CHECK_CALL(round_trip(create_test_jpg(128, 128, 90), "correct horse battery staple"));
    PASS();
}

TEST decode_writes_to_stdout(void) {
    char *carrier = create_test_png(96, 96, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = create_temp_path();
    char *back = create_temp_path();

    ASSERT(carrier && data && stego && back);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s", carrier, data, stego);
    ASSERT_EQ(0, run(args, NULL));

    snprintf(args, sizeof(args), "decode %s -o -", stego);
    ASSERT_EQ(0, run_into(args, NULL, back));

    // Nothing but the payload, the prompt and status lines stay out of stdout.
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

TEST decode_refuses_the_wrong_passphrase(void) {
    char *carrier = create_test_png(96, 96, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = create_temp_path();
    char *back = payload_file("already here");

    ASSERT(carrier && data && stego && back);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s", carrier, data, stego);
    ASSERT_EQ(0, run(args, "the right one"));

    snprintf(args, sizeof(args), "decode %s -o %s", stego, back);
    ASSERT_EQ(1, run(args, "the wrong one"));
    ASSERT_EQ(1, run(args, NULL));

    CHECK_CALL(same_as_payload(back, "already here"));

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
    char *back = create_temp_path();

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

TEST decode_will_not_overwrite_its_own_target(void) {
    char *carrier = create_test_png(96, 96, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = create_temp_path();

    ASSERT(carrier && data && stego);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s", carrier, data, stego);
    ASSERT_EQ(0, run(args, NULL));

    snprintf(args, sizeof(args), "decode %s -o %s", stego, stego);
    ASSERT_EQ(1, run(args, NULL));
    ASSERT_EQ(TYPE_PNG_IMAGE, get_file_type(stego));

    unlink(carrier);
    unlink(data);
    unlink(stego);

    free(carrier);
    free(data);
    free(stego);
    PASS();
}

TEST commands_reject_stray_arguments(void) {
    char *carrier = create_test_png(64, 64, 3);
    char *data = payload_file(PAYLOAD);
    char *stego = create_temp_path();

    ASSERT(carrier && data && stego);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s stray -o %s", carrier, data, stego);
    ASSERT_EQ(1, run(args, NULL));

    snprintf(args, sizeof(args), "decode %s stray -o %s", carrier, stego);
    ASSERT_EQ(1, run(args, NULL));

    unlink(carrier);
    unlink(data);
    unlink(stego);

    free(carrier);
    free(data);
    free(stego);
    PASS();
}

TEST encode_will_not_take_a_carrier_it_cannot_read(void) {
    char *data = payload_file(PAYLOAD);
    char *stego = create_temp_path();

    ASSERT(data && stego);

    char args[1024];
    snprintf(args, sizeof(args), "encode /tmp/no_such_carrier_e2e.png %s -o %s", data, stego);
    ASSERT_EQ(1, run(args, NULL));

    snprintf(args, sizeof(args), "encode %s %s -o %s", data, data, stego);
    ASSERT_EQ(1, run(args, NULL));

    unlink(data);
    unlink(stego);

    free(data);
    free(stego);
    PASS();
}

TEST encode_needs_data_it_can_read_and_fit(void) {
    char *tiny = create_test_png(8, 8, 3);
    char *carrier = create_test_png(64, 64, 3);
    char *data = payload_file(PAYLOAD PAYLOAD PAYLOAD);
    char *stego = create_temp_path();

    ASSERT(tiny && carrier && data && stego);
    unlink(stego);

    char args[1024];
    snprintf(args, sizeof(args), "encode %s %s -o %s", tiny, data, stego);
    ASSERT_EQ(1, run(args, NULL));
    ASSERT(access(stego, F_OK) != 0);

    snprintf(args, sizeof(args), "encode %s /tmp/no_such_data_e2e -o %s", carrier, stego);
    ASSERT_EQ(1, run(args, NULL));

    unlink(tiny);
    unlink(carrier);
    unlink(data);

    free(tiny);
    free(carrier);
    free(data);
    free(stego);
    PASS();
}

TEST the_binary_knows_its_subcommands(void) {
    ASSERT_EQ(1, run("", NULL));
    ASSERT_EQ(1, run("nonsense", NULL));
    ASSERT_EQ(1, run("encode", NULL));
    ASSERT_EQ(1, run("decode", NULL));
    ASSERT_EQ(0, run("help", NULL));
    PASS();
}

SUITE(e2e_suite) {
    RUN_TEST(encode_then_decode_in_the_clear);
    RUN_TEST(encode_then_decode_encrypted);
    RUN_TEST(encode_then_decode_a_jpeg);
    RUN_TEST(decode_writes_to_stdout);
    RUN_TEST(decode_refuses_the_wrong_passphrase);
    RUN_TEST(decode_finds_nothing_in_a_plain_carrier);
    RUN_TEST(decode_will_not_overwrite_its_own_target);
    RUN_TEST(commands_reject_stray_arguments);
    RUN_TEST(encode_will_not_take_a_carrier_it_cannot_read);
    RUN_TEST(encode_needs_data_it_can_read_and_fit);
    RUN_TEST(the_binary_knows_its_subcommands);
}
