#include "greatest.h"
#include <sodium.h>

SUITE_EXTERN(checksum_suite);
SUITE_EXTERN(byte_suite);
SUITE_EXTERN(prng_suite);
SUITE_EXTERN(scatter_suite);
SUITE_EXTERN(carrier_suite);
SUITE_EXTERN(container_suite);
SUITE_EXTERN(stego_suite);
SUITE_EXTERN(file_suite);
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialise libsodium\n");
        return 1;
    }

    GREATEST_MAIN_BEGIN();
    RUN_SUITE(checksum_suite);
    RUN_SUITE(byte_suite);
    RUN_SUITE(prng_suite);
    RUN_SUITE(scatter_suite);
    RUN_SUITE(file_suite);
    RUN_SUITE(carrier_suite);
    RUN_SUITE(container_suite);
    RUN_SUITE(stego_suite);
    GREATEST_MAIN_END();
}
