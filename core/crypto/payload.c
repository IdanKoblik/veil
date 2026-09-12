#include "payload.h"
#include <veil/log.h>

#include <sodium/utils.h>
#include <stdlib.h>

struct Payload *encrypt_data(const unsigned char *data, size_t data_len, const unsigned char nonce[NONCE_LEN], const unsigned char key[KEY_LEN]) {
    if (!data) {
        ERROR("Target data was not found");
        return NULL;
    }

    if (data_len > SIZE_MAX - crypto_secretbox_MACBYTES) {
        ERROR("Data length is too large");
        return NULL;
    }

    size_t body_len = data_len + crypto_secretbox_MACBYTES;
    unsigned char *body = malloc(body_len);
    if (!body) {
        ERROR("Failed to allocate the body buffer");
        return NULL;
    }

    if (crypto_secretbox_easy(body, data, data_len, nonce, key) != 0) {
        ERROR("Encryption failed");
        free(body);
        return NULL;
    }

    struct Payload *p = malloc(sizeof(*p));
    if (!p) {
        ERROR("Failed to allocate the payload");
        free(body);
        return NULL;
    }

    p->body = body;
    p->body_len = body_len;

    return p;
}

int payload_free(struct Payload *payload) {
    if (!payload) {
        ERROR("Payload was not found");
        return -1;
    }

    if (payload->body) {
        sodium_memzero(payload->body, payload->body_len);
        free(payload->body);
    }

    free(payload);
    return 0;
}
