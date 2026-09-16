#include "greatest.h"
#include <sodium.h>

SUITE_EXTERN(checksum_suite);
SUITE_EXTERN(byte_suite);
SUITE_EXTERN(codec_suite);
SUITE_EXTERN(prng_suite);
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialise libsodium\n");
        return 1;
    }

    GREATEST_MAIN_BEGIN();
    RUN_SUITE(checksum_suite);
    RUN_SUITE(byte_suite);
    RUN_SUITE(codec_suite);
    RUN_SUITE(prng_suite);
    GREATEST_MAIN_END();
}
