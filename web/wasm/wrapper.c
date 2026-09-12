#include <cJSON.h>
#include <emscripten/emscripten.h>
#include <sodium.h>
#include <stb_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <veil/codecs/codec.h>
#include <veil/crypto/passphrase.h>
#include <veil/decode.h>
#include <veil/encode.h>
#include <veil/fs/file.h>
#include <veil/handlers/image.h>
#include <veil/log.h>

static char *json_take(cJSON *root) {
    char *out = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return out;
}

static char *fail(const char *reason) {
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", reason);

    return json_take(root);
}

static int file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;

    fclose(file);
    return 1;
}

static int passphrase_copy(const char *in, char out[PASSPHRASE_MAX]) {
    memset(out, 0, PASSPHRASE_MAX);

    if (!in)
        return 0;

    size_t len = strlen(in);
    if (len >= PASSPHRASE_MAX)
        return -1;

    memcpy(out, in, len);
    return 0;
}

EMSCRIPTEN_KEEPALIVE int veil_init(void) {
    return sodium_init() < 0 ? -1 : 0;
}

EMSCRIPTEN_KEEPALIVE void veil_set_verbose(int on) {
    verbose = on;
}

EMSCRIPTEN_KEEPALIVE void veil_free(char *ptr) {
    free(ptr);
}

EMSCRIPTEN_KEEPALIVE char *veil_encode(const char *target, const char *data_file, const char *output, const char *codec_name, const char *passphrase) {
    if (!target || !data_file || !output)
        return fail("target, data and output paths are all required");

    enum CodecType codec = str_to_codec(codec_name ? codec_name : "lsbm");
    if (codec == CODEC_UNKNOWN)
        return fail("unknown codec, expected one of lsbm, lsbr, dct");

    if (!file_exists(target))
        return fail("carrier file was not found");

    enum FileType type = get_file_type(target);
    if (!is_image_file(type))
        return fail("unsupported carrier file type");

    char secret[PASSPHRASE_MAX];
    if (passphrase_copy(passphrase, secret) < 0)
        return fail("passphrase is too long");

    unsigned char *data = NULL;
    size_t data_len = 0;

    if (read_file_raw_data(data_file, &data, &data_len) < 0) {
        sodium_memzero(secret, sizeof(secret));
        return fail("could not read the payload file");
    }

    struct ImageCtx ctx = {0};
    int status = encode(target, output, codec, secret, data, data_len, &ctx);

    int encrypted = secret[0] != '\0';
    sodium_memzero(secret, sizeof(secret));
    free(data);

    if (status < 0)
        return fail("encoding failed");

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddStringToObject(root, "output", output);
    cJSON_AddNumberToObject(root, "bytes", (double)data_len);
    cJSON_AddBoolToObject(root, "encrypted", encrypted);

    return json_take(root);
}

EMSCRIPTEN_KEEPALIVE char *veil_decode(const char *target, const char *output, const char *passphrase) {
    if (!target || !output)
        return fail("target and output paths are both required");

    if (!file_exists(target))
        return fail("target file was not found");

    char secret[PASSPHRASE_MAX];
    if (passphrase_copy(passphrase, secret) < 0)
        return fail("passphrase is too long");

    unsigned char *data = NULL;
    size_t data_len = 0;

    int status = decode(target, secret, &data, &data_len);
    sodium_memzero(secret, sizeof(secret));

    if (status < 0)
        return fail("decoding failed, wrong passphrase or no payload present");

    status = write_to_file_raw_data(output, data, data_len);

    sodium_memzero(data, data_len);
    free(data);

    if (status < 0)
        return fail("could not write the extracted payload");

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddStringToObject(root, "output", output);
    cJSON_AddNumberToObject(root, "bytes", (double)data_len);

    return json_take(root);
}
