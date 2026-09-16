#include "scatter.h"

#include <stdlib.h>
#include <veil/log.h>

int scatter_init(
    struct Scatter *scatter,
    size_t capacity,
    int encrypted,
    const unsigned char *key,
    const unsigned char *nonce
) {
    scatter->capacity = capacity;
    scatter->pos = 0;
    scatter->encrypted = encrypted;
    scatter->slots = NULL;

    if (!encrypted)
        return 0;

    scatter->slots = malloc(capacity * sizeof(*scatter->slots));
    if (!scatter->slots) {
        ERROR("Failed to allocate %zu scatter slots", capacity);
        return -1;
    }

    for (size_t i = 0; i < capacity; i++)
        scatter->slots[i] = i;

    prng_init(
        &scatter->prng,
        key,
        nonce
    );

    return 0;
}

size_t scatter_next(struct Scatter *scatter) {
    if (!scatter->encrypted) {
        if (scatter->pos >= scatter->capacity)
            return SIZE_MAX;

        return scatter->pos++;
    }

    if (scatter->pos >= scatter->capacity)
        return SIZE_MAX;

    size_t remaining = scatter->capacity - scatter->pos;
    size_t offset = prng_uniform(
            &scatter->prng,
            remaining
    );

    size_t index = scatter->pos + offset;
    size_t slot = scatter->slots[index];
    scatter->slots[index] = scatter->slots[scatter->pos];
    scatter->slots[scatter->pos] = slot;

    scatter->pos++;
    return slot;
}

void scatter_clear(struct Scatter *scatter) {
    if (scatter->encrypted)
        prng_clear(&scatter->prng);

    free(scatter->slots);

    scatter->slots = NULL;
    scatter->capacity = 0;
    scatter->pos = 0;
    scatter->encrypted = 0;
}