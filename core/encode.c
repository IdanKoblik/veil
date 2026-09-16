#include "encode.h"
#include <errno.h>
#include <unistd.h>
#include <veil/log.h>

#include "codecs/container.h"
#include "fs/file.h"

#define CHUNK_SIZE (64 * 1024)

int encode(const char *target, const char *output, char passphrase[PASSPHRASE_MAX], int is_pipe) {
    struct Container *container = container_init(target, passphrase);
    if (!container)
        return -1;

    if (container_reserve_header(container) < 0)
        goto fail;

    if (!is_pipe) {
        unsigned char *data = NULL;
        size_t data_len = 0;

        if (read_file_raw_data(target, &data, &data_len) < 0)
            goto fail;

        container->header.payload_len = data_len;
        if (container_write_header(container))
            goto fail;

        if (container_encode_chunk(container, data, data_len) < 0)
            goto fail;

        goto success;
    }

    unsigned char buffer[CHUNK_SIZE];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (n == 0)
            goto success; // EOF

        if (n > 0) {
            container_encode_chunk(container, buffer, n);
            continue;
        }

        if (errno == EINTR)
            continue;

        goto fail;
    }

success:
    if (container->carrier->save(container->carrier, output) < 0)
        goto fail;

    container_free(container);
    return 0;
fail:
    container_free(container);
    return -1;
}
