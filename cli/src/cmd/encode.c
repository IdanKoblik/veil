#include "../args.h"
#include "../prompt.h"
#include "command.h"
#include <fcntl.h>
#include <flag.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <veil/encode.h>
#include <veil/fs/checksum.h>
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

    if (!output_file)
        return EXEC_USAGE_ERROR;

    enum FileType type = get_file_type(target);
    if (type == TYPE_NOT_FOUND) {
        ERROR("Target file was not found");
        return EXEC_GENERIC_ERROR;
    }

    if (!is_image_file(type)) {
        ERROR("UNSUPPORTED");
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
    if (encode(target, data_file, output_file, passphrase) < 0) {
        ERROR("Failed to encode target (%s)", target);
        goto fail;
    }

    return EXEC_OK;
fail:
    return EXEC_GENERIC_ERROR;
}

static const struct Command encode_cmd = {.name = "encode", .description = "Encodes a data isnside of a target file.", .usage = "Usage: encode <target_file> <data_file> -o <output_file> -c <codec> [-verify]\n", .exec = exec};

COMMAND(encode_cmd);
