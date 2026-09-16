#include "decode.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <veil/log.h>

#include "codecs/container.h"

#define CHUNK_SIZE (64 * 1024)

typedef int (*ChunkSink)(const unsigned char *chunk, size_t len, void *ctx);

static int write_all(int fd, const unsigned char *buffer, size_t len) {
    while (len > 0) {
        const ssize_t n = write(fd, buffer, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        buffer += n;
        len -= (size_t)n;
    }

    return 0;
}

static int fd_sink(const unsigned char *chunk, size_t len, void *ctx) {
    return write_all(*(const int *)ctx, chunk, len);
}

static int hash_sink(const unsigned char *chunk, size_t len, void *ctx) {
    return crypto_generichash_update(ctx, chunk, len);
}

static struct Container *open_container(const char *target, char passphrase[PASSPHRASE_MAX]) {
    struct Container *container = container_init(target, passphrase);
    if (!container)
        return NULL;

    if (container_read_header(container) < 0) {
        container_free(container);
        return NULL;
    }

    return container;
}

static int decode_payload(struct Container *container, ChunkSink sink, void *ctx) {
    size_t remaining = container->header.payload_len;
    while (remaining > 0) {
        const size_t len = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;

        unsigned char *chunk = NULL;
        if (container_decode_chunk(container, &chunk, len) < 0)
            return -1;

        const int rc = sink(chunk, len, ctx);
        sodium_memzero(chunk, len);
        free(chunk);

        if (rc < 0)
            return -1;

        remaining -= len;
    }

    return 0;
}

int decode(const char *target, const char *output, char passphrase[PASSPHRASE_MAX], size_t *payload_len) {
    if (!target || !output)
        return -1;

    // Read the header before touching the output, so a wrong passphrase doesn't truncate an existing file.
    struct Container *container = open_container(target, passphrase);
    if (!container)
        return -1;

    const int is_pipe = strcmp(output, "-") == 0;
    const int fd = is_pipe ? STDOUT_FILENO : open(output, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        ERROR("Failed to open the output file (%s)", output);
        container_free(container);
        return -1;
    }

    int rc = decode_payload(container, fd_sink, (void *)&fd);
    if (rc < 0)
        ERROR("Failed to write the decoded data (%s)", output);
    else if (payload_len)
        *payload_len = container->header.payload_len;

    container_free(container);
    if (!is_pipe) {
        if (close(fd) < 0 && rc == 0) {
            ERROR("Failed to write the decoded data (%s)", output);
            rc = -1;
        }

        if (rc < 0)
            unlink(output);
    }

    return rc;
}

int decode_digest(const char *target, char passphrase[PASSPHRASE_MAX], unsigned char digest[PAYLOAD_DIGEST_BYTES]) {
    if (!target || !digest)
        return -1;

    struct Container *container = open_container(target, passphrase);
    if (!container)
        return -1;

    crypto_generichash_state hash;
    crypto_generichash_init(&hash, NULL, 0, PAYLOAD_DIGEST_BYTES);

    const int rc = decode_payload(container, hash_sink, &hash);
    if (rc == 0)
        crypto_generichash_final(&hash, digest, PAYLOAD_DIGEST_BYTES);

    sodium_memzero(&hash, sizeof(hash));
    container_free(container);
    return rc;
}
