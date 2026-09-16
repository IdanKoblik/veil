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

TEST scatter_refuses_a_table_that_overflows(void) {
    struct Scatter scatter;
    unsigned char key[PRNG_KEYBYTES] = {0};
    unsigned char nonce[PRNG_NONCEBYTES] = {0};

    ASSERT(scatter_init(&scatter, SIZE_MAX / 2, 1, key, nonce) < 0);
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
    RUN_TEST(scatter_refuses_a_table_that_overflows);
    RUN_TEST(scatter_is_empty_after_clear);
}
