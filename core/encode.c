#include "encode.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <veil/log.h>

#include "codecs/container.h"

#define CHUNK_SIZE (64 * 1024)

int encode(const char *target, const char *data_file, const char *output, char passphrase[PASSPHRASE_MAX], unsigned char digest[PAYLOAD_DIGEST_BYTES], size_t *payload_len) {
    if (!target || !data_file || !output)
        return -1;

    const int is_pipe = strcmp(data_file, "-") == 0;
    const int fd = is_pipe ? STDIN_FILENO : open(data_file, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        ERROR("Failed to open the data file (%s)", data_file);
        return -1;
    }

    int rc = -1;
    unsigned char buffer[CHUNK_SIZE];

    crypto_generichash_state hash;
    crypto_generichash_init(&hash, NULL, 0, PAYLOAD_DIGEST_BYTES);

    struct Container *container = container_init(target, passphrase);
    if (!container)
        goto done;

    if (container_reserve_header(container) < 0)
        goto done;

    size_t total = 0;
    for (;;) {
        const ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR)
                continue;

            ERROR("Failed to read the data (%s)", data_file);
            goto done;
        }

        if (n == 0)
            break;

        if (container_encode_chunk(container, buffer, (size_t)n) < 0)
            goto done;

        crypto_generichash_update(&hash, buffer, (size_t)n);

        total += (size_t)n;
    }

    container->header.payload_len = total;
    if (container_write_header(container) < 0)
        goto done;

    if (container->carrier->save(container->carrier, output) < 0)
        goto done;

    if (digest)
        crypto_generichash_final(&hash, digest, PAYLOAD_DIGEST_BYTES);

    if (payload_len)
        *payload_len = total;

    DEBUG("Encoded %zu bytes into %s", total, output);
    rc = 0;
done:
    sodium_memzero(&hash, sizeof(hash));
    sodium_memzero(buffer, sizeof(buffer));
    container_free(container);
    if (!is_pipe)
        close(fd);

    return rc;
}
