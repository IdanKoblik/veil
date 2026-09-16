#pragma once

#include <stddef.h>
#include "../crypto/prng.h"

struct Scatter {
    struct Prng prng;

    size_t *slots;
    size_t capacity;
    size_t pos;

    int encrypted;
};

int scatter_init(
    struct Scatter *scatter,
    size_t capacity,
    int encrypted,
    const unsigned char *key,
    const unsigned char *nonce
);

size_t scatter_next(struct Scatter *scatter);
void scatter_clear(struct Scatter *scatter);