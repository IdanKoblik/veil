#include "encode.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <veil/log.h>

#include "codecs/container.h"

#define CHUNK_SIZE (64 * 1024)

int encode(const char *target, const char *data_file, const char *output, char passphrase[PASSPHRASE_MAX]) {
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

        total += (size_t)n;
    }

    container->header.payload_len = total;
    if (container_write_header(container) < 0)
        goto done;

    if (container->carrier->save(container->carrier, output) < 0)
        goto done;

    DEBUG("Encoded %zu bytes into %s", total, output);
    rc = 0;
done:
    sodium_memzero(buffer, sizeof(buffer));
    container_free(container);
    if (!is_pipe)
        close(fd);

    return rc;
}
