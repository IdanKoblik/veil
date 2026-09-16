#include "../args.h"
#include "../prompt.h"
#include "command.h"
#include <fcntl.h>
#include <flag.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <veil/decode.h>
#include <veil/encode.h>
#include <veil/fs/file.h>
#include <veil/log.h>

static int exec(int argc, char *argv[]) {
    const char *target = shift_args(&argc, &argv);
    const char *data_file = shift_args(&argc, &argv);
    if (!target || !data_file)
        return EXEC_USAGE_ERROR;

    char *output_file = NULL;

    // THANKS TSODING
    argc++;
    argv--;

    bool verify = false;
    flag_str_var(&output_file, "o", "output.bin", "Output file");
    flag_bool_var(&verify, "verify", false, "Check encoding result");

    if (!flag_parse(argc, argv)) {
        flag_print_error(stderr);
        return EXEC_USAGE_ERROR;
    }

    // flag.h stops at the first stray argument, so any flags after it were never parsed.
    if (flag_rest_argc() > 0) {
        ERROR("Unexpected argument %s", flag_rest_argv()[0]);
        return EXEC_USAGE_ERROR;
    }

    if (!output_file)
        return EXEC_USAGE_ERROR;

    enum FileType type = get_file_type(target);
    if (type == TYPE_NOT_FOUND) {
        ERROR("Target file was not found");
        return EXEC_GENERIC_ERROR;
    }

    if (!is_image_file(type)) {
        ERROR("%s is %s, not a PNG or JPEG image", target, file_type_name(type));
        return EXEC_GENERIC_ERROR;
    }

    const int is_pipe = strcmp(data_file, "-") == 0;
    const int passphrase_fd = is_pipe ? open("/dev/tty", O_RDONLY | O_CLOEXEC) : STDIN_FILENO;
    if (passphrase_fd < 0) {
        ERROR("No terminal to read the passphrase from while the data comes through stdin");
        return EXEC_GENERIC_ERROR;
    }

    PASSPHRASE(passphrase);
    const int read_status = read_passphrase("Passphrase (leave empty to disable encryption): ", passphrase_fd, passphrase, sizeof(passphrase));
    if (is_pipe)
        close(passphrase_fd);

    if (read_status < 0) {
        ERROR("Failed to read the passphrase");
        return EXEC_GENERIC_ERROR;
    }

    DEBUG("Target: %s, Data: %s, Output: %s", target, data_file, output_file);
    unsigned char want[PAYLOAD_DIGEST_BYTES];
    size_t payload_len = 0;
    if (encode(target, data_file, output_file, passphrase, verify ? want : NULL, &payload_len) < 0) {
        ERROR("Failed to encode target (%s)", target);
        return EXEC_GENERIC_ERROR;
    }

    INFO("Read %zu bytes from %s", payload_len, is_pipe ? "stdin" : data_file);
    INFO("Hid them in %s (%s, %s)", target, file_type_name(type), passphrase[0] ? "encrypted" : "not encrypted");

    if (verify) {
        unsigned char got[PAYLOAD_DIGEST_BYTES];
        if (decode_digest(output_file, passphrase, got) < 0 || sodium_memcmp(want, got, sizeof(want)) != 0) {
            ERROR("Verification failed, %s does not decode back to the data", output_file);
            return EXEC_GENERIC_ERROR;
        }

        INFO("Verified %zu bytes read back from %s", payload_len, output_file);
    }

    INFO("Encoded successfully -> %s", output_file);
    return EXEC_OK;
}

static const struct Command encode_cmd = {.name = "encode", .description = "Encodes a data inside of a target file.", .usage = "Usage: encode <target_file> <data_file> -o <output_file> [-verify]\n", .exec = exec};

COMMAND(encode_cmd);
