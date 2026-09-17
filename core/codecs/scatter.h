#pragma once

#include "../crypto/prng.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ScatterEntry {
    size_t key;
    size_t value;
};

struct Scatter {
    struct Prng prng;

    struct ScatterEntry *swaps;
    size_t swaps_size;
    size_t swaps_used;

    size_t capacity;
    size_t pos;

    int encrypted;
};

int scatter_init(struct Scatter *scatter, size_t capacity, int encrypted, const unsigned char *key, const unsigned char *nonce);

size_t scatter_next(struct Scatter *scatter);
void scatter_clear(struct Scatter *scatter);

#ifdef __cplusplus
}
#endif
