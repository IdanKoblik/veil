#include "scatter.h"

#include <stdint.h>
#include <stdlib.h>
#include <veil/log.h>

#define SCATTER_MIN_SWAPS 64

static size_t swap_home(const struct Scatter *scatter, size_t index) {
    return (size_t)(((uint64_t)index * 0x9E3779B97F4A7C15ull) >> 32) & (scatter->swaps_size - 1);
}

static struct ScatterEntry *swap_find(const struct Scatter *scatter, size_t index) {
    // Keys are stored off by one, so a zeroed entry is an empty one.
    for (size_t i = swap_home(scatter, index);; i = (i + 1) & (scatter->swaps_size - 1)) {
        struct ScatterEntry *entry = &scatter->swaps[i];
        if (entry->key == 0 || entry->key == index + 1)
            return entry;
    }
}

static size_t swap_get(const struct Scatter *scatter, size_t index) {
    const struct ScatterEntry *entry = swap_find(scatter, index);
    return entry->key ? entry->value : index;
}

static int swaps_rebuild(struct Scatter *scatter) {
    // Everything below pos has been handed out and is never looked at again, so it is dropped here.
    size_t live = 0;
    for (size_t i = 0; i < scatter->swaps_size; i++)
        live += scatter->swaps[i].key > scatter->pos;

    size_t size = SCATTER_MIN_SWAPS;
    while (size < (live + 1) * 4)
        size *= 2;

    struct ScatterEntry *swaps = calloc(size, sizeof(*swaps));
    if (!swaps) {
        ERROR("Failed to grow the scatter table to %zu entries", size);
        return -1;
    }

    struct Scatter grown = *scatter;
    grown.swaps = swaps;
    grown.swaps_size = size;
    grown.swaps_used = live;

    for (size_t i = 0; i < scatter->swaps_size; i++) {
        const struct ScatterEntry *entry = &scatter->swaps[i];
        if (entry->key > scatter->pos)
            *swap_find(&grown, entry->key - 1) = *entry;
    }

    free(scatter->swaps);
    scatter->swaps = swaps;
    scatter->swaps_size = size;
    scatter->swaps_used = live;
    return 0;
}

static int swap_set(struct Scatter *scatter, size_t index, size_t value) {
    if ((scatter->swaps_used + 1) * 2 > scatter->swaps_size && swaps_rebuild(scatter) < 0)
        return -1;

    struct ScatterEntry *entry = swap_find(scatter, index);
    if (entry->key == 0) {
        entry->key = index + 1;
        scatter->swaps_used++;
    }

    entry->value = value;
    return 0;
}

int scatter_init(struct Scatter *scatter, size_t capacity, int encrypted, const unsigned char *key, const unsigned char *nonce) {
    scatter->capacity = capacity;
    scatter->pos = 0;
    scatter->encrypted = encrypted;
    scatter->swaps = NULL;
    scatter->swaps_size = 0;
    scatter->swaps_used = 0;

    if (!encrypted)
        return 0;

    // A video runs to billions of slots, far too many for a table with one entry per slot.
    // Only the swapped entries are kept, which walks the exact same Fisher-Yates order.
    scatter->swaps = calloc(SCATTER_MIN_SWAPS, sizeof(*scatter->swaps));
    if (!scatter->swaps) {
        ERROR("Failed to allocate the scatter table");
        return -1;
    }

    scatter->swaps_size = SCATTER_MIN_SWAPS;
    prng_init(&scatter->prng, key, nonce);

    return 0;
}

size_t scatter_next(struct Scatter *scatter) {
    if (scatter->pos >= scatter->capacity)
        return SIZE_MAX;

    if (!scatter->encrypted)
        return scatter->pos++;

    const size_t remaining = scatter->capacity - scatter->pos;
    const size_t index = scatter->pos + prng_uniform(&scatter->prng, remaining);

    const size_t slot = swap_get(scatter, index);
    if (index != scatter->pos && swap_set(scatter, index, swap_get(scatter, scatter->pos)) < 0)
        return SIZE_MAX;

    scatter->pos++;
    return slot;
}

void scatter_clear(struct Scatter *scatter) {
    if (scatter->encrypted)
        prng_clear(&scatter->prng);

    free(scatter->swaps);

    scatter->swaps = NULL;
    scatter->swaps_size = 0;
    scatter->swaps_used = 0;
    scatter->capacity = 0;
    scatter->pos = 0;
    scatter->encrypted = 0;
}
