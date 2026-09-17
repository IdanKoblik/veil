#include "greatest.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "veil/codecs/scatter.h"

#define CAPACITY 257

static void keyed(struct Scatter *scatter, unsigned char key_byte) {
    unsigned char key[PRNG_KEYBYTES];
    unsigned char nonce[PRNG_NONCEBYTES];

    memset(key, key_byte, sizeof(key));
    memset(nonce, 0x42, sizeof(nonce));

    scatter_init(scatter, CAPACITY, 1, key, nonce);
}

TEST scatter_walks_in_order_in_the_clear(void) {
    struct Scatter scatter;
    ASSERT_EQ(0, scatter_init(&scatter, CAPACITY, 0, NULL, NULL));

    for (size_t i = 0; i < CAPACITY; i++)
        ASSERT_EQ(i, scatter_next(&scatter));

    ASSERT_EQ(SIZE_MAX, scatter_next(&scatter));

    scatter_clear(&scatter);
    PASS();
}

TEST scatter_visits_every_slot_once_when_encrypted(void) {
    struct Scatter scatter;
    keyed(&scatter, 0x11);

    unsigned char seen[CAPACITY] = {0};
    size_t in_order = 0;

    for (size_t i = 0; i < CAPACITY; i++) {
        const size_t slot = scatter_next(&scatter);
        ASSERT(slot < CAPACITY);
        ASSERT_EQ(0, seen[slot]);

        seen[slot] = 1;
        in_order += slot == i;
    }

    ASSERT_EQ(SIZE_MAX, scatter_next(&scatter));
    ASSERT(in_order < CAPACITY / 4);

    scatter_clear(&scatter);
    PASS();
}

TEST scatter_repeats_for_the_same_key(void) {
    struct Scatter first;
    struct Scatter again;
    keyed(&first, 0x11);
    keyed(&again, 0x11);

    for (size_t i = 0; i < CAPACITY; i++)
        ASSERT_EQ(scatter_next(&first), scatter_next(&again));

    scatter_clear(&first);
    scatter_clear(&again);
    PASS();
}

TEST scatter_parts_ways_on_a_different_key(void) {
    struct Scatter first;
    struct Scatter other;
    keyed(&first, 0x11);
    keyed(&other, 0x12);

    size_t same = 0;
    for (size_t i = 0; i < CAPACITY; i++)
        same += scatter_next(&first) == scatter_next(&other);

    ASSERT(same < CAPACITY / 4);

    scatter_clear(&first);
    scatter_clear(&other);
    PASS();
}

TEST scatter_matches_a_full_table_shuffle(void) {
    // Files already out there were scattered with a table of every slot, the order must not move.
    enum { SLOTS = 5000 };
    static size_t table[SLOTS];
    for (size_t i = 0; i < SLOTS; i++)
        table[i] = i;

    unsigned char key[PRNG_KEYBYTES];
    unsigned char nonce[PRNG_NONCEBYTES];
    memset(key, 0x23, sizeof(key));
    memset(nonce, 0x42, sizeof(nonce));

    struct Prng prng;
    prng_init(&prng, key, nonce);

    struct Scatter scatter;
    ASSERT_EQ(0, scatter_init(&scatter, SLOTS, 1, key, nonce));

    for (size_t pos = 0; pos < SLOTS; pos++) {
        const size_t index = pos + prng_uniform(&prng, SLOTS - pos);
        const size_t slot = table[index];
        table[index] = table[pos];
        table[pos] = slot;

        ASSERT_EQ(slot, scatter_next(&scatter));
    }

    ASSERT_EQ(SIZE_MAX, scatter_next(&scatter));

    prng_clear(&prng);
    scatter_clear(&scatter);
    PASS();
}

TEST scatter_handles_a_carrier_too_big_for_a_table(void) {
    struct Scatter scatter;
    unsigned char key[PRNG_KEYBYTES] = {0};
    unsigned char nonce[PRNG_NONCEBYTES] = {0};

    const size_t capacity = SIZE_MAX / 2;
    ASSERT_EQ(0, scatter_init(&scatter, capacity, 1, key, nonce));

    for (size_t i = 0; i < 10000; i++)
        ASSERT(scatter_next(&scatter) < capacity);

    scatter_clear(&scatter);
    PASS();
}

TEST scatter_is_empty_after_clear(void) {
    struct Scatter scatter;
    keyed(&scatter, 0x11);
    scatter_clear(&scatter);

    ASSERT_EQ(SIZE_MAX, scatter_next(&scatter));
    PASS();
}

SUITE(scatter_suite) {
    RUN_TEST(scatter_walks_in_order_in_the_clear);
    RUN_TEST(scatter_visits_every_slot_once_when_encrypted);
    RUN_TEST(scatter_repeats_for_the_same_key);
    RUN_TEST(scatter_parts_ways_on_a_different_key);
    RUN_TEST(scatter_matches_a_full_table_shuffle);
    RUN_TEST(scatter_handles_a_carrier_too_big_for_a_table);
    RUN_TEST(scatter_is_empty_after_clear);
}
