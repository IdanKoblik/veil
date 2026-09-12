#include "greatest.h"

#include <string.h>

#include "veil/crypto/prng.h"

static void seed(struct Prng *prng, unsigned char key_byte, unsigned char nonce_byte) {
    unsigned char key[PRNG_KEYBYTES];
    unsigned char nonce[PRNG_NONCEBYTES];

    memset(key, key_byte, sizeof(key));
    memset(nonce, nonce_byte, sizeof(nonce));

    prng_init(prng, key, nonce);
}

TEST prng_repeats_for_the_same_key(void) {
    struct Prng first;
    struct Prng again;

    seed(&first, 0x11, 0x22);
    seed(&again, 0x11, 0x22);

    for (int i = 0; i < 64; i++)
        ASSERT_EQ(prng_next(&first), prng_next(&again));

    prng_clear(&first);
    prng_clear(&again);
    PASS();
}

TEST prng_parts_ways_on_a_different_key_or_nonce(void) {
    struct Prng base;
    struct Prng other_key;
    struct Prng other_nonce;

    seed(&base, 0x11, 0x22);
    seed(&other_key, 0x12, 0x22);
    seed(&other_nonce, 0x11, 0x23);

    int key_differs = 0;
    int nonce_differs = 0;

    for (int i = 0; i < 32; i++) {
        const uint64_t from_base = prng_next(&base);

        key_differs |= prng_next(&other_key) != from_base;
        nonce_differs |= prng_next(&other_nonce) != from_base;
    }

    ASSERT(key_differs);
    ASSERT(nonce_differs);

    prng_clear(&base);
    prng_clear(&other_key);
    prng_clear(&other_nonce);
    PASS();
}

TEST prng_uniform_stays_under_the_bound(void) {
    struct Prng prng;
    seed(&prng, 0x33, 0x44);

    for (int i = 0; i < 512; i++)
        ASSERT(prng_uniform(&prng, 10) < 10);

    ASSERT_EQ((uint64_t)0, prng_uniform(&prng, 1));

    prng_clear(&prng);
    PASS();
}

TEST prng_uniform_spreads_over_the_range(void) {
    struct Prng prng;
    seed(&prng, 0x55, 0x66);

    int seen[10] = {0};
    for (int i = 0; i < 2000; i++)
        seen[prng_uniform(&prng, 10)]++;

    for (int slot = 0; slot < 10; slot++)
        ASSERT(seen[slot] > 0);

    prng_clear(&prng);
    PASS();
}

TEST prng_clear_wipes_the_key(void) {
    struct Prng prng;
    seed(&prng, 0x77, 0x88);
    prng_next(&prng);

    prng_clear(&prng);

    unsigned char empty[PRNG_KEYBYTES] = {0};
    ASSERT_EQ(0, memcmp(prng.key, empty, sizeof(empty)));
    PASS();
}

SUITE(prng_suite) {
    RUN_TEST(prng_repeats_for_the_same_key);
    RUN_TEST(prng_parts_ways_on_a_different_key_or_nonce);
    RUN_TEST(prng_uniform_stays_under_the_bound);
    RUN_TEST(prng_uniform_spreads_over_the_range);
    RUN_TEST(prng_clear_wipes_the_key);
}
