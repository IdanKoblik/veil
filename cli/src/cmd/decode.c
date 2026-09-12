#include "../args.h"
#include "../prompt.h"
#include "command.h"
#include <flag.h>
#include <sodium/utils.h>
#include <veil/decode.h>
#include <veil/fs/file.h>
#include <veil/handlers/image.h>
#include <veil/log.h>

#include <sodium.h>
#include <stdlib.h>

static int exec(int argc, char *argv[]) {
    const char *target = shift_args(&argc, &argv);
    if (!target)
        return EXEC_USAGE_ERROR;

    char *output_file = NULL;

    // THANKS TSODING
    argc++;
    argv--;

    flag_str_var(&output_file, "o", NULL, "Output file");

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

    PASSPHRASE(passphrase);
    if (read_passphrase("Passphrase (leave empty if the data is not encrypted): ", passphrase, sizeof(passphrase)) < 0) {
        ERROR("Failed to read the passphrase");
        return EXEC_GENERIC_ERROR;
    }

    unsigned char *data = NULL;
    size_t data_len = 0;

    DEBUG("Target: %s, Output: %s", target, output_file);

    if (decode(target, passphrase, &data, &data_len) < 0) {
        ERROR("Failed to decode the target");
        return EXEC_GENERIC_ERROR;
    }

    DEBUG("Writing %zu bytes into %s", data_len, output_file);
    int status = write_to_file_raw_data(output_file, data, data_len);

    if (data) {
        sodium_memzero(data, data_len);
        free(data);
    }

    if (status < 0) {
        ERROR("Failed to write the decoded data into %s", output_file);
        return EXEC_GENERIC_ERROR;
    }

    INFO("Decoded successfully -> %s", output_file);
    return EXEC_OK;
}

static const struct Command decode_cmd = {.name = "decode", .description = "Decodes the data hidden inside of a target file.", .usage = "Usage: decode <target_file> -o <output_file>\n", .exec = exec};

COMMAND(decode_cmd);
