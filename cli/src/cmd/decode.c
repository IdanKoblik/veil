#include "../args.h"
#include "../prompt.h"
#include "command.h"
#include <flag.h>
#include <sodium.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <veil/decode.h>
#include <veil/fs/file.h>
#include <veil/log.h>

static int exec(int argc, char *argv[]) {
    const char *target = shift_args(&argc, &argv);
    if (!target)
        return EXEC_USAGE_ERROR;

    char *output_file = NULL;

    // THANKS TSODING
    argc++;
    argv--;

    flag_str_var(&output_file, "o", "output.bin", "Output file, - for stdout");

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

    struct stat target_stat;
    struct stat output_stat;
    if (strcmp(output_file, "-") != 0 && stat(target, &target_stat) == 0 && stat(output_file, &output_stat) == 0 &&
        target_stat.st_dev == output_stat.st_dev && target_stat.st_ino == output_stat.st_ino) {
        ERROR("Refusing to decode %s into itself, pick another output with -o", target);
        return EXEC_GENERIC_ERROR;
    }

    enum FileType type = get_file_type(target);
    if (type == TYPE_NOT_FOUND) {
        ERROR("Target file was not found");
        return EXEC_GENERIC_ERROR;
    }

    if (!is_image_file(type)) {
        ERROR("%s is %s, not a PNG or JPEG image", target, file_type_name(type));
        return EXEC_GENERIC_ERROR;
    }

    PASSPHRASE(passphrase);
    if (read_passphrase("Passphrase (leave empty if the data is not encrypted): ", STDIN_FILENO, passphrase, sizeof(passphrase)) < 0) {
        ERROR("Failed to read the passphrase");
        return EXEC_GENERIC_ERROR;
    }

    DEBUG("Target: %s, Output: %s", target, output_file);
    size_t payload_len = 0;
    if (decode(target, output_file, passphrase, &payload_len) < 0) {
        ERROR("Failed to decode target (%s)", target);
        return EXEC_GENERIC_ERROR;
    }

    if (strcmp(output_file, "-") != 0) {
        INFO("Read %zu bytes out of %s (%s, %s)", payload_len, target, file_type_name(type), passphrase[0] ? "encrypted" : "not encrypted");
        INFO("Decoded successfully -> %s", output_file);
    }

    return EXEC_OK;
}

static const struct Command decode_cmd = {.name = "decode", .description = "Decodes the data hidden inside of a target file.", .usage = "Usage: decode <target_file> -o <output_file>\n", .exec = exec};

COMMAND(decode_cmd);
